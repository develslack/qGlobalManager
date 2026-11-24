#include "db.h"
#include "session.h"
#include "elevationManager.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostInfo>
#include <QPair>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>
#include <QSqlRecord>
#include <QTextStream>

namespace {
constexpr int kSaltBytes = 16;
constexpr int kHashIterations = 100000;
constexpr int kMaxAutoBackups = 10;
} // namespace

Database &Database::instance()
{
    static Database db;
    return db;
}

Database::~Database()
{
    close();
}

void Database::close()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        {
            QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
            if (db.isOpen()) {
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool Database::isSafeDatabaseIdentifier(const QString &name)
{
    static const QRegularExpression rx(QStringLiteral(R"(^[A-Za-z][A-Za-z0-9_]{0,62}$)"));
    return rx.match(name.trimmed()).hasMatch();
}

bool Database::initialize(QString *errorMessage)
{
    const BootstrapData cfg = BootstrapConfig::load();
    return initializeWithConfig(cfg, true, errorMessage);
}

bool Database::initializeWithConfig(const BootstrapData &config,
                                    bool createSchemaIfMissing,
                                    QString *errorMessage)
{
    close();
    m_activeConfig = config;

    if (config.isSqlite()) {
        if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("No se encontró el driver SQLite de Qt (QSQLITE).");
            }
            return false;
        }

        const QString dbPath = config.sqlitePath.trimmed().isEmpty()
                                   ? BootstrapConfig::defaultSqlitePath()
                                   : config.sqlitePath.trimmed();
        m_activeConfig.sqlitePath = dbPath;

        QFileInfo fi(dbPath);
        BootstrapConfig::hideDirectory(fi.absolutePath());

        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
        db.setDatabaseName(dbPath);
        if (!db.open()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("No se pudo abrir la base de datos SQLite:\n%1")
                                    .arg(db.lastError().text());
            }
            return false;
        }

        QFile::setPermissions(dbPath, QFile::ReadOwner | QFile::WriteOwner);

        QSqlQuery pragma(db);
        pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON;"));
        pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL;"));
    } else {
        if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QMYSQL"))) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                    "El driver QMYSQL de Qt (MariaDB/MySQL) no está disponible en este equipo.\n\n"
                    "Instala el paquete de soporte MariaDB/Qt SQL MySQL para operar en modo red.");
            }
            return false;
        }

        const QString cleanDbName = config.dbName.trimmed();
        if (!isSafeDatabaseIdentifier(cleanDbName)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("El nombre de la base de datos solo puede contener letras, números y guiones bajos.");
            }
            return false;
        }

        // Si corresponde crear la base en MariaDB (Puesto #1), conectar primero al servidor para CREATE DATABASE
        if (createSchemaIfMissing) {
            const QString tmpConn = QStringLiteral("qGlobalManager_bootstrap_mariadb");
            {
                QSqlDatabase bootstrapDb = QSqlDatabase::addDatabase(QStringLiteral("QMYSQL"), tmpConn);
                bootstrapDb.setHostName(config.dbHost.trimmed());
                bootstrapDb.setPort(config.dbPort > 0 ? config.dbPort : 3306);
                bootstrapDb.setUserName(config.dbUser.trimmed());
                bootstrapDb.setPassword(config.dbPassword);

                if (!bootstrapDb.open()) {
                    if (errorMessage) {
                        *errorMessage = QStringLiteral("No se pudo conectar al servidor MariaDB (%1:%2):\n%3")
                                            .arg(config.dbHost).arg(config.dbPort).arg(bootstrapDb.lastError().text());
                    }
                    bootstrapDb.close();
                    QSqlDatabase::removeDatabase(tmpConn);
                    return false;
                }

                QSqlQuery createDbQ(bootstrapDb);
                const QString createSql = QStringLiteral(
                    "CREATE DATABASE IF NOT EXISTS `%1` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;")
                                              .arg(cleanDbName);
                if (!createDbQ.exec(createSql)) {
                    if (errorMessage) {
                        *errorMessage = QStringLiteral("No se pudo crear o verificar la base '%1' en MariaDB:\n%2")
                                            .arg(cleanDbName, createDbQ.lastError().text());
                    }
                    bootstrapDb.close();
                    QSqlDatabase::removeDatabase(tmpConn);
                    return false;
                }
                bootstrapDb.close();
            }
            QSqlDatabase::removeDatabase(tmpConn);
        }

        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QMYSQL"), m_connectionName);
        db.setHostName(config.dbHost.trimmed());
        db.setPort(config.dbPort > 0 ? config.dbPort : 3306);
        db.setDatabaseName(cleanDbName);
        db.setUserName(config.dbUser.trimmed());
        db.setPassword(config.dbPassword);

        if (!db.open()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("No se pudo conectar a la base '%1' en MariaDB (%2:%3):\n%4")
                                    .arg(cleanDbName, config.dbHost).arg(config.dbPort).arg(db.lastError().text());
            }
            return false;
        }
    }

    if (createSchemaIfMissing) {
        if (!createSchema(errorMessage)) {
            return false;
        }
        if (isSqlite() && !ensureSqliteMigrations(errorMessage)) {
            return false;
        }
        if (!seedInitialData(errorMessage)) {
            return false;
        }
    }

    return true;
}

bool Database::testMariaDbConnection(const BootstrapData &config,
                                     bool *schemaAlreadyExistsOut,
                                     QString *errorMessage)
{
    if (schemaAlreadyExistsOut) {
        *schemaAlreadyExistsOut = false;
    }

    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QMYSQL"))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                "El driver QMYSQL (MariaDB/MySQL) de Qt no está instalado en este equipo.");
        }
        return false;
    }

    const QString cleanDbName = config.dbName.trimmed();
    if (!isSafeDatabaseIdentifier(cleanDbName)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Nombre de base de datos inválido (usa solo letras, números y '_').");
        }
        return false;
    }

    const QString tmpConn = QStringLiteral("qGlobalManager_test_mariadb");
    bool ok = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QMYSQL"), tmpConn);
        db.setHostName(config.dbHost.trimmed());
        db.setPort(config.dbPort > 0 ? config.dbPort : 3306);
        db.setUserName(config.dbUser.trimmed());
        db.setPassword(config.dbPassword);

        if (!db.open()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Error conectando al servidor MariaDB:\n%1")
                                    .arg(db.lastError().text());
            }
        } else {
            ok = true;
            // Verificar si ya existe la base y la tabla users dentro de ella
            QSqlQuery q(db);
            q.prepare(QStringLiteral(
                "SELECT COUNT(*) FROM information_schema.tables "
                "WHERE table_schema = :db AND table_name = 'users';"));
            q.bindValue(QStringLiteral(":db"), cleanDbName);
            if (q.exec() && q.next() && q.value(0).toInt() > 0) {
                if (schemaAlreadyExistsOut) {
                    *schemaAlreadyExistsOut = true;
                }
            }
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(tmpConn);
    return ok;
}

bool Database::isOpen() const
{
    if (!QSqlDatabase::contains(m_connectionName)) {
        return false;
    }
    return QSqlDatabase::database(m_connectionName, false).isOpen();
}

bool Database::isSqlite() const
{
    return m_activeConfig.isSqlite();
}

bool Database::isMariaDb() const
{
    return m_activeConfig.isMariaDb();
}

QString Database::engineName() const
{
    return isMariaDb() ? QStringLiteral("MariaDB (Red)") : QStringLiteral("SQLite (Local)");
}

const BootstrapData &Database::activeConfig() const
{
    return m_activeConfig;
}

QSqlDatabase Database::connection() const
{
    return QSqlDatabase::database(m_connectionName);
}

QString Database::databasePath() const
{
    if (isMariaDb()) {
        return QStringLiteral("mariadb://%1@%2:%3/%4")
            .arg(m_activeConfig.dbUser, m_activeConfig.dbHost)
            .arg(m_activeConfig.dbPort)
            .arg(m_activeConfig.dbName);
    }
    return m_activeConfig.sqlitePath;
}

bool Database::hasExistingSchema() const
{
    if (!isOpen()) {
        return false;
    }
    const QStringList tables = connection().tables(QSql::Tables);
    return tables.contains(QStringLiteral("users"), Qt::CaseInsensitive) &&
           tables.contains(QStringLiteral("roles"), Qt::CaseInsensitive);
}

bool Database::hasAnyUsers() const
{
    if (!hasExistingSchema()) {
        return false;
    }
    QSqlQuery q(connection());
    return q.exec(QStringLiteral("SELECT COUNT(*) FROM users;")) && q.next() && q.value(0).toInt() > 0;
}

