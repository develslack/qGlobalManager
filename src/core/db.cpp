#include "db.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QRandomGenerator>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStringList>
#include <QVariant>


// ============================================================================================================================================= //


namespace {
constexpr int kSaltBytes = 16;
constexpr int kHashIterations = 100000;
}


// ============================================================================================================================================= //


Database &Database::instance()
{
    static Database db;
    return db;
}


// ============================================================================================================================================= //


Database::~Database()
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


// ============================================================================================================================================= //


QString Database::resolveDatabasePath() const
{
#ifdef APP_DEBUG_BUILD
    const QString dataDir = QCoreApplication::applicationDirPath() + QStringLiteral("/data");
#else
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDir.isEmpty()) {
        dataDir = QCoreApplication::applicationDirPath() + QStringLiteral("/data");
    }
#endif
    QDir().mkpath(dataDir);
    return dataDir + QStringLiteral("/qGlobalManager.sqlite");
}


// ============================================================================================================================================= //


bool Database::initialize(QString *errorMessage)
{
    if (isOpen()) {
        return true;
    }

    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No se encontró el driver SQLite de Qt (QSQLITE).");
        }
        return false;
    }

    m_dbPath = resolveDatabasePath();

    QSqlDatabase db = QSqlDatabase::contains(m_connectionName)
                          ? QSqlDatabase::database(m_connectionName)
                          : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);

    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No se pudo abrir la base de datos: %1")
                                .arg(db.lastError().text());
        }
        return false;
    }

    QSqlQuery pragma(db);
    pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON;"));
    pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL;"));

    if (!createSchema(errorMessage)) {
        return false;
    }

    if (!seedInitialData(errorMessage)) {
        return false;
    }

    return true;
}


// ============================================================================================================================================= //


bool Database::isOpen() const
{
    if (!QSqlDatabase::contains(m_connectionName)) {
        return false;
    }
    return QSqlDatabase::database(m_connectionName, false).isOpen();
}


// ============================================================================================================================================= //


QSqlDatabase Database::connection() const
{
    return QSqlDatabase::database(m_connectionName);
}


// ============================================================================================================================================= //


QString Database::databasePath() const
{
    return m_dbPath;
}


// ============================================================================================================================================= //


QString Database::getSetting(const QString &key, const QString &defaultValue) const
{
    if (!isOpen()) {
        return defaultValue;
    }

    QSqlQuery query(connection());
    query.prepare(QStringLiteral("SELECT value FROM app_settings WHERE key = :key LIMIT 1;"));
    query.bindValue(QStringLiteral(":key"), key);

    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return defaultValue;
}


// ============================================================================================================================================= //


bool Database::setSetting(const QString &key, const QString &value, QString *errorMessage)
{
    if (!isOpen()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("La base de datos no está abierta.");
        }
        return false;
    }

    QSqlQuery query(connection());
    query.prepare(QStringLiteral(
        "INSERT INTO app_settings (key, value, updated_at) "
        "VALUES (:key, :value, CURRENT_TIMESTAMP) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value, updated_at = CURRENT_TIMESTAMP;"
    ));
    query.bindValue(QStringLiteral(":key"), key);
    query.bindValue(QStringLiteral(":value"), value);

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }
    return true;
}


// ============================================================================================================================================= //


void Database::logAudit(const QString &userEmail,
                        const QString &moduleId,
                        const QString &action,
                        const QString &details)
{
    if (!isOpen()) {
        return;
    }

    QSqlQuery query(connection());
    query.prepare(QStringLiteral(
        "INSERT INTO audit_log (user_email, module_id, action, details) "
        "VALUES (:email, :module_id, :action, :details);"
    ));
    query.bindValue(QStringLiteral(":email"), userEmail);
    query.bindValue(QStringLiteral(":module_id"), moduleId);
    query.bindValue(QStringLiteral(":action"), action);
    query.bindValue(QStringLiteral(":details"), details);
    query.exec();
}


// ============================================================================================================================================= //


