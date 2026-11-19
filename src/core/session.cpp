#include "session.h"
#include "db.h"

#include <QEvent>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QtGlobal>

Session &Session::instance()
{
    static Session s;
    return s;
}

Session::Session(QObject *parent)
    : QObject(parent)
{
    m_idleTimer.setSingleShot(true);
    m_warningTimer.setInterval(1000);

    connect(&m_idleTimer, &QTimer::timeout, this, &Session::onIdleTimeout);
    connect(&m_warningTimer, &QTimer::timeout, this, &Session::onWarningTick);
}

bool Session::login(const QString &email, const QString &password, QString *errorMessage)
{
    const QString cleanEmail = email.trimmed().toLower();
    if (cleanEmail.isEmpty() || password.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Debes ingresar tu correo electrónico y contraseña.");
        }
        return false;
    }

    if (!Database::instance().isOpen()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("La base de datos no está disponible.");
        }
        return false;
    }

    QSqlQuery query(Database::instance().connection());
    query.prepare(QStringLiteral(
        "SELECT u.id, u.email, u.full_name, u.role_id, r.name, r.is_system, "
        "       u.password_hash, u.is_active, u.must_change_password "
        "FROM users u "
        "INNER JOIN roles r ON r.id = u.role_id "
        "WHERE u.email = :email LIMIT 1;"));
    query.bindValue(QStringLiteral(":email"), cleanEmail);

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Error consultando credenciales: %1")
                                .arg(query.lastError().text());
        }
        return false;
    }

    if (!query.next()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Correo o contraseña incorrectos.");
        }
        return false;
    }

    if (query.value(7).toInt() != 1) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Esta cuenta de usuario se encuentra bloqueada.");
        }
        return false;
    }

    const QString storedHash = query.value(6).toString();
    if (!Database::verifyPassword(password, storedHash)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Correo o contraseña incorrectos.");
        }
        return false;
    }

    m_user = UserSessionInfo{};
    m_user.id = query.value(0).toInt();
    m_user.email = query.value(1).toString();
    m_user.fullName = query.value(2).toString();
    m_user.roleId = query.value(3).toInt();
    m_user.roleName = query.value(4).toString();
    m_user.isAdmin = (query.value(5).toInt() == 1);
    m_user.mustChangePassword = (query.value(8).toInt() == 1);

    loadUserPermissions(m_user.id, m_user.roleId);
    reloadInactivityFromDatabase();

    m_authenticated = true;
    startInactivityWatch();

    Database::instance().logAudit(
        m_user.email, QStringLiteral("session"), QStringLiteral("login"),
        QStringLiteral("Inicio de sesión exitoso"));

    emit sessionStarted();
    return true;
}

void Session::logout(bool expiredByInactivity)
{
    if (!m_authenticated) {
        return;
    }

    Database::instance().logAudit(
        m_user.email, QStringLiteral("session"),
        expiredByInactivity ? QStringLiteral("timeout_logout") : QStringLiteral("logout"),
        expiredByInactivity ? QStringLiteral("Sesión caducada por inactividad")
                            : QStringLiteral("Cierre de sesión manual"));

    stopInactivityWatch();
    m_authenticated = false;
    m_user = UserSessionInfo{};

    emit sessionEnded(expiredByInactivity);
}

bool Session::isAuthenticated() const
{
    return m_authenticated;
}

const UserSessionInfo &Session::currentUser() const
{
    return m_user;
}

bool Session::mustChangePassword() const
{
    return m_authenticated && m_user.mustChangePassword;
}

void Session::markPasswordChanged()
{
    if (!m_authenticated) {
        return;
    }
    m_user.mustChangePassword = false;
    emit passwordPolicyResolved();
}

bool Session::hasPermission(const QString &moduleId, const QString &action) const
{
    if (!m_authenticated) {
        return false;
    }
    if (m_user.isAdmin) {
        return true;
    }

    const QString exactKey = moduleId + QLatin1Char(':') + action;
    const QString wildcardModule = moduleId + QStringLiteral(":*");
    return m_user.permissions.contains(exactKey) || m_user.permissions.contains(wildcardModule);
}