bool Database::createInitialAccounts(const QString &adminName,
                                     const QString &adminEmail,
                                     const QString &adminPassword,
                                     bool createDailyUser,
                                     const QString &dailyName,
                                     const QString &dailyEmail,
                                     const QString &dailyPassword,
                                     const QString &dailyRoleName,
                                     QString *errorMessage)
{
    if (!isOpen()) {
        if (errorMessage) *errorMessage = QStringLiteral("La base de datos no está abierta.");
        return false;
    }

    QSqlDatabase db = connection();
    db.transaction();

    // Obtener ID del rol Administrador
    int adminRoleId = -1;
    {
        QSqlQuery rq(db);
        if (rq.exec(QStringLiteral("SELECT id FROM roles WHERE is_system = 1 LIMIT 1;")) && rq.next()) {
            adminRoleId = rq.value(0).toInt();
        }
    }
    if (adminRoleId <= 0) {
        db.rollback();
        if (errorMessage) *errorMessage = QStringLiteral("No se encontró el rol de sistema 'Administrador'.");
        return false;
    }

    // 1. Insertar usuario Administrador (contraseña ya definida por el cliente -> must_change_password = 0)
    {
        QSqlQuery qAdmin(db);
        qAdmin.prepare(QStringLiteral(
            "INSERT INTO users (email, full_name, role_id, password_hash, is_active, must_change_password) "
            "VALUES (:email, :full_name, :role_id, :password_hash, 1, 0);"));
        qAdmin.bindValue(QStringLiteral(":email"), adminEmail.trimmed().toLower());
        qAdmin.bindValue(QStringLiteral(":full_name"), adminName.trimmed());
        qAdmin.bindValue(QStringLiteral(":role_id"), adminRoleId);
        qAdmin.bindValue(QStringLiteral(":password_hash"), hashPassword(adminPassword));

        if (!qAdmin.exec()) {
            db.rollback();
            if (errorMessage) {
                *errorMessage = QStringLiteral("Error creando el usuario Administrador:\n%1")
                                    .arg(qAdmin.lastError().text());
            }
            return false;
        }
    }

    // 2. Insertar usuario operativo diario (obligatorio en Puesto Único estilo UNIX)
    if (createDailyUser) {
        int dailyRoleId = -1;
        QSqlQuery rq(db);
        rq.prepare(QStringLiteral("SELECT id FROM roles WHERE name = :name LIMIT 1;"));
        rq.bindValue(QStringLiteral(":name"), dailyRoleName);
        if (rq.exec() && rq.next()) {
            dailyRoleId = rq.value(0).toInt();
        }
        if (dailyRoleId <= 0) {
            QSqlQuery fallbackQ(db);
            if (fallbackQ.exec(QStringLiteral("SELECT id FROM roles WHERE is_system = 0 ORDER BY id ASC LIMIT 1;")) && fallbackQ.next()) {
                dailyRoleId = fallbackQ.value(0).toInt();
            }
        }

        QSqlQuery qDaily(db);
        qDaily.prepare(QStringLiteral(
            "INSERT INTO users (email, full_name, role_id, password_hash, is_active, must_change_password) "
            "VALUES (:email, :full_name, :role_id, :password_hash, 1, 0);"));
        qDaily.bindValue(QStringLiteral(":email"), dailyEmail.trimmed().toLower());
        qDaily.bindValue(QStringLiteral(":full_name"), dailyName.trimmed());
        qDaily.bindValue(QStringLiteral(":role_id"), dailyRoleId);
        qDaily.bindValue(QStringLiteral(":password_hash"), hashPassword(dailyPassword));

        if (!qDaily.exec()) {
            db.rollback();
            if (errorMessage) {
                *errorMessage = QStringLiteral("Error creando el usuario de operación diaria:\n%1")
                                    .arg(qDaily.lastError().text());
            }
            return false;
        }
    }

    if (!db.commit()) {
        db.rollback();
        if (errorMessage) *errorMessage = db.lastError().text();
        return false;
    }

    AuditEvent ev;
    ev.userEmail = adminEmail.trimmed().toLower();
    ev.userFullName = adminName.trimmed();
    ev.userRole = QStringLiteral("Administrador");
    ev.moduleId = QStringLiteral("setup");
    ev.targetTable = QStringLiteral("users");
    ev.action = QStringLiteral("INITIAL_SETUP");
    ev.authContext = QStringLiteral("SETUP_WIZARD");
    ev.details = createDailyUser
                     ? QStringLiteral("Inicialización Puesto Único (Admin + Usuario diario: %1)").arg(dailyEmail.trimmed().toLower())
                     : QStringLiteral("Inicialización Puesto #1 Red (Admin creado)");
    logAuditEvent(ev);

    return true;
}

bool Database::isTerminalNumberTakenByOtherHost(int terminalNumber, QString *otherHostOut) const
{
    if (!isOpen()) {
        return false;
    }
    const QString localHost = QHostInfo::localHostName();
    QSqlQuery q(connection());
    q.prepare(QStringLiteral("SELECT hostname FROM workstations WHERE terminal_number = :num LIMIT 1;"));
    q.bindValue(QStringLiteral(":num"), terminalNumber);
    if (q.exec() && q.next()) {
        const QString registeredHost = q.value(0).toString();
        if (registeredHost.compare(localHost, Qt::CaseInsensitive) != 0) {
            if (otherHostOut) *otherHostOut = registeredHost;
            return true;
        }
    }
    return false;
}

bool Database::registerCurrentWorkstation(int terminalNumber,
                                          const QString &terminalName,
                                          QString *errorMessage)
{
    if (!isOpen()) {
        return false;
    }

    const QString host = QHostInfo::localHostName();
    const QString desc = terminalName.trimmed().isEmpty()
                             ? QStringLiteral("Puesto #%1 (%2)").arg(terminalNumber).arg(host)
                             : terminalName.trimmed();

    QSqlQuery q(connection());
    if (isSqlite()) {
        q.prepare(QStringLiteral(
            "INSERT INTO workstations (terminal_number, hostname, description, last_seen_at) "
            "VALUES (:num, :host, :desc, CURRENT_TIMESTAMP) "
            "ON CONFLICT(terminal_number) DO UPDATE SET "
            "  hostname = excluded.hostname, "
            "  description = excluded.description, "
            "  last_seen_at = CURRENT_TIMESTAMP;"));
    } else {
        q.prepare(QStringLiteral(
            "INSERT INTO workstations (terminal_number, hostname, description, last_seen_at) "
            "VALUES (:num, :host, :desc, CURRENT_TIMESTAMP) "
            "ON DUPLICATE KEY UPDATE "
            "  hostname = VALUES(hostname), "
            "  description = VALUES(description), "
            "  last_seen_at = CURRENT_TIMESTAMP;"));
    }

    q.bindValue(QStringLiteral(":num"), qMax(1, terminalNumber));
    q.bindValue(QStringLiteral(":host"), host);
    q.bindValue(QStringLiteral(":desc"), desc);

    if (!q.exec()) {
        if (errorMessage) *errorMessage = q.lastError().text();
        return false;
    }
    return true;
}

QString Database::getSetting(const QString &key, const QString &defaultValue) const
{
    if (!isOpen()) {
        return defaultValue;
    }

    QSqlQuery query(connection());
    if (isSqlite()) {
        query.prepare(QStringLiteral("SELECT \"value\" FROM app_settings WHERE \"key\" = :key LIMIT 1;"));
    } else {
        query.prepare(QStringLiteral("SELECT `value` FROM app_settings WHERE `key` = :key LIMIT 1;"));
    }
    query.bindValue(QStringLiteral(":key"), key);

    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return defaultValue;
}

bool Database::setSetting(const QString &key, const QString &value, QString *errorMessage)
{
    if (!isOpen()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("La base de datos no está abierta.");
        }
        return false;
    }

    QSqlQuery query(connection());
    if (isSqlite()) {
        query.prepare(QStringLiteral(
            "INSERT INTO app_settings (\"key\", \"value\", updated_at) "
            "VALUES (:key, :value, CURRENT_TIMESTAMP) "
            "ON CONFLICT(\"key\") DO UPDATE SET \"value\" = excluded.\"value\", updated_at = CURRENT_TIMESTAMP;"));
    } else {
        query.prepare(QStringLiteral(
            "INSERT INTO app_settings (`key`, `value`, updated_at) "
            "VALUES (:key, :value, CURRENT_TIMESTAMP) "
            "ON DUPLICATE KEY UPDATE `value` = VALUES(`value`), updated_at = CURRENT_TIMESTAMP;"));
    }

    query.bindValue(QStringLiteral(":key"), key);
    query.bindValue(QStringLiteral(":value"), value.isNull() ? QStringLiteral("") : value);

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    return true;
}

