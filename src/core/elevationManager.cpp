#include "elevationManager.h"
#include "db.h"
#include "mailer.h"
#include "session.h"

#include <QDateTime>
#include <QHostInfo>
#include <QRandomGenerator>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>
#include <QVariant>

ElevationManager &ElevationManager::instance()
{
    static ElevationManager mgr;
    return mgr;
}

ElevationManager::ElevationManager(QObject *parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &ElevationManager::onTimerTick);
}

QString ElevationManager::scopeDisplayName(const QString &scope)
{
    if (scope == QStringLiteral("support_tables")) {
        return QStringLiteral("Tablas Soporte");
    }
    if (scope == QStringLiteral("backup_audit")) {
        return QStringLiteral("Respaldo y Auditoría");
    }
    if (scope == QStringLiteral("db_admin")) {
        return QStringLiteral("Base de Datos / SQL");
    }
    return scope;
}

ElevationPolicy ElevationManager::loadPolicy()
{
    const Database &db = Database::instance();
    ElevationPolicy p;
    p.durationMinutes    = qBound(5, db.getSetting(QStringLiteral("security.elevation_duration_min"), QStringLiteral("20")).toInt(), 120);
    p.allowSupportTables = (db.getSetting(QStringLiteral("security.elevation_allow_support"), QStringLiteral("1")) == QStringLiteral("1"));
    p.allowBackupAudit   = (db.getSetting(QStringLiteral("security.elevation_allow_backup"), QStringLiteral("1")) == QStringLiteral("1"));
    p.allowDbAdmin       = (db.getSetting(QStringLiteral("security.elevation_allow_db_admin"), QStringLiteral("1")) == QStringLiteral("1"));
    p.notifyByEmail      = (db.getSetting(QStringLiteral("security.elevation_notify_email"), QStringLiteral("0")) == QStringLiteral("1"));
    return p;
}

bool ElevationManager::savePolicy(const ElevationPolicy &policy, QString *errorMessage)
{
    Database &db = Database::instance();
    const bool ok =
        db.setSetting(QStringLiteral("security.elevation_duration_min"),
                      QString::number(qBound(5, policy.durationMinutes, 120)), errorMessage) &&
        db.setSetting(QStringLiteral("security.elevation_allow_support"),
                      policy.allowSupportTables ? QStringLiteral("1") : QStringLiteral("0"), errorMessage) &&
        db.setSetting(QStringLiteral("security.elevation_allow_backup"),
                      policy.allowBackupAudit ? QStringLiteral("1") : QStringLiteral("0"), errorMessage) &&
        db.setSetting(QStringLiteral("security.elevation_allow_db_admin"),
                      policy.allowDbAdmin ? QStringLiteral("1") : QStringLiteral("0"), errorMessage) &&
        db.setSetting(QStringLiteral("security.elevation_notify_email"),
                      policy.notifyByEmail ? QStringLiteral("1") : QStringLiteral("0"), errorMessage);
    return ok;
}

bool ElevationManager::isScopeAllowedByPolicy(const QString &scope) const
{
    const ElevationPolicy p = loadPolicy();
    if (scope == QStringLiteral("support_tables")) return p.allowSupportTables;
    if (scope == QStringLiteral("backup_audit"))   return p.allowBackupAudit;
    if (scope == QStringLiteral("db_admin"))       return p.allowDbAdmin;
    return false;
}

bool ElevationManager::canCurrentUserRequestElevation(const QString &scope, QString *denialReasonOut) const
{
    const Session &session = Session::instance();
    if (!session.isAuthenticated()) {
        if (denialReasonOut) {
            *denialReasonOut = QStringLiteral("No existe una sesión activa.");
        }
        return false;
    }

    if (session.currentUser().isAdmin) {
        return true;
    }

    if (!session.hasPermission(QStringLiteral("panel_control"), QStringLiteral("request_elevation"))) {
        if (denialReasonOut) {
            *denialReasonOut = QStringLiteral(
                "Usted no posee permisos para solicitar tokens elevados.\n\n"
                "La sección \"%1\" está reservada para el Administrador o personal con nivel de Supervisor autorizado.")
                                   .arg(scopeDisplayName(scope));
        }
        return false;
    }

    if (!isScopeAllowedByPolicy(scope)) {
        if (denialReasonOut) {
            *denialReasonOut = QStringLiteral(
                "La política de seguridad actual del Administrador no permite solicitar "
                "elevación temporal para la sección \"%1\".")
                                   .arg(scopeDisplayName(scope));
        }
        return false;
    }

    return true;
}

bool ElevationManager::isElevationActive() const
{
    if (!m_active.active) {
        return false;
    }
    if (!Session::instance().isAuthenticated() ||
        Session::instance().currentUser().id != m_active.userId) {
        return false;
    }
    return QDateTime::currentSecsSinceEpoch() < m_active.expiresAtEpoch;
}

