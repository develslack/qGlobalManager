#ifndef CORE_DB_H
#define CORE_DB_H

#include <QSqlDatabase>
#include <QString>

class Database
{
public:
    static Database &instance();

    bool initialize(QString *errorMessage = nullptr);
    bool isOpen() const;
    QSqlDatabase connection() const;
    QString databasePath() const;

    // Configuración global (tabla app_settings)
    QString getSetting(const QString &key, const QString &defaultValue = QString()) const;
    bool setSetting(const QString &key, const QString &value, QString *errorMessage = nullptr);

    // Registro de auditoría
    void logAudit(const QString &userEmail,
                  const QString &moduleId,
                  const QString &action,
                  const QString &details = QString());

    // Utilidades criptográficas
    static QString hashPassword(const QString &plainPassword);
    static bool verifyPassword(const QString &plainPassword, const QString &storedHash);
    static QString generateRandomPassword(int length = 12);

private:
    Database() = default;
    ~Database();
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    bool createSchema(QString *errorMessage);
    bool seedInitialData(QString *errorMessage);
    QString resolveDatabasePath() const;

    static QByteArray deriveKey(const QByteArray &password, const QByteArray &salt, int iterations);
    static bool constantTimeEquals(const QByteArray &a, const QByteArray &b);

    QString m_connectionName = QStringLiteral("qGlobalManager_main_conn");
    QString m_dbPath;
};

#endif // CORE_DB_H