QString Database::computeAuditRowHash(const QString &prevHash,
                                      const QString &createdAt,
                                      const QString &userEmail,
                                      const QString &moduleId,
                                      const QString &targetTable,
                                      const QString &recordId,
                                      const QString &action,
                                      const QString &oldValues,
                                      const QString &newValues,
                                      const QString &authContext,
                                      const QString &workstation,
                                      const QString &details)
{
    const QString payload = QStringList({
        prevHash, createdAt, userEmail, moduleId, targetTable, recordId,
        action, oldValues, newValues, authContext, workstation, details
    }).join(QStringLiteral("|#|"));

    return QString::fromLatin1(
        QCryptographicHash::hash(payload.toUtf8(), QCryptographicHash::Sha256).toHex());
}

void Database::logAudit(const QString &userEmail,
                        const QString &moduleId,
                        const QString &action,
                        const QString &details)
{
    AuditEvent ev;
    ev.userEmail = userEmail;
    ev.moduleId = moduleId;
    ev.action = action;
    ev.details = details;
    logAuditEvent(ev);
}

void Database::logAuditEvent(const AuditEvent &event)
{
    if (!isOpen()) {
        return;
    }

    AuditEvent ev = event;
    if (Session::instance().isAuthenticated()) {
        const UserSessionInfo &u = Session::instance().currentUser();
        if (ev.userId < 0) ev.userId = u.id;
        if (ev.userEmail.isEmpty()) ev.userEmail = u.email;
        if (ev.userFullName.isEmpty()) ev.userFullName = u.fullName;
        if (ev.userRole.isEmpty()) ev.userRole = u.roleName;
    }
    if (ev.authContext.isEmpty()) {
        ev.authContext = ElevationManager::instance().activeAuthContext();
    }

    const QString workstation = QStringLiteral("Puesto #%1 (%2)")
                                    .arg(m_activeConfig.terminalNumber)
                                    .arg(QHostInfo::localHostName());
    const QString createdAt = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));

    // Obtener hash anterior para encadenar integridad
    QString prevHash = QStringLiteral("GENESIS");
    {
        QSqlQuery lastQ(connection());
        if (lastQ.exec(QStringLiteral("SELECT row_hash FROM audit_log ORDER BY id DESC LIMIT 1;")) && lastQ.next()) {
            const QString last = lastQ.value(0).toString().trimmed();
            if (!last.isEmpty()) {
                prevHash = last;
            }
        }
    }

    const QString rowHash = computeAuditRowHash(
        prevHash, createdAt, ev.userEmail, ev.moduleId, ev.targetTable, ev.recordId,
        ev.action, ev.oldValuesJson, ev.newValuesJson, ev.authContext, workstation, ev.details);

    QSqlQuery query(connection());
    query.prepare(QStringLiteral(
        "INSERT INTO audit_log ("
        "  user_id, user_email, user_full_name, user_role, module_id, target_table, record_id, "
        "  action, old_values, new_values, auth_context, workstation, details, prev_hash, row_hash, created_at"
        ") VALUES ("
        "  :uid, :email, :fname, :role, :mod, :tbl, :rid, "
        "  :act, :oldv, :newv, :auth, :ws, :det, :phash, :rhash, :cat"
        ");"));

    query.bindValue(QStringLiteral(":uid"), ev.userId);
    query.bindValue(QStringLiteral(":email"), ev.userEmail.isNull() ? QStringLiteral("") : ev.userEmail);
    query.bindValue(QStringLiteral(":fname"), ev.userFullName.isNull() ? QStringLiteral("") : ev.userFullName);
    query.bindValue(QStringLiteral(":role"), ev.userRole.isNull() ? QStringLiteral("") : ev.userRole);
    query.bindValue(QStringLiteral(":mod"), ev.moduleId.isNull() ? QStringLiteral("") : ev.moduleId);
    query.bindValue(QStringLiteral(":tbl"), ev.targetTable.isNull() ? QStringLiteral("") : ev.targetTable);
    query.bindValue(QStringLiteral(":rid"), ev.recordId.isNull() ? QStringLiteral("") : ev.recordId);
    query.bindValue(QStringLiteral(":act"), ev.action.isNull() ? QStringLiteral("") : ev.action);
    query.bindValue(QStringLiteral(":oldv"), ev.oldValuesJson.isNull() ? QStringLiteral("") : ev.oldValuesJson);
    query.bindValue(QStringLiteral(":newv"), ev.newValuesJson.isNull() ? QStringLiteral("") : ev.newValuesJson);
    query.bindValue(QStringLiteral(":auth"), ev.authContext);
    query.bindValue(QStringLiteral(":ws"), workstation);
    query.bindValue(QStringLiteral(":det"), ev.details.isNull() ? QStringLiteral("") : ev.details);
    query.bindValue(QStringLiteral(":phash"), prevHash);
    query.bindValue(QStringLiteral(":rhash"), rowHash);
    query.bindValue(QStringLiteral(":cat"), createdAt);
    query.exec();
}

bool Database::verifyAuditChain(int *brokenRowIdOut, QString *reportOut) const
{
    if (brokenRowIdOut) *brokenRowIdOut = -1;
    if (!isOpen()) {
        if (reportOut) *reportOut = QStringLiteral("La base de datos no está abierta.");
        return false;
    }

    QSqlQuery q(connection());
    if (!q.exec(QStringLiteral(
            "SELECT id, created_at, user_email, module_id, target_table, record_id, "
            "       action, old_values, new_values, auth_context, workstation, details, "
            "       prev_hash, row_hash "
            "FROM audit_log ORDER BY id ASC;"))) {
        if (reportOut) *reportOut = q.lastError().text();
        return false;
    }

    QString expectedPrev = QStringLiteral("GENESIS");
    int checked = 0;
    int legacySkipped = 0;

    while (q.next()) {
        const int id = q.value(0).toInt();
        const QString prevHash = q.value(12).toString();
        const QString storedHash = q.value(13).toString();

        // Filas heredadas anteriores a la activación de hash chaining
        if (storedHash.isEmpty()) {
            ++legacySkipped;
            continue;
        }

        if (prevHash != expectedPrev) {
            if (brokenRowIdOut) *brokenRowIdOut = id;
            if (reportOut) {
                *reportOut = QStringLiteral("Cadena rota en registro de auditoría #%1 (posible eliminación o alteración manual).").arg(id);
            }
            return false;
        }

        const QString recomputed = computeAuditRowHash(
            prevHash,
            q.value(1).toString(),
            q.value(2).toString(),
            q.value(3).toString(),
            q.value(4).toString(),
            q.value(5).toString(),
            q.value(6).toString(),
            q.value(7).toString(),
            q.value(8).toString(),
            q.value(9).toString(),
            q.value(10).toString(),
            q.value(11).toString());

        if (recomputed != storedHash) {
            if (brokenRowIdOut) *brokenRowIdOut = id;
            if (reportOut) {
                *reportOut = QStringLiteral("Firma SHA-256 inválida en registro de auditoría #%1 (contenido modificado externamente).").arg(id);
            }
            return false;
        }

        expectedPrev = storedHash;
        ++checked;
    }

    if (reportOut) {
        *reportOut = QStringLiteral("Integridad verificada: %1 evento(s) sellados íntegros%2.")
                         .arg(checked)
                         .arg(legacySkipped > 0
                                  ? QStringLiteral(" (%1 registros históricos previos)").arg(legacySkipped)
                                  : QString());
    }
    return true;
}

bool Database::isSqliteFilePresentOnDisk() const
{
    if (!isSqlite()) {
        return true;
    }
    return QFile::exists(m_activeConfig.sqlitePath);
}