bool ElevationManager::hasActiveElevationFor(const QString &scope) const
{
    return isElevationActive() && (m_active.scope == scope);
}

const ActiveElevation &ElevationManager::activeElevation() const
{
    return m_active;
}

int ElevationManager::remainingSeconds() const
{
    if (!isElevationActive()) {
        return 0;
    }
    return static_cast<int>(qMax<qint64>(0, m_active.expiresAtEpoch - QDateTime::currentSecsSinceEpoch()));
}

QString ElevationManager::activeAuthContext() const
{
    if (!isElevationActive()) {
        return QStringLiteral("NORMAL");
    }
    return QStringLiteral("TOKEN_ELEVATION:%1:%2")
        .arg(m_active.tokenCode, m_active.reason.left(120));
}

QString ElevationManager::generateTokenCode()
{
    const QString chars = QStringLiteral("ABCDEFGHJKLMNPQRSTUVWXYZ23456789");
    auto block = [&chars](int n) {
        QString out;
        out.reserve(n);
        for (int i = 0; i < n; ++i) {
            const int idx = static_cast<int>(QRandomGenerator::system()->bounded(static_cast<quint32>(chars.size())));
            out.append(chars.at(idx));
        }
        return out;
    };
    return QStringLiteral("ELV-%1-%2").arg(block(4), block(4));
}

bool ElevationManager::requestSelfElevation(const QString &scope,
                                            const QString &userPassword,
                                            const QString &reason,
                                            QString *errorMessage)
{
    QString denialReason;
    if (!canCurrentUserRequestElevation(scope, &denialReason)) {
        if (errorMessage) *errorMessage = denialReason;
        return false;
    }

    const QString cleanReason = reason.trimmed();
    if (cleanReason.size() < 5) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Debes indicar un motivo breve (mínimo 5 caracteres) para registrar en bitácora.");
        }
        return false;
    }

    if (userPassword.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Ingresa tu contraseña actual para confirmar tu identidad.");
        }
        return false;
    }

    const UserSessionInfo &user = Session::instance().currentUser();

    // Verificar contraseña real del usuario activo (revalidación estilo sudo)
    QSqlQuery passQ(Database::instance().connection());
    passQ.prepare(QStringLiteral("SELECT password_hash FROM users WHERE id = :id AND is_active = 1 LIMIT 1;"));
    passQ.bindValue(QStringLiteral(":id"), user.id);

    if (!passQ.exec() || !passQ.next() || !Database::verifyPassword(userPassword, passQ.value(0).toString())) {
        AuditEvent failEv;
        failEv.moduleId = QStringLiteral("panel_control");
        failEv.targetTable = QStringLiteral("elevation_tokens");
        failEv.action = QStringLiteral("TOKEN_ELEVATION_FAIL");
        failEv.authContext = QStringLiteral("NORMAL");
        failEv.details = QStringLiteral("Contraseña incorrecta solicitando elevación para %1 | Motivo: %2")
                             .arg(scopeDisplayName(scope), cleanReason);
        Database::instance().logAuditEvent(failEv);

        if (errorMessage) {
            *errorMessage = QStringLiteral("La contraseña ingresada es incorrecta. El intento fallido quedó registrado en bitácora.");
        }
        return false;
    }

    // Si ya había un token previo activo, revocarlo limpiamente antes de emitir el nuevo
    if (m_active.active) {
        revokeActiveElevation(false);
    }

    const ElevationPolicy policy = loadPolicy();
    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    const QDateTime expUtc = nowUtc.addSecs(policy.durationMinutes * 60);
    const QString tokenCode = generateTokenCode();

    QSqlQuery insQ(Database::instance().connection());
    insQ.prepare(QStringLiteral(
        "INSERT INTO elevation_tokens (token_code, user_id, user_email, target_scope, reason, expires_at, revoked_at) "
        "VALUES (:code, :uid, :email, :scope, :reason, :exp, '');"));
    insQ.bindValue(QStringLiteral(":code"), tokenCode);
    insQ.bindValue(QStringLiteral(":uid"), user.id);
    insQ.bindValue(QStringLiteral(":email"), user.email);
    insQ.bindValue(QStringLiteral(":scope"), scope);
    insQ.bindValue(QStringLiteral(":reason"), cleanReason);
    insQ.bindValue(QStringLiteral(":exp"), expUtc.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));

    if (!insQ.exec()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No se pudo emitir el token de elevación:\n%1")
                                .arg(insQ.lastError().text());
        }
        return false;
    }

    m_active.active           = true;
    m_active.tokenCode        = tokenCode;
    m_active.scope            = scope;
    m_active.scopeDisplayName = scopeDisplayName(scope);
    m_active.reason           = cleanReason;
    m_active.userId           = user.id;
    m_active.userEmail        = user.email;
    m_active.userFullName     = user.fullName;
    m_active.issuedAtEpoch    = nowUtc.toSecsSinceEpoch();
    m_active.expiresAtEpoch   = expUtc.toSecsSinceEpoch();

    AuditEvent okEv;
    okEv.moduleId = QStringLiteral("panel_control");
    okEv.targetTable = QStringLiteral("elevation_tokens");
    okEv.recordId = tokenCode;
    okEv.action = QStringLiteral("TOKEN_ELEVATION_OK");
    okEv.authContext = activeAuthContext();
    okEv.details = QStringLiteral("%1 (%2) ingresó con token %3 a %4 por %5 min | Motivo: %6")
                       .arg(user.fullName, user.roleName, tokenCode, m_active.scopeDisplayName)
                       .arg(policy.durationMinutes)
                       .arg(cleanReason);
    Database::instance().logAuditEvent(okEv);

    m_timer->start();
    emit elevationChanged(true);
    emit elevationTick(remainingSeconds());

    if (policy.notifyByEmail) {
        sendAuditNotificationEmailAsync(m_active);
    }

    return true;
}

