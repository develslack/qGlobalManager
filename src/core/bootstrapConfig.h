#ifndef CORE_BOOTSTRAPCONFIG_H
#define CORE_BOOTSTRAPCONFIG_H

#include <QString>

struct BootstrapData
{
    bool initialized = false;
    QString engine = QStringLiteral("SQLITE"); // "SQLITE" o "MARIADB"
    int terminalNumber = 1;
    QString terminalName;

    // Modo Local (SQLite)
    QString sqlitePath;

    // Modo Red (MariaDB)
    QString dbHost;
    int dbPort = 3306;
    QString dbName = QStringLiteral("qglobalmanager");
    QString dbUser;
    QString dbPassword;

    bool isSqlite() const;
    bool isMariaDb() const;
};

class BootstrapConfig
{
public:
    static QString protectedDirPath();
    static QString protectedDbDirPath();
    static QString configFilePath();
    static QString defaultSqlitePath();
    static QString autoBackupDirPath();

    static bool exists();
    static BootstrapData load();
    static bool save(const BootstrapData &data, QString *errorMessage = nullptr);
    static void hideDirectory(const QString &dirPath);

private:
    static QString encodeSecret(const QString &plainText);
    static QString decodeSecret(const QString &encodedText);
};

#endif // CORE_BOOTSTRAPCONFIG_H