bool Database::createAutomaticBackup(QString *backupPathOut, QString *errorMessage)
{
    if (!isOpen() || !isSqlite()) {
        return false;
    }

    const QString backupDir = BootstrapConfig::autoBackupDirPath();
    const QString fileName = QStringLiteral("auto_%1.sqlite")
                                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString destPath = QDir(backupDir).filePath(fileName);

    QString escaped = destPath;
    escaped.replace(QLatin1Char('\''), QStringLiteral("''"));

    QSqlQuery q(connection());
    bool ok = q.exec(QStringLiteral("VACUUM INTO '%1';").arg(escaped));
    if (!ok) {
        q.exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE);"));
        ok = QFile::copy(m_activeConfig.sqlitePath, destPath);
    }

    if (!ok) {
        if (errorMessage) *errorMessage = QStringLiteral("No se pudo crear respaldo automático en %1").arg(destPath);
        return false;
    }

    QFile::setPermissions(destPath, QFile::ReadOwner | QFile::WriteOwner);

    // Rotación: conservar solo los últimos kMaxAutoBackups archivos
    QDir dir(backupDir);
    const QFileInfoList files = dir.entryInfoList(
        {QStringLiteral("auto_*.sqlite")}, QDir::Files, QDir::Time);
    for (int i = kMaxAutoBackups; i < files.size(); ++i) {
        QFile::remove(files.at(i).absoluteFilePath());
    }

    if (backupPathOut) *backupPathOut = destPath;
    return true;
}


bool Database::createMariaDbDump(QString *dumpPathOut,
                                 QString *errorMessage,
                                 const QString &customFilePath)
{
    return createSqlDump(true, dumpPathOut, errorMessage, customFilePath);
}

bool Database::createSqlDump(bool includeData,
                             QString *dumpPathOut,
                             QString *errorMessage,
                             const QString &customFilePath)
{
    if (!isOpen()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No hay una conexión de base de datos activa.");
        }
        return false;
    }

    const QString suffix = includeData ? QStringLiteral("full") : QStringLiteral("schema_only");
    const QString baseLabel = isMariaDb() ? m_activeConfig.dbName : QStringLiteral("qGlobalManager_sqlite");
    const QString destPath = customFilePath.trimmed().isEmpty()
                                 ? QDir(BootstrapConfig::protectedDbDirPath()).filePath(
                                       QStringLiteral("%1_%2_%3.sql")
                                           .arg(baseLabel,
                                                suffix,
                                                QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))))
                                 : customFilePath.trimmed();

    BootstrapConfig::hideDirectory(QFileInfo(destPath).absolutePath());

    QFile file(destPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No se pudo crear el archivo de dump en:\n%1").arg(destPath);
        }
        return false;
    }

    QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
#endif

    const bool sqlite = isSqlite();

    auto escapeSqlValue = [sqlite](const QVariant &val) -> QString {
        if (val.isNull() || !val.isValid()) {
            return QStringLiteral("NULL");
        }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        const int tid = val.typeId();
        if (tid == QMetaType::Bool) return val.toBool() ? QStringLiteral("1") : QStringLiteral("0");
        if (tid == QMetaType::Int || tid == QMetaType::UInt ||
            tid == QMetaType::LongLong || tid == QMetaType::ULongLong ||
            tid == QMetaType::Double || tid == QMetaType::Float) {
            return val.toString();
        }
        if (tid == QMetaType::QDateTime) {
            return QStringLiteral("'%1'").arg(val.toDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        }
        if (tid == QMetaType::QByteArray) {
            const QByteArray raw = val.toByteArray();
            if (raw.isEmpty()) return QStringLiteral("''");
            return sqlite ? QStringLiteral("X'%1'").arg(QString::fromLatin1(raw.toHex()))
                          : QStringLiteral("0x%1").arg(QString::fromLatin1(raw.toHex()));
        }
#else
        const QVariant::Type t = val.type();
        if (t == QVariant::Bool) return val.toBool() ? QStringLiteral("1") : QStringLiteral("0");
        if (t == QVariant::Int || t == QVariant::UInt ||
            t == QVariant::LongLong || t == QVariant::ULongLong ||
            t == QVariant::Double) {
            return val.toString();
        }
        if (t == QVariant::DateTime) {
            return QStringLiteral("'%1'").arg(val.toDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        }
        if (t == QVariant::ByteArray) {
            const QByteArray raw = val.toByteArray();
            if (raw.isEmpty()) return QStringLiteral("''");
            return sqlite ? QStringLiteral("X'%1'").arg(QString::fromLatin1(raw.toHex()))
                          : QStringLiteral("0x%1").arg(QString::fromLatin1(raw.toHex()));
        }
#endif
        QString s = val.toString();
        if (sqlite) {
            s.replace(QLatin1Char('\''), QStringLiteral("''"));
        } else {
            s.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
            s.replace(QLatin1Char('\''), QStringLiteral("\\'"));
            s.replace(QLatin1Char('\r'), QStringLiteral("\\r"));
            s.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
            s.replace(QChar(0), QStringLiteral("\\0"));
        }
        return QStringLiteral("'%1'").arg(s);
    };

    out << "-- ============================================================\n";
    out << "-- Global Manager - Dump SQL (" << engineName() << ")\n";
    out << "-- Modo          : " << (includeData ? "Completo (Estructura + Datos + Índices + Triggers)"
                                               : "Solo Estructura (Tablas + Índices + Triggers, sin datos)") << "\n";
    out << "-- Origen        : " << databasePath() << "\n";
    out << "-- Puesto        : #" << m_activeConfig.terminalNumber << " (" << QHostInfo::localHostName() << ")\n";
    out << "-- Fecha         : " << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) << "\n";
    out << "-- ============================================================\n\n";

    if (sqlite) {
        out << "PRAGMA foreign_keys = OFF;\n";
        out << "BEGIN TRANSACTION;\n\n";

        // 1. Tablas (+ datos opcionales)
        QSqlQuery tblQ(connection());
        if (!tblQ.exec(QStringLiteral(
                "SELECT name, sql FROM sqlite_master "
                "WHERE type = 'table' AND name NOT LIKE 'sqlite_%' ORDER BY name ASC;"))) {
            file.close();
            QFile::remove(destPath);
            if (errorMessage) *errorMessage = tblQ.lastError().text();
            return false;
        }

        while (tblQ.next()) {
            const QString table = tblQ.value(0).toString();
            const QString ddl = tblQ.value(1).toString();

            out << "-- Tabla: " << table << "\n";
            out << "DROP TABLE IF EXISTS \"" << table << "\";\n";
            out << ddl << ";\n\n";

            if (includeData) {
                QSqlQuery dataQ(connection());
                if (!dataQ.exec(QStringLiteral("SELECT * FROM \"%1\";").arg(table))) {
                    file.close();
                    QFile::remove(destPath);
                    if (errorMessage) *errorMessage = dataQ.lastError().text();
                    return false;
                }
                const QSqlRecord rec = dataQ.record();
                const int colCount = rec.count();
                QStringList cols;
                cols.reserve(colCount);
                for (int i = 0; i < colCount; ++i) {
                    cols.append(QStringLiteral("\"%1\"").arg(rec.fieldName(i)));
                }
                const QString colList = cols.join(QStringLiteral(", "));

                while (dataQ.next()) {
                    QStringList vals;
                    vals.reserve(colCount);
                    for (int i = 0; i < colCount; ++i) {
                        vals.append(escapeSqlValue(dataQ.value(i)));
                    }
                    out << "INSERT INTO \"" << table << "\" (" << colList << ") VALUES ("
                        << vals.join(QStringLiteral(", ")) << ");\n";
                }
                out << "\n";
            }
        }

        // 2. Índices explícitos
        QSqlQuery idxQ(connection());
        if (idxQ.exec(QStringLiteral(
                "SELECT name, sql FROM sqlite_master WHERE type = 'index' AND sql IS NOT NULL ORDER BY name ASC;"))) {
            bool headerPrinted = false;
            while (idxQ.next()) {
                if (!headerPrinted) {
                    out << "-- ============================================================\n";
                    out << "-- Índices\n";
                    out << "-- ============================================================\n";
                    headerPrinted = true;
                }
                out << idxQ.value(1).toString() << ";\n";
            }
            if (headerPrinted) out << "\n";
        }

        // 3. Vistas
        QSqlQuery viewQ(connection());
        if (viewQ.exec(QStringLiteral(
                "SELECT name, sql FROM sqlite_master WHERE type = 'view' AND sql IS NOT NULL ORDER BY name ASC;"))) {
            bool headerPrinted = false;
            while (viewQ.next()) {
                if (!headerPrinted) {
                    out << "-- ============================================================\n";
                    out << "-- Vistas\n";
                    out << "-- ============================================================\n";
                    headerPrinted = true;
                }
                out << "DROP VIEW IF EXISTS \"" << viewQ.value(0).toString() << "\";\n";
                out << viewQ.value(1).toString() << ";\n\n";
            }
        }

        // 4. Triggers
        QSqlQuery trgQ(connection());
        if (trgQ.exec(QStringLiteral(
                "SELECT name, sql FROM sqlite_master WHERE type = 'trigger' AND sql IS NOT NULL ORDER BY name ASC;"))) {
            bool headerPrinted = false;
            while (trgQ.next()) {
                if (!headerPrinted) {
                    out << "-- ============================================================\n";
                    out << "-- Triggers\n";
                    out << "-- ============================================================\n";
                    headerPrinted = true;
                }
                out << "DROP TRIGGER IF EXISTS \"" << trgQ.value(0).toString() << "\";\n";
                out << trgQ.value(1).toString() << ";\n\n";
            }
        }

        out << "COMMIT;\n";
        out << "PRAGMA foreign_keys = ON;\n";
    } else {
        // MariaDB: incluye CREATE DATABASE, tablas (con sus índices y FKs), datos opcionales, vistas y triggers
        out << "SET NAMES utf8mb4;\n";
        out << "SET FOREIGN_KEY_CHECKS = 0;\n\n";
        out << "CREATE DATABASE IF NOT EXISTS `" << m_activeConfig.dbName
            << "` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;\n";
        out << "USE `" << m_activeConfig.dbName << "`;\n\n";

        // 1. Tablas base (+ índices integrados en SHOW CREATE TABLE + datos opcionales)
        QSqlQuery tablesQ(connection());
        if (!tablesQ.exec(QStringLiteral("SHOW FULL TABLES WHERE Table_type = 'BASE TABLE';"))) {
            file.close();
            QFile::remove(destPath);
            if (errorMessage) *errorMessage = tablesQ.lastError().text();
            return false;
        }

        QStringList tables;
        while (tablesQ.next()) {
            tables.append(tablesQ.value(0).toString());
        }

        for (const QString &table : tables) {
            out << "-- ------------------------------------------------------------\n";
            out << "-- Estructura e índices de tabla `" << table << "`\n";
            out << "-- ------------------------------------------------------------\n";
            out << "DROP TABLE IF EXISTS `" << table << "`;\n";

            QSqlQuery ddlQ(connection());
            if (!ddlQ.exec(QStringLiteral("SHOW CREATE TABLE `%1`;").arg(table)) || !ddlQ.next()) {
                file.close();
                QFile::remove(destPath);
                if (errorMessage) *errorMessage = ddlQ.lastError().text();
                return false;
            }
            out << ddlQ.value(1).toString() << ";\n\n";

            if (includeData) {
                QSqlQuery dataQ(connection());
                if (!dataQ.exec(QStringLiteral("SELECT * FROM `%1`;").arg(table))) {
                    file.close();
                    QFile::remove(destPath);
                    if (errorMessage) *errorMessage = dataQ.lastError().text();
                    return false;
                }

                const QSqlRecord rec = dataQ.record();
                const int colCount = rec.count();
                QStringList colNames;
                colNames.reserve(colCount);
                for (int i = 0; i < colCount; ++i) {
                    colNames.append(QStringLiteral("`%1`").arg(rec.fieldName(i)));
                }
                const QString colList = colNames.join(QStringLiteral(", "));

                while (dataQ.next()) {
                    QStringList values;
                    values.reserve(colCount);
                    for (int i = 0; i < colCount; ++i) {
                        values.append(escapeSqlValue(dataQ.value(i)));
                    }
                    out << "INSERT INTO `" << table << "` (" << colList << ") VALUES ("
                        << values.join(QStringLiteral(", ")) << ");\n";
                }
                out << "\n";
            }
        }

        // 2. Vistas
        QSqlQuery viewsQ(connection());
        if (viewsQ.exec(QStringLiteral("SHOW FULL TABLES WHERE Table_type = 'VIEW';"))) {
            while (viewsQ.next()) {
                const QString viewName = viewsQ.value(0).toString();
                QSqlQuery vDdlQ(connection());
                if (vDdlQ.exec(QStringLiteral("SHOW CREATE VIEW `%1`;").arg(viewName)) && vDdlQ.next()) {
                    out << "DROP VIEW IF EXISTS `" << viewName << "`;\n";
                    out << vDdlQ.value(1).toString() << ";\n\n";
                }
            }
        }

        // 3. Triggers
        QSqlQuery triggersQ(connection());
        if (triggersQ.exec(QStringLiteral("SHOW TRIGGERS;"))) {
            bool headerPrinted = false;
            while (triggersQ.next()) {
                const QString trigName = triggersQ.value(0).toString();
                QSqlQuery tDdlQ(connection());
                if (tDdlQ.exec(QStringLiteral("SHOW CREATE TRIGGER `%1`;").arg(trigName)) && tDdlQ.next()) {
                    if (!headerPrinted) {
                        out << "-- ============================================================\n";
                        out << "-- Triggers\n";
                        out << "-- ============================================================\n";
                        out << "DELIMITER $$\n";
                        headerPrinted = true;
                    }
                    out << "DROP TRIGGER IF EXISTS `" << trigName << "`$$\n";
                    out << tDdlQ.value(2).toString() << "$$\n\n";
                }
            }
            if (headerPrinted) {
                out << "DELIMITER ;\n\n";
            }
        }

        out << "SET FOREIGN_KEY_CHECKS = 1;\n";
    }

    file.close();
    QFile::setPermissions(destPath, QFile::ReadOwner | QFile::WriteOwner);

    if (dumpPathOut) {
        *dumpPathOut = destPath;
    }
    return true;
}