void ElevationManager::revokeActiveElevation(bool expired)
{
    if (!m_active.active) {
        return;
    }

    const ActiveElevation closing = m_active;
    m_timer->stop();
    m_active = ActiveElevation{};

    if (Database::instance().isOpen()) {
        QSqlQuery updQ(Database::instance().connection());
        updQ.prepare(QStringLiteral("UPDATE elevation_tokens SET revoked_at = :ts WHERE token_code = :code;"));
        updQ.bindValue(QStringLiteral(":ts"),
                       QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        updQ.bindValue(QStringLiteral(":code"), closing.tokenCode);
        updQ.exec();

        AuditEvent ev;
        ev.userId = closing.userId;
        ev.userEmail = closing.userEmail;
        ev.userFullName = closing.userFullName;
        ev.moduleId = QStringLiteral("panel_control");
        ev.targetTable = QStringLiteral("elevation_tokens");
        ev.recordId = closing.tokenCode;
        ev.action = expired ? QStringLiteral("TOKEN_ELEVATION_EXPIRED") : QStringLiteral("TOKEN_ELEVATION_REVOKED");
        ev.authContext = QStringLiteral("TOKEN_ELEVATION:%1").arg(closing.tokenCode);
        ev.details = expired
                         ? QStringLiteral("Expiró el token %1 para %2").arg(closing.tokenCode, closing.scopeDisplayName)
                         : QStringLiteral("Cierre anticipado del token %1 para %2").arg(closing.tokenCode, closing.scopeDisplayName);
        Database::instance().logAuditEvent(ev);
    }

    emit elevationChanged(false);
    if (expired) {
        emit elevationExpired(closing.scopeDisplayName);
    }
}

void ElevationManager::onTimerTick()
{
    if (!m_active.active) {
        m_timer->stop();
        return;
    }

    if (!Session::instance().isAuthenticated() ||
        Session::instance().currentUser().id != m_active.userId) {
        revokeActiveElevation(false);
        return;
    }

    const int left = remainingSeconds();
    if (left <= 0) {
        revokeActiveElevation(true);
        return;
    }

    emit elevationTick(left);
}

void ElevationManager::sendAuditNotificationEmailAsync(const ActiveElevation &ev)
{
    QTimer::singleShot(150, this, [ev]() {
        const SmtpConfig cfg = Mailer::loadConfig();
        if (!cfg.isComplete() || cfg.fromEmail.trimmed().isEmpty()) {
            return;
        }

        const QString subject = QStringLiteral("Global Manager [Auditoría] - Elevación temporal: %1")
                                    .arg(ev.scopeDisplayName);
        const QString body = QStringLiteral(
            "Aviso automático de seguridad de Global Manager:\n\n"
            "• Usuario     : %1 (%2)\n"
            "• Sección     : %3\n"
            "• Token       : %4\n"
            "• Puesto      : #%5 (%6)\n"
            "• Motivo      : %7\n"
            "• Emitido     : %8\n\n"
            "Todas las acciones realizadas durante esta ventana quedan registradas en la bitácora inmutable.")
                                 .arg(ev.userFullName,
                                      ev.userEmail,
                                      ev.scopeDisplayName,
                                      ev.tokenCode)
                                 .arg(Database::instance().activeConfig().terminalNumber)
                                 .arg(QHostInfo::localHostName(),
                                      ev.reason,
                                      QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yyyy HH:mm:ss")));

        Mailer::sendMail(cfg.fromEmail, subject, body);
    });
}
