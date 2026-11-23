#ifndef CORE_DB_H
#define CORE_DB_H

#include "bootstrapConfig.h"

#include <QSqlDatabase>
#include <QString>

struct AuditEvent
{
    int userId = -1;
    QString userEmail;
    QString userFullName;
    QString userRole;
    QString moduleId;
    QString targetTable;
    QString recordId;
    QString action;
    QString oldValuesJson;
    QString newValuesJson;
    QString authContext;   // "NORMAL" o "TOKEN_ELEVATION:<id>:<motivo>"
    QString details;
};

struct MariaDbServerStatus
{
    bool connected = false;
    bool schemaExists = false;
    bool hasUsers = false;
    int registeredWorkstations = 0;
    int nextAvailableTerminal = 1;
};

class Database
{
public:
    static Database &instance();

    // Inicialización usando bootstrap.ini o configuración explícita (desde SetupWizard)
    bool initialize(QString *errorMessage = nullptr);
    bool initializeWithConfig(const BootstrapData &config,
                              bool createSchemaIfMissing,
                              QString *errorMessage = nullptr);
    void close();

    // Prueba de conexión sin alterar la conexión activa (para SetupWizard MariaDB)
    static bool testMariaDbConnection(const BootstrapData &config,
                                      bool *schemaAlreadyExistsOut,
                                      QString *errorMessage = nullptr);

    bool isOpen() const;
    bool isSqlite() const;
    bool isMariaDb() const;
    QString engineName() const;
    const BootstrapData &activeConfig() const;

    QSqlDatabase connection() const;
    QString databasePath() const;

    // Utilidades de primer arranque y puestos de trabajo
    bool hasExistingSchema() const;
    bool hasAnyUsers() const;
    bool createInitialAccounts(const QString &adminName,
                               const QString &adminEmail,
                               const QString &adminPassword,
                               bool createDailyUser,
                               const QString &dailyName,
                               const QString &dailyEmail,
                               const QString &dailyPassword,
                               const QString &dailyRoleName,
                               QString *errorMessage = nullptr);

    bool isTerminalNumberTakenByOtherHost(int terminalNumber, QString *otherHostOut = nullptr) const;
    bool registerCurrentWorkstation(int terminalNumber,
                                    const QString &terminalName,
                                    QString *errorMessage = nullptr);

    // Configuración global (tabla app_settings)
    QString getSetting(const QString &key, const QString &defaultValue = QString()) const;
    bool setSetting(const QString &key, const QString &value, QString *errorMessage = nullptr);

    // Bitácora de auditoría con cadena de integridad SHA-256
    void logAudit(const QString &userEmail,
                  const QString &moduleId,
                  const QString &action,
                  const QString &details = QString());
    void logAuditEvent(const AuditEvent &event);
    bool verifyAuditChain(int *brokenRowIdOut = nullptr, QString *reportOut = nullptr) const;

    // Respaldos automáticos silenciosos (SQLite) y verificación en disco
    bool createAutomaticBackup(QString *backupPathOut = nullptr, QString *errorMessage = nullptr);
    bool createMariaDbDump(QString *dumpPathOut = nullptr,
                           QString *errorMessage = nullptr,
                           const QString &customFilePath = QString());
    bool isSqliteFilePresentOnDisk() const;

    // Utilidades criptográficas
    static QString hashPassword(const QString &plainPassword);
    static bool verifyPassword(const QString &plainPassword, const QString &storedHash);
    static QString generateRandomPassword(int length = 12);

    static bool inspectMariaDbServer(const BootstrapData &config,
                                 MariaDbServerStatus *statusOut,
                                 QString *errorMessage = nullptr);

    int nextAvailableTerminalNumber() const;

    // Dump SQL portátil (SQLite y MariaDB): completo (estructura + datos + índices + triggers) o solo estructura
    bool createSqlDump(bool includeData,
                    QString *dumpPathOut = nullptr,
                    QString *errorMessage = nullptr,
                    const QString &customFilePath = QString());

private:
    Database() = default;
    ~Database();
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    bool createSchema(QString *errorMessage);
    bool ensureSqliteMigrations(QString *errorMessage);
    bool seedInitialData(QString *errorMessage);
    static bool isSafeDatabaseIdentifier(const QString &name);
    static QString computeAuditRowHash(const QString &prevHash,
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
                                       const QString &details);

    static QByteArray deriveKey(const QByteArray &password, const QByteArray &salt, int iterations);
    static bool constantTimeEquals(const QByteArray &a, const QByteArray &b);

    QString m_connectionName = QStringLiteral("qGlobalManager_main_conn");
    BootstrapData m_activeConfig;
};

#endif // CORE_DB_H