bool Database::createSchema(QString *errorMessage)
{
    const bool sqlite = isSqlite();

    const QString pk   = sqlite ? QStringLiteral("INTEGER PRIMARY KEY AUTOINCREMENT")
                                : QStringLiteral("INT AUTO_INCREMENT PRIMARY KEY");
    const QString strU = sqlite ? QStringLiteral("TEXT NOT NULL UNIQUE COLLATE NOCASE")
                                : QStringLiteral("VARCHAR(190) NOT NULL UNIQUE");
    const QString strS = sqlite ? QStringLiteral("TEXT NOT NULL")
                                : QStringLiteral("VARCHAR(190) NOT NULL");
    const QString strD = sqlite ? QStringLiteral("TEXT NOT NULL DEFAULT ''")
                                : QStringLiteral("VARCHAR(500) NOT NULL DEFAULT ''");
    const QString strL = sqlite ? QStringLiteral("TEXT NOT NULL DEFAULT ''")
                                : QStringLiteral("TEXT");
    const QString ts   = sqlite ? QStringLiteral("TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP")
                                : QStringLiteral("TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP");
    const QString keyCol = sqlite ? QStringLiteral("\"key\" TEXT PRIMARY KEY")
                                  : QStringLiteral("`key` VARCHAR(190) PRIMARY KEY");
    const QString valCol = sqlite ? QStringLiteral("\"value\" TEXT NOT NULL DEFAULT ''")
                                  : QStringLiteral("`value` TEXT NOT NULL");

    const QStringList statements = {
        // 1. Configuración general
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS app_settings ("
            "  %1, %2, updated_at %3"
            ");").arg(keyCol, valCol, ts),

        // 2. Puestos de trabajo conectados (control Multi-Puesto)
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS workstations ("
            "  terminal_number INTEGER PRIMARY KEY,"
            "  hostname %1,"
            "  description %2,"
            "  last_seen_at %3"
            ");").arg(strS, strD, ts),

        // 3. Tabla soporte: Roles (con soporte de deprecado)
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS roles ("
            "  id %1,"
            "  name %2,"
            "  description %3,"
            "  is_system INTEGER NOT NULL DEFAULT 0,"
            "  is_active INTEGER NOT NULL DEFAULT 1,"
            "  is_deprecated INTEGER NOT NULL DEFAULT 0,"
            "  deprecated_at %3,"
            "  deprecated_by %3,"
            "  replacement_id INTEGER NULL,"
            "  deprecation_note %3"
            ");").arg(pk, strU, strD),

        // 4. Tabla soporte: Permisos maestros
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS permissions ("
            "  id %1,"
            "  module_id %2,"
            "  action %2,"
            "  description %3,"
            "  UNIQUE(module_id, action)"
            ");").arg(pk, strS, strD),

        // 5. Permisos por rol
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS role_permissions ("
            "  role_id INTEGER NOT NULL,"
            "  permission_id INTEGER NOT NULL,"
            "  PRIMARY KEY(role_id, permission_id),"
            "  FOREIGN KEY(role_id) REFERENCES roles(id) ON DELETE CASCADE,"
            "  FOREIGN KEY(permission_id) REFERENCES permissions(id) ON DELETE CASCADE"
            ");"),

        // 6. Usuarios (email = usuario de acceso)
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS users ("
            "  id %1,"
            "  email %2,"
            "  full_name %3,"
            "  role_id INTEGER NOT NULL,"
            "  password_hash %3,"
            "  is_active INTEGER NOT NULL DEFAULT 1,"
            "  must_change_password INTEGER NOT NULL DEFAULT 0,"
            "  created_at %4,"
            "  FOREIGN KEY(role_id) REFERENCES roles(id)"
            ");").arg(pk, strU, strS, ts),

        // 7. Excepciones de permisos por usuario
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS user_permissions ("
            "  user_id INTEGER NOT NULL,"
            "  permission_id INTEGER NOT NULL,"
            "  granted INTEGER NOT NULL DEFAULT 1,"
            "  PRIMARY KEY(user_id, permission_id),"
            "  FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE,"
            "  FOREIGN KEY(permission_id) REFERENCES permissions(id) ON DELETE CASCADE"
            ");"),

        // 8. Tablas soporte generales con soporte estándar de deprecado
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS iva_conditions ("
            "  id %1,"
            "  name %2,"
            "  is_active INTEGER NOT NULL DEFAULT 1,"
            "  is_deprecated INTEGER NOT NULL DEFAULT 0,"
            "  deprecated_at %3,"
            "  deprecated_by %3,"
            "  replacement_id INTEGER NULL,"
            "  deprecation_note %3"
            ");").arg(pk, strU, strD),

        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS document_types ("
            "  id %1,"
            "  code %2,"
            "  name %3,"
            "  is_active INTEGER NOT NULL DEFAULT 1,"
            "  is_deprecated INTEGER NOT NULL DEFAULT 0,"
            "  deprecated_at %4,"
            "  deprecated_by %4,"
            "  replacement_id INTEGER NULL,"
            "  deprecation_note %4"
            ");").arg(pk, strU, strS, strD),

        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS payment_methods ("
            "  id %1,"
            "  name %2,"
            "  is_active INTEGER NOT NULL DEFAULT 1,"
            "  is_deprecated INTEGER NOT NULL DEFAULT 0,"
            "  deprecated_at %3,"
            "  deprecated_by %3,"
            "  replacement_id INTEGER NULL,"
            "  deprecation_note %3"
            ");").arg(pk, strU, strD),

        // 9. Fuentes externas agnósticas (Etapa C)
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS external_sources ("
            "  id %1,"
            "  name %2,"
            "  source_type %3,"
            "  source_uri %4,"
            "  format %3,"
            "  delimiter %4,"
            "  key_field %4,"
            "  label_field %4,"
            "  value_field %4,"
            "  target_module %4,"
            "  last_sync_at %4,"
            "  is_active INTEGER NOT NULL DEFAULT 1"
            ");").arg(pk, strU, strS, strD),

        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS external_source_items ("
            "  id %1,"
            "  source_id INTEGER NOT NULL,"
            "  item_key %2,"
            "  item_label %3,"
            "  value_num DOUBLE NULL,"
            "  value_text %3,"
            "  updated_at %4,"
            "  UNIQUE(source_id, item_key),"
            "  FOREIGN KEY(source_id) REFERENCES external_sources(id) ON DELETE CASCADE"
            ");").arg(pk, strS, strD, ts),

        // 10. Registro de tokens de elevación desatendidos (Etapa B)
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS elevation_tokens ("
            "  id %1,"
            "  token_code %2,"
            "  user_id INTEGER NOT NULL,"
            "  user_email %3,"
            "  target_scope %3,"
            "  reason %4,"
            "  issued_at %5,"
            "  expires_at %4,"
            "  revoked_at %4,"
            "  FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE"
            ");").arg(pk, strU, strS, strD, ts),

        // 11. Bitácora inmutable de auditoría con hash chaining SHA-256
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS audit_log ("
            "  id %1,"
            "  user_id INTEGER NOT NULL DEFAULT -1,"
            "  user_email %2,"
            "  user_full_name %3,"
            "  user_role %3,"
            "  module_id %2,"
            "  target_table %3,"
            "  record_id %3,"
            "  action %2,"
            "  old_values %4,"
            "  new_values %4,"
            "  auth_context %3,"
            "  workstation %3,"
            "  details %4,"
            "  prev_hash %3,"
            "  row_hash %3,"
            "  created_at %5"
            ");").arg(pk, strS, strD, strL, ts),


            // 12. Módulo troncal: Clientes / Pacientes (adaptable Comercio / Consultorio)
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS clients ("
            "  id %1,"
            "  full_name %2,"
            "  trade_name %3,"
            "  document_type_id INTEGER NULL,"
            "  document_number %3,"
            "  iva_condition_id INTEGER NULL,"
            "  phone %3,"
            "  email %3,"
            "  address %3,"
            "  birth_date %3,"
            "  health_insurance %3,"
            "  affiliate_number %3,"
            "  credit_limit DOUBLE NOT NULL DEFAULT 0,"
            "  current_balance DOUBLE NOT NULL DEFAULT 0,"
            "  notes %4,"
            "  is_active INTEGER NOT NULL DEFAULT 1,"
            "  is_deprecated INTEGER NOT NULL DEFAULT 0,"
            "  deprecated_at %3,"
            "  deprecated_by %3,"
            "  deprecation_note %3,"
            "  created_at %5,"
            "  updated_at %5,"
            "  FOREIGN KEY(document_type_id) REFERENCES document_types(id),"
            "  FOREIGN KEY(iva_condition_id) REFERENCES iva_conditions(id)"
            ");").arg(pk, strS, strD, strL, ts)

    };

    QSqlQuery query(connection());
    for (const QString &sql : statements) {
        if (!query.exec(sql)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Error creando esquema de base de datos:\n%1")
                                    .arg(query.lastError().text());
            }
            return false;
        }
    }

    return true;
}