void Session::configureInactivityTimeouts(int idleSeconds, int warningSeconds)
{
    m_idleSeconds = qMax(10, idleSeconds);
    m_warningSeconds = qMax(5, warningSeconds);
    if (m_authenticated && !m_warningActive) {
        m_idleTimer.start(m_idleSeconds * 1000);
    }
}

void Session::reloadInactivityFromDatabase()
{
    bool okIdle = false;
    bool okWarn = false;
    const int idleMin = Database::instance()
                            .getSetting(QStringLiteral("security.idle_minutes"), QStringLiteral("15"))
                            .toInt(&okIdle);
    const int warnSec = Database::instance()
                            .getSetting(QStringLiteral("security.warning_seconds"), QStringLiteral("30"))
                            .toInt(&okWarn);

    configureInactivityTimeouts((okIdle && idleMin > 0 ? idleMin : 15) * 60,
                                (okWarn && warnSec >= 5 ? warnSec : 30));
}

void Session::keepAlive()
{
    if (!m_authenticated) {
        return;
    }
    m_warningActive = false;
    m_warningTimer.stop();
    m_idleTimer.start(m_idleSeconds * 1000);
}

void Session::loadUserPermissions(int userId, int roleId)
{
    m_user.permissions.clear();

    // 1. Permisos heredados del rol
    QSqlQuery roleQuery(Database::instance().connection());
    roleQuery.prepare(QStringLiteral(
        "SELECT p.module_id, p.action "
        "FROM role_permissions rp "
        "INNER JOIN permissions p ON p.id = rp.permission_id "
        "WHERE rp.role_id = :role_id;"));
    roleQuery.bindValue(QStringLiteral(":role_id"), roleId);
    if (roleQuery.exec()) {
        while (roleQuery.next()) {
            const QString key = roleQuery.value(0).toString() + QLatin1Char(':')
                                + roleQuery.value(1).toString();
            m_user.permissions.insert(key);
        }
    }

    // 2. Excepciones específicas del usuario (granted = 1 agrega, granted = 0 revoca)
    QSqlQuery userQuery(Database::instance().connection());
    userQuery.prepare(QStringLiteral(
        "SELECT p.module_id, p.action, up.granted "
        "FROM user_permissions up "
        "INNER JOIN permissions p ON p.id = up.permission_id "
        "WHERE up.user_id = :user_id;"));
    userQuery.bindValue(QStringLiteral(":user_id"), userId);
    if (userQuery.exec()) {
        while (userQuery.next()) {
            const QString key = userQuery.value(0).toString() + QLatin1Char(':')
                                + userQuery.value(1).toString();
            if (userQuery.value(2).toInt() == 1) {
                m_user.permissions.insert(key);
            } else {
                m_user.permissions.remove(key);
            }
        }
    }
}

void Session::startInactivityWatch()
{
    m_warningActive = false;
    m_warningTimer.stop();
    m_idleTimer.start(m_idleSeconds * 1000);
}

void Session::stopInactivityWatch()
{
    m_warningActive = false;
    m_idleTimer.stop();
    m_warningTimer.stop();
}

bool Session::eventFilter(QObject *watched, QEvent *event)
{
    if (m_authenticated && !m_warningActive) {
        switch (event->type()) {
        case QEvent::MouseMove:
        case QEvent::MouseButtonPress:
        case QEvent::KeyPress:
        case QEvent::Wheel:
        case QEvent::TouchBegin:
            m_idleTimer.start(m_idleSeconds * 1000);
            break;
        default:
            break;
        }
    }
    return QObject::eventFilter(watched, event);
}

void Session::onIdleTimeout()
{
    if (!m_authenticated) {
        return;
    }
    m_warningActive = true;
    m_remainingSeconds = m_warningSeconds;
    emit inactivityWarningStarted(m_remainingSeconds);
    m_warningTimer.start();
}

void Session::onWarningTick()
{
    if (!m_authenticated || !m_warningActive) {
        m_warningTimer.stop();
        return;
    }

    --m_remainingSeconds;
    if (m_remainingSeconds <= 0) {
        logout(true);
        return;
    }

    emit inactivityCountdownTick(m_remainingSeconds);
}