bool Database::createSchema(QString *errorMessage)
{
    const QStringList statements = {
        // 1. Configuración general de la aplicación
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS app_settings ("
            "  key TEXT PRIMARY KEY,"
            "  value TEXT NOT NULL DEFAULT '',"
            "  updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP"
            ");"),

        // 2. Tabla soporte: Roles
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS roles ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  name TEXT NOT NULL UNIQUE COLLATE NOCASE,"
            "  description TEXT NOT NULL DEFAULT '',"
            "  is_system INTEGER NOT NULL DEFAULT 0,"
            "  is_active INTEGER NOT NULL DEFAULT 1"
            ");"),

        // 3. Tabla soporte: Catálogo de Permisos
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS permissions ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  module_id TEXT NOT NULL,"
            "  action TEXT NOT NULL,"
            "  description TEXT NOT NULL DEFAULT '',"
            "  UNIQUE(module_id, action)"
            ");"),

        // 4. Relación Rol <-> Permisos predeterminados
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS role_permissions ("
            "  role_id INTEGER NOT NULL,"
            "  permission_id INTEGER NOT NULL,"
            "  PRIMARY KEY(role_id, permission_id),"
            "  FOREIGN KEY(role_id) REFERENCES roles(id) ON DELETE CASCADE,"
            "  FOREIGN KEY(permission_id) REFERENCES permissions(id) ON DELETE CASCADE"
            ");"),

        // 5. Usuarios (identificador de acceso = email)
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS users ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  email TEXT NOT NULL UNIQUE COLLATE NOCASE,"
            "  full_name TEXT NOT NULL,"
            "  role_id INTEGER NOT NULL,"
            "  password_hash TEXT NOT NULL,"
            "  is_active INTEGER NOT NULL DEFAULT 1,"
            "  must_change_password INTEGER NOT NULL DEFAULT 0,"
            "  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
            "  FOREIGN KEY(role_id) REFERENCES roles(id)"
            ");"),

        // 6. Permisos puntuales por usuario (override sobre el rol)
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS user_permissions ("
            "  user_id INTEGER NOT NULL,"
            "  permission_id INTEGER NOT NULL,"
            "  granted INTEGER NOT NULL DEFAULT 1,"
            "  PRIMARY KEY(user_id, permission_id),"
            "  FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE,"
            "  FOREIGN KEY(permission_id) REFERENCES permissions(id) ON DELETE CASCADE"
            ");"),

        // 7. Tablas soporte generales para comercios y consultorios
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS iva_conditions ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  name TEXT NOT NULL UNIQUE COLLATE NOCASE,"
            "  is_active INTEGER NOT NULL DEFAULT 1"
            ");"),

        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS document_types ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  code TEXT NOT NULL UNIQUE COLLATE NOCASE,"
            "  name TEXT NOT NULL,"
            "  is_active INTEGER NOT NULL DEFAULT 1"
            ");"),

        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS payment_methods ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  name TEXT NOT NULL UNIQUE COLLATE NOCASE,"
            "  is_active INTEGER NOT NULL DEFAULT 1"
            ");"),

        // 8. Bitácora de auditoría
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS audit_log ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  user_email TEXT NOT NULL,"
            "  module_id TEXT NOT NULL,"
            "  action TEXT NOT NULL,"
            "  details TEXT NOT NULL DEFAULT '',"
            "  created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP"
            ");")
    };

    QSqlQuery query(connection());
    for (const QString &sql : statements) {
        if (!query.exec(sql)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Error creando esquema SQLite: %1")
                                    .arg(query.lastError().text());
            }
            return false;
        }
    }

    return true;
}


// ============================================================================================================================================= //