bool Database::ensureSqliteMigrations(QString *errorMessage)
{
    auto addMissingColumns = [this, errorMessage](const QString &table,
                                                  const QList<QPair<QString, QString>> &cols) -> bool {
        QSet<QString> existing;
        QSqlQuery infoQ(connection());
        if (infoQ.exec(QStringLiteral("PRAGMA table_info(\"%1\");").arg(table))) {
            while (infoQ.next()) {
                existing.insert(infoQ.value(1).toString().toLower());
            }
        }
        QSqlQuery alterQ(connection());
        for (const auto &col : cols) {
            if (!existing.contains(col.first.toLower())) {
                const QString sql = QStringLiteral("ALTER TABLE \"%1\" ADD COLUMN %2 %3;")
                                        .arg(table, col.first, col.second);
                if (!alterQ.exec(sql)) {
                    if (errorMessage) {
                        *errorMessage = QStringLiteral("Error migrando tabla '%1': %2")
                                            .arg(table, alterQ.lastError().text());
                    }
                    return false;
                }
            }
        }
        return true;
    };

    const QList<QPair<QString, QString>> deprecationCols = {
        {QStringLiteral("is_deprecated"),    QStringLiteral("INTEGER NOT NULL DEFAULT 0")},
        {QStringLiteral("deprecated_at"),    QStringLiteral("TEXT NOT NULL DEFAULT ''")},
        {QStringLiteral("deprecated_by"),    QStringLiteral("TEXT NOT NULL DEFAULT ''")},
        {QStringLiteral("replacement_id"),   QStringLiteral("INTEGER NULL")},
        {QStringLiteral("deprecation_note"), QStringLiteral("TEXT NOT NULL DEFAULT ''")}
    };

    if (!addMissingColumns(QStringLiteral("roles"), deprecationCols)) return false;
    if (!addMissingColumns(QStringLiteral("iva_conditions"), deprecationCols)) return false;
    if (!addMissingColumns(QStringLiteral("document_types"), deprecationCols)) return false;
    if (!addMissingColumns(QStringLiteral("payment_methods"), deprecationCols)) return false;

    const QList<QPair<QString, QString>> auditCols = {
        {QStringLiteral("user_id"),        QStringLiteral("INTEGER NOT NULL DEFAULT -1")},
        {QStringLiteral("user_full_name"), QStringLiteral("TEXT NOT NULL DEFAULT ''")},
        {QStringLiteral("user_role"),      QStringLiteral("TEXT NOT NULL DEFAULT ''")},
        {QStringLiteral("target_table"),   QStringLiteral("TEXT NOT NULL DEFAULT ''")},
        {QStringLiteral("record_id"),      QStringLiteral("TEXT NOT NULL DEFAULT ''")},
        {QStringLiteral("old_values"),     QStringLiteral("TEXT NOT NULL DEFAULT ''")},
        {QStringLiteral("new_values"),     QStringLiteral("TEXT NOT NULL DEFAULT ''")},
        {QStringLiteral("auth_context"),   QStringLiteral("TEXT NOT NULL DEFAULT 'NORMAL'")},
        {QStringLiteral("workstation"),    QStringLiteral("TEXT NOT NULL DEFAULT ''")},
        {QStringLiteral("prev_hash"),      QStringLiteral("TEXT NOT NULL DEFAULT ''")},
        {QStringLiteral("row_hash"),       QStringLiteral("TEXT NOT NULL DEFAULT ''")}
    };

    return addMissingColumns(QStringLiteral("audit_log"), auditCols);
}

bool Database::seedInitialData(QString *errorMessage)
{
    const QString insertIgnore = isSqlite() ? QStringLiteral("INSERT OR IGNORE")
                                            : QStringLiteral("INSERT IGNORE");
    QSqlQuery query(connection());

    // 1. Roles base
    const QStringList roleSeeds = {
        QStringLiteral("%1 INTO roles (name, description, is_system, is_active) "
                       "VALUES ('Administrador', 'Acceso total al sistema y Panel de Control', 1, 1);").arg(insertIgnore),
        QStringLiteral("%1 INTO roles (name, description, is_system, is_active) "
                       "VALUES ('Responsable / Supervisor', 'Gestión operativa y configuración de la compañía con elevación por token', 0, 1);").arg(insertIgnore),
        QStringLiteral("%1 INTO roles (name, description, is_system, is_active) "
                       "VALUES ('Operador', 'Operación diaria (vendedor, secretaria, profesional) según permisos asignados', 0, 1);").arg(insertIgnore)
    };
    for (const QString &sql : roleSeeds) {
        if (!query.exec(sql)) {
            if (errorMessage) *errorMessage = query.lastError().text();
            return false;
        }
    }

    // 2. Permisos maestros (incluyendo permisos granulares del Panel de Control y elevación)
    struct PermSeed { const char *mod; const char *act; const char *desc; };
    const PermSeed perms[] = {
        {"panel_control", "access",            "Acceder al Panel de Control"},
        {"panel_control", "company",           "Panel de Control: Datos de la Compañía"},
        {"panel_control", "smtp",              "Panel de Control: Correo / SMTP"},
        {"panel_control", "printers",          "Panel de Control: Impresoras del Sistema"},
        {"panel_control", "security",          "Panel de Control: Seguridad y Licencia (Sensible)"},
        {"panel_control", "support_tables",    "Panel de Control: Tablas Soporte (Sensible)"},
        {"panel_control", "backup_audit",      "Panel de Control: Respaldo y Auditoría (Sensible)"},
        {"panel_control", "db_admin",          "Panel de Control: Administrador SQL (Sensible)"},
        {"panel_control", "request_elevation", "Solicitar token temporal de elevación"},
        {"usuarios",      "access",            "Ver listado de usuarios"},
        {"usuarios",      "create",            "Dar de alta nuevos usuarios"},
        {"usuarios",      "edit",              "Editar datos de usuarios"},
        {"usuarios",      "permissions",       "Modificar permisos de usuarios"},
        {"usuarios",      "reset_password",    "Generar y reenviar nuevas contraseñas"},
        {"usuarios",      "block",             "Bloquear o desbloquear usuarios"},
        {"roles",         "access",            "Administrar roles del sistema"},
        {"clientes",      "access",            "Ver listado de Clientes / Pacientes"},
        {"clientes",      "create",            "Dar de alta Clientes / Pacientes"},
        {"clientes",      "edit",              "Editar datos de Clientes / Pacientes"},
        {"clientes",      "deprecate",         "Deprecar o reactivar Clientes / Pacientes"},
        {"clientes",      "report",            "Generar y enviar informes de Clientes / Pacientes"},
    };

    query.prepare(QStringLiteral(
        "%1 INTO permissions (module_id, action, description) VALUES (:mod, :act, :desc);").arg(insertIgnore));
    for (const auto &p : perms) {
        query.bindValue(QStringLiteral(":mod"), QString::fromLatin1(p.mod));
        query.bindValue(QStringLiteral(":act"), QString::fromLatin1(p.act));
        query.bindValue(QStringLiteral(":desc"), QString::fromUtf8(p.desc));
        if (!query.exec()) {
            if (errorMessage) *errorMessage = query.lastError().text();
            return false;
        }
    }

    // 2b. Permisos por defecto para Responsable / Supervisor (negocio + solicitud de token)
    const QStringList supervisorPerms = {
        QStringLiteral("access"),
        QStringLiteral("company"),
        QStringLiteral("smtp"),
        QStringLiteral("printers"),
        QStringLiteral("request_elevation")
    };
    {
        QSqlQuery rpQ(connection());
        rpQ.prepare(QStringLiteral(
            "%1 INTO role_permissions (role_id, permission_id) "
            "SELECT r.id, p.id FROM roles r, permissions p "
            "WHERE r.name = 'Responsable / Supervisor' "
            "  AND p.module_id = 'panel_control' AND p.action = :act;").arg(insertIgnore));
        for (const QString &act : supervisorPerms) {
            rpQ.bindValue(QStringLiteral(":act"), act);
            rpQ.exec();
        }
    }

    // 2c. Permisos por defecto para Operador (acceso a Panel de Control solo para configurar impresoras)
    const QStringList operatorPerms = {
        QStringLiteral("access"),
        QStringLiteral("printers")
    };
    {
        QSqlQuery opQ(connection());
        opQ.prepare(QStringLiteral(
            "%1 INTO role_permissions (role_id, permission_id) "
            "SELECT r.id, p.id FROM roles r, permissions p "
            "WHERE r.name = 'Operador' "
            "  AND p.module_id = 'panel_control' AND p.action = :act;").arg(insertIgnore));
        for (const QString &act : operatorPerms) {
            opQ.bindValue(QStringLiteral(":act"), act);
            opQ.exec();
        }
    }

    // 2d. Permisos por defecto del módulo Clientes / Pacientes para Supervisor y Operador
    const QStringList clientAllPerms = {
        QStringLiteral("access"),
        QStringLiteral("create"),
        QStringLiteral("edit"),
        QStringLiteral("deprecate"),
        QStringLiteral("report")
    };
    const QStringList clientOperatorPerms = {
        QStringLiteral("access"),
        QStringLiteral("create"),
        QStringLiteral("edit"),
        QStringLiteral("report")
    };
    {
        QSqlQuery cpQ(connection());
        cpQ.prepare(QStringLiteral(
            "%1 INTO role_permissions (role_id, permission_id) "
            "SELECT r.id, p.id FROM roles r, permissions p "
            "WHERE r.name = :rname AND p.module_id = 'clientes' AND p.action = :act;").arg(insertIgnore));

        for (const QString &act : clientAllPerms) {
            cpQ.bindValue(QStringLiteral(":rname"), QStringLiteral("Responsable / Supervisor"));
            cpQ.bindValue(QStringLiteral(":act"), act);
            cpQ.exec();
        }
        for (const QString &act : clientOperatorPerms) {
            cpQ.bindValue(QStringLiteral(":rname"), QStringLiteral("Operador"));
            cpQ.bindValue(QStringLiteral(":act"), act);
            cpQ.exec();
        }
    }

    // 3. Tablas soporte iniciales
    const QStringList supportSeeds = {
        QStringLiteral("%1 INTO iva_conditions (name) VALUES ('Responsable Inscripto');").arg(insertIgnore),
        QStringLiteral("%1 INTO iva_conditions (name) VALUES ('Monotributista');").arg(insertIgnore),
        QStringLiteral("%1 INTO iva_conditions (name) VALUES ('Exento');").arg(insertIgnore),
        QStringLiteral("%1 INTO iva_conditions (name) VALUES ('Consumidor Final');").arg(insertIgnore),

        QStringLiteral("%1 INTO document_types (code, name) VALUES ('DNI', 'Documento Nacional de Identidad');").arg(insertIgnore),
        QStringLiteral("%1 INTO document_types (code, name) VALUES ('CUIT', 'Clave Única de Identificación Tributaria');").arg(insertIgnore),
        QStringLiteral("%1 INTO document_types (code, name) VALUES ('CUIL', 'Clave Única de Identificación Laboral');").arg(insertIgnore),
        QStringLiteral("%1 INTO document_types (code, name) VALUES ('PAS', 'Pasaporte');").arg(insertIgnore),

        QStringLiteral("%1 INTO payment_methods (name) VALUES ('Efectivo');").arg(insertIgnore),
        QStringLiteral("%1 INTO payment_methods (name) VALUES ('Transferencia Bancaria');").arg(insertIgnore),
        QStringLiteral("%1 INTO payment_methods (name) VALUES ('Tarjeta de Débito');").arg(insertIgnore),
        QStringLiteral("%1 INTO payment_methods (name) VALUES ('Tarjeta de Crédito');").arg(insertIgnore),
        QStringLiteral("%1 INTO payment_methods (name) VALUES ('Billetera Virtual / QR');").arg(insertIgnore)
    };
    for (const QString &sql : supportSeeds) {
        query.exec(sql);
    }

    // 4. Parámetros por defecto en app_settings (incluyendo política de elevación desatendida)
    const QList<QPair<QString, QString>> defaultSettings = {
        {QStringLiteral("security.idle_minutes"),              QStringLiteral("15")},
        {QStringLiteral("security.warning_seconds"),           QStringLiteral("30")},
        {QStringLiteral("security.elevation_duration_min"),    QStringLiteral("20")},
        {QStringLiteral("security.elevation_allow_support"),   QStringLiteral("1")},
        {QStringLiteral("security.elevation_allow_backup"),    QStringLiteral("1")},
        {QStringLiteral("security.elevation_allow_db_admin"),  QStringLiteral("1")},
        {QStringLiteral("security.elevation_notify_email"),    QStringLiteral("0")},
        {QStringLiteral("company.business_type"),              QStringLiteral("Comercio")},
        {QStringLiteral("regional.currency_symbol"),           QStringLiteral("$")},
        {QStringLiteral("smtp.mode"),                          QStringLiteral("BASIC")},
        {QStringLiteral("smtp.provider"),                      QStringLiteral("GMAIL")},
        {QStringLiteral("smtp.port"),                          QStringLiteral("465")},
        {QStringLiteral("smtp.security"),                      QStringLiteral("SSL")}
    };

    const QString keyIdent = isSqlite() ? QStringLiteral("\"key\"") : QStringLiteral("`key`");
    const QString valIdent = isSqlite() ? QStringLiteral("\"value\"") : QStringLiteral("`value`");

    for (const auto &pair : defaultSettings) {
        QSqlQuery sQuery(connection());
        sQuery.prepare(QStringLiteral("%1 INTO app_settings (%2, %3) VALUES (:k, :v);")
                           .arg(insertIgnore, keyIdent, valIdent));
        sQuery.bindValue(QStringLiteral(":k"), pair.first);
        sQuery.bindValue(QStringLiteral(":v"), pair.second);
        sQuery.exec();
    }

    return true;
}