bool Database::seedInitialData(QString *errorMessage)
{
    QSqlQuery query(connection());

    // 1. Roles base
    const QStringList roleSeeds = {
        QStringLiteral("INSERT OR IGNORE INTO roles (name, description, is_system, is_active) "
                       "VALUES ('Administrador', 'Acceso total al sistema y Panel de Control', 1, 1);"),
        QStringLiteral("INSERT OR IGNORE INTO roles (name, description, is_system, is_active) "
                       "VALUES ('Operador', 'Operación diaria según permisos asignados', 0, 1);")
    };
    for (const QString &sql : roleSeeds) {
        if (!query.exec(sql)) {
            if (errorMessage) *errorMessage = query.lastError().text();
            return false;
        }
    }

    // 2. Permisos maestros iniciales
    struct PermSeed { const char *mod; const char *act; const char *desc; };
    const PermSeed perms[] = {
        {"panel_control", "access",      "Acceder al Panel de Control"},
        {"usuarios",      "access",      "Ver listado de usuarios"},
        {"usuarios",      "create",      "Dar de alta nuevos usuarios"},
        {"usuarios",      "edit",        "Editar datos de usuarios"},
        {"usuarios",      "permissions", "Modificar permisos de usuarios"},
        {"usuarios",      "block",       "Bloquear o desbloquear usuarios"},
        {"usuarios", "reset_password", "Generar y reenviar nuevas contraseñas"},
        {"roles",         "access",      "Administrar roles del sistema"}
    };

    query.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO permissions (module_id, action, description) "
        "VALUES (:mod, :act, :desc);"));
    for (const auto &p : perms) {
        query.bindValue(QStringLiteral(":mod"), QString::fromLatin1(p.mod));
        query.bindValue(QStringLiteral(":act"), QString::fromLatin1(p.act));
        query.bindValue(QStringLiteral(":desc"), QString::fromUtf8(p.desc));
        if (!query.exec()) {
            if (errorMessage) *errorMessage = query.lastError().text();
            return false;
        }
    }

    // 3. Tablas soporte iniciales (IVA, Documentos, Medios de pago)
    const QStringList supportSeeds = {
        QStringLiteral("INSERT OR IGNORE INTO iva_conditions (name) VALUES ('Responsable Inscripto');"),
        QStringLiteral("INSERT OR IGNORE INTO iva_conditions (name) VALUES ('Monotributista');"),
        QStringLiteral("INSERT OR IGNORE INTO iva_conditions (name) VALUES ('Exento');"),
        QStringLiteral("INSERT OR IGNORE INTO iva_conditions (name) VALUES ('Consumidor Final');"),

        QStringLiteral("INSERT OR IGNORE INTO document_types (code, name) VALUES ('DNI', 'Documento Nacional de Identidad');"),
        QStringLiteral("INSERT OR IGNORE INTO document_types (code, name) VALUES ('CUIT', 'Clave Única de Identificación Tributaria');"),
        QStringLiteral("INSERT OR IGNORE INTO document_types (code, name) VALUES ('CUIL', 'Clave Única de Identificación Laboral');"),
        QStringLiteral("INSERT OR IGNORE INTO document_types (code, name) VALUES ('PAS', 'Pasaporte');"),

        QStringLiteral("INSERT OR IGNORE INTO payment_methods (name) VALUES ('Efectivo');"),
        QStringLiteral("INSERT OR IGNORE INTO payment_methods (name) VALUES ('Transferencia Bancaria');"),
        QStringLiteral("INSERT OR IGNORE INTO payment_methods (name) VALUES ('Tarjeta de Débito');"),
        QStringLiteral("INSERT OR IGNORE INTO payment_methods (name) VALUES ('Tarjeta de Crédito');"),
        QStringLiteral("INSERT OR IGNORE INTO payment_methods (name) VALUES ('Billetera Virtual / QR');")
    };
    for (const QString &sql : supportSeeds) {
        query.exec(sql);
    }

    // 4. Parámetros por defecto en app_settings
    const QList<QPair<QString, QString>> defaultSettings = {
        {QStringLiteral("security.idle_minutes"),     QStringLiteral("15")},
        {QStringLiteral("security.warning_seconds"),  QStringLiteral("30")},
        {QStringLiteral("company.business_type"),     QStringLiteral("Comercio")},
        {QStringLiteral("regional.currency_symbol"),  QStringLiteral("$")},
        {QStringLiteral("smtp.port"),                 QStringLiteral("587")},
        {QStringLiteral("smtp.security"),             QStringLiteral("STARTTLS")}
    };
    for (const auto &pair : defaultSettings) {
        QSqlQuery sQuery(connection());
        sQuery.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO app_settings (key, value) VALUES (:k, :v);"));
        sQuery.bindValue(QStringLiteral(":k"), pair.first);
        sQuery.bindValue(QStringLiteral(":v"), pair.second);
        sQuery.exec();
    }

    // 5. Usuario administrador inicial con cambio obligatorio de contraseña
    QSqlQuery countQuery(connection());
    if (!countQuery.exec(QStringLiteral("SELECT COUNT(*) FROM users;")) || !countQuery.next()) {
        if (errorMessage) *errorMessage = countQuery.lastError().text();
        return false;
    }

    if (countQuery.value(0).toInt() == 0) {
        QSqlQuery roleIdQuery(connection());
        roleIdQuery.exec(QStringLiteral("SELECT id FROM roles WHERE is_system = 1 LIMIT 1;"));
        int adminRoleId = 1;
        if (roleIdQuery.next()) {
            adminRoleId = roleIdQuery.value(0).toInt();
        }

        QSqlQuery insertAdmin(connection());
        insertAdmin.prepare(QStringLiteral(
            "INSERT INTO users (email, full_name, role_id, password_hash, is_active, must_change_password) "
            "VALUES (:email, :full_name, :role_id, :password_hash, 1, 1);"));
        insertAdmin.bindValue(QStringLiteral(":email"), QStringLiteral("admin@globalmanager.local"));
        insertAdmin.bindValue(QStringLiteral(":full_name"), QStringLiteral("Administrador General"));
        insertAdmin.bindValue(QStringLiteral(":role_id"), adminRoleId);
        insertAdmin.bindValue(QStringLiteral(":password_hash"), hashPassword(QStringLiteral("Admin123!")));

        if (!insertAdmin.exec()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Error creando administrador inicial: %1")
                                    .arg(insertAdmin.lastError().text());
            }
            return false;
        }
    }

    return true;
}


// ============================================================================================================================================= //


QByteArray Database::deriveKey(const QByteArray &password, const QByteArray &salt, int iterations)
{
    QByteArray digest = QCryptographicHash::hash(salt + password, QCryptographicHash::Sha256);
    for (int i = 1; i < iterations; ++i) {
        digest = QCryptographicHash::hash(digest + salt + password, QCryptographicHash::Sha256);
    }
    return digest;
}


// ============================================================================================================================================= //


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


// ============================================================================================================================================= //


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


// ============================================================================================================================================= //


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


// ============================================================================================================================================= //


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

    // Garantizar al menos un carácter de cada tipo
    pwd.append(pick(upper));
    pwd.append(pick(lower));
    pwd.append(pick(digits));
    pwd.append(pick(symbols));

    for (int i = 4; i < finalLen; ++i) {
        pwd.append(pick(all));
    }

    // Mezclar posiciones (Fisher-Yates)
    for (int i = pwd.size() - 1; i > 0; --i) {
        const int j = static_cast<int>(QRandomGenerator::system()->bounded(static_cast<quint32>(i + 1)));
        const QChar tmp = pwd[i];
        pwd[i] = pwd[j];
        pwd[j] = tmp;
    }

    return pwd;
}


// ============================================================================================================================================= //