QByteArray Database::deriveKey(const QByteArray &password, const QByteArray &salt, int iterations)
{
    QByteArray digest = QCryptographicHash::hash(salt + password, QCryptographicHash::Sha256);
    for (int i = 1; i < iterations; ++i) {
        digest = QCryptographicHash::hash(digest + salt + password, QCryptographicHash::Sha256);
    }
    return digest;
}

bool Database::constantTimeEquals(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size()) {
        return false;
    }
    unsigned char diff = 0;
    for (int i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a.at(i) ^ b.at(i));
    }
    return diff == 0;
}

QString Database::hashPassword(const QString &plainPassword)
{
    QByteArray salt(kSaltBytes, Qt::Uninitialized);
    for (int i = 0; i < kSaltBytes; ++i) {
        salt[i] = static_cast<char>(QRandomGenerator::system()->generate() & 0xFF);
    }

    const QByteArray hash = deriveKey(plainPassword.toUtf8(), salt, kHashIterations);
    return QStringLiteral("sha256$%1$%2$%3")
        .arg(kHashIterations)
        .arg(QString::fromLatin1(salt.toHex()), QString::fromLatin1(hash.toHex()));
}

bool Database::verifyPassword(const QString &plainPassword, const QString &storedHash)
{
    const QStringList parts = storedHash.split(QLatin1Char('$'));
    if (parts.size() != 4 || parts.at(0) != QStringLiteral("sha256")) {
        return false;
    }

    bool ok = false;
    const int iterations = parts.at(1).toInt(&ok);
    if (!ok || iterations <= 0) {
        return false;
    }

    const QByteArray salt = QByteArray::fromHex(parts.at(2).toLatin1());
    const QByteArray expectedHash = QByteArray::fromHex(parts.at(3).toLatin1());
    const QByteArray computedHash = deriveKey(plainPassword.toUtf8(), salt, iterations);

    return constantTimeEquals(computedHash, expectedHash);
}

QString Database::generateRandomPassword(int length)
{
    const QString upper = QStringLiteral("ABCDEFGHJKLMNPQRSTUVWXYZ");
    const QString lower = QStringLiteral("abcdefghijkmnopqrstuvwxyz");
    const QString digits = QStringLiteral("23456789");
    const QString symbols = QStringLiteral("!@#$%&*?");
    const QString all = upper + lower + digits + symbols;

    const int finalLen = qMax(10, length);
    QString pwd;
    pwd.reserve(finalLen);

    auto pick = [](const QString &pool) -> QChar {
        const quint32 idx = QRandomGenerator::system()->bounded(static_cast<quint32>(pool.size()));
        return pool.at(static_cast<int>(idx));
    };

    pwd.append(pick(upper));
    pwd.append(pick(lower));
    pwd.append(pick(digits));
    pwd.append(pick(symbols));

    for (int i = 4; i < finalLen; ++i) {
        pwd.append(pick(all));
    }

    for (int i = pwd.size() - 1; i > 0; --i) {
        const int j = static_cast<int>(QRandomGenerator::system()->bounded(static_cast<quint32>(i + 1)));
        const QChar tmp = pwd[i];
        pwd[i] = pwd[j];
        pwd[j] = tmp;
    }

    return pwd;
}


bool Database::inspectMariaDbServer(const BootstrapData &config,
                                    MariaDbServerStatus *statusOut,
                                    QString *errorMessage)
{
    MariaDbServerStatus st;
    if (statusOut) *statusOut = st;

    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QMYSQL"))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                "El driver QMYSQL (MariaDB/MySQL) de Qt no está instalado en este equipo.");
        }
        return false;
    }

    const QString cleanDbName = config.dbName.trimmed();
    if (!isSafeDatabaseIdentifier(cleanDbName)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Nombre de base de datos inválido (usa solo letras, números y '_').");
        }
        return false;
    }

    const QString tmpConn = QStringLiteral("qGlobalManager_inspect_mariadb");
    bool ok = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QMYSQL"), tmpConn);
        db.setHostName(config.dbHost.trimmed());
        db.setPort(config.dbPort > 0 ? config.dbPort : 3306);
        db.setUserName(config.dbUser.trimmed());
        db.setPassword(config.dbPassword);

        if (!db.open()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Error conectando al servidor MariaDB:\n%1")
                                    .arg(db.lastError().text());
            }
        } else {
            ok = true;
            st.connected = true;

            QSqlQuery q(db);
            q.prepare(QStringLiteral(
                "SELECT COUNT(*) FROM information_schema.tables "
                "WHERE table_schema = :db AND table_name = 'users';"));
            q.bindValue(QStringLiteral(":db"), cleanDbName);
            if (q.exec() && q.next() && q.value(0).toInt() > 0) {
                st.schemaExists = true;

                QSqlQuery uq(db);
                if (uq.exec(QStringLiteral("SELECT COUNT(*) FROM `%1`.`users`;").arg(cleanDbName)) && uq.next()) {
                    st.hasUsers = (uq.value(0).toInt() > 0);
                }

                QSqlQuery wq(db);
                if (wq.exec(QStringLiteral(
                        "SELECT COUNT(*), COALESCE(MAX(terminal_number), 0) FROM `%1`.`workstations`;")
                                .arg(cleanDbName)) && wq.next()) {
                    st.registeredWorkstations = wq.value(0).toInt();
                    st.nextAvailableTerminal = wq.value(1).toInt() + 1;
                } else {
                    st.nextAvailableTerminal = st.hasUsers ? 2 : 1;
                }
            }
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(tmpConn);

    if (statusOut) {
        *statusOut = st;
    }
    return ok;
}

int Database::nextAvailableTerminalNumber() const
{
    if (!isOpen()) {
        return 1;
    }
    QSqlQuery q(connection());
    if (q.exec(QStringLiteral("SELECT COALESCE(MAX(terminal_number), 0) + 1 FROM workstations;")) && q.next()) {
        return qMax(1, q.value(0).toInt());
    }
    return 1;
}
