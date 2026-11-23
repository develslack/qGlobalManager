#include "bootstrapConfig.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QHostInfo>
#include <QSettings>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

QByteArray bootstrapMaskKey()
{
    return QCryptographicHash::hash(
        QByteArrayLiteral("qGlobalManager::Bootstrap::v1::LocalKey"),
        QCryptographicHash::Sha256);
}

} // namespace

bool BootstrapData::isSqlite() const
{
    return engine.compare(QStringLiteral("MARIADB"), Qt::CaseInsensitive) != 0;
}

bool BootstrapData::isMariaDb() const
{
    return engine.compare(QStringLiteral("MARIADB"), Qt::CaseInsensitive) == 0;
}

void BootstrapConfig::hideDirectory(const QString &dirPath)
{
    QDir().mkpath(dirPath);
#ifdef Q_OS_WIN
    const std::wstring widePath = QDir::toNativeSeparators(dirPath).toStdWString();
    const DWORD attrs = GetFileAttributesW(widePath.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES) {
        SetFileAttributesW(widePath.c_str(), attrs | FILE_ATTRIBUTE_HIDDEN);
    }
#endif
}

QString BootstrapConfig::protectedDirPath()
{
    QString dirPath;
#ifdef APP_DEBUG_BUILD
    // En desarrollo: carpeta oculta fuera de build/ para que "./build.sh --clean" no borre la BD ni el arranque
    QDir appDir(QCoreApplication::applicationDirPath());
    if (appDir.dirName() == QStringLiteral("build")) {
        appDir.cdUp();
    }
    dirPath = appDir.filePath(QStringLiteral(".data_dev"));
#elif defined(Q_OS_WIN)
    const QString programData = qEnvironmentVariable("PROGRAMDATA", QStringLiteral("C:/ProgramData"));
    dirPath = QDir(programData).filePath(QStringLiteral("qGlobalManager/.vault"));
#else
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath();
    }
    dirPath = QDir(base).filePath(QStringLiteral(".qGlobalManager"));
#endif
    hideDirectory(dirPath);
    return dirPath;
}

QString BootstrapConfig::configFilePath()
{
    return QDir(protectedDirPath()).filePath(QStringLiteral("bootstrap.ini"));
}

QString BootstrapConfig::protectedDbDirPath()
{
    const QString dbDir = QDir(protectedDirPath()).filePath(QStringLiteral(".db"));
    hideDirectory(dbDir);
    return dbDir;
}

QString BootstrapConfig::defaultSqlitePath()
{
    return QDir(protectedDbDirPath()).filePath(QStringLiteral("qGlobalManager.sqlite"));
}

QString BootstrapConfig::autoBackupDirPath()
{
    const QString backupDir = QDir(protectedDirPath()).filePath(QStringLiteral(".autobackups"));
    hideDirectory(backupDir);
    return backupDir;
}

bool BootstrapConfig::exists()
{
    if (!QFile::exists(configFilePath())) {
        return false;
    }
    return load().initialized;
}

QString BootstrapConfig::encodeSecret(const QString &plainText)
{
    if (plainText.isEmpty()) {
        return QStringLiteral("");
    }
    const QByteArray key = bootstrapMaskKey();
    QByteArray data = plainText.toUtf8();
    for (int i = 0; i < data.size(); ++i) {
        data[i] = static_cast<char>(data.at(i) ^ key.at(i % key.size()));
    }
    return QStringLiteral("benc1:") + QString::fromLatin1(data.toBase64());
}

QString BootstrapConfig::decodeSecret(const QString &encodedText)
{
    if (!encodedText.startsWith(QStringLiteral("benc1:"))) {
        return encodedText;
    }
    const QByteArray key = bootstrapMaskKey();
    QByteArray data = QByteArray::fromBase64(encodedText.mid(6).toLatin1());
    for (int i = 0; i < data.size(); ++i) {
        data[i] = static_cast<char>(data.at(i) ^ key.at(i % key.size()));
    }
    return QString::fromUtf8(data);
}

BootstrapData BootstrapConfig::load()
{
    BootstrapData data;
    QSettings ini(configFilePath(), QSettings::IniFormat);

    // Usar sección "app/" (en Qt IniFormat, "general/" colisiona con la sección reservada [General])
    data.initialized    = ini.value(QStringLiteral("app/initialized"), false).toBool();
    data.engine         = ini.value(QStringLiteral("app/engine"), QStringLiteral("SQLITE")).toString().toUpper();
    data.terminalNumber = ini.value(QStringLiteral("app/terminal_number"), 1).toInt();
    data.terminalName   = ini.value(QStringLiteral("app/terminal_name"), QHostInfo::localHostName()).toString();

    data.sqlitePath     = ini.value(QStringLiteral("sqlite/path"), defaultSqlitePath()).toString();

    data.dbHost         = ini.value(QStringLiteral("mariadb/host")).toString();
    data.dbPort         = ini.value(QStringLiteral("mariadb/port"), 3306).toInt();
    data.dbName         = ini.value(QStringLiteral("mariadb/database"), QStringLiteral("qglobalmanager")).toString();
    data.dbUser         = ini.value(QStringLiteral("mariadb/user")).toString();
    data.dbPassword     = decodeSecret(ini.value(QStringLiteral("mariadb/password")).toString());

    return data;
}

bool BootstrapConfig::save(const BootstrapData &data, QString *errorMessage)
{
    protectedDirPath();

    QSettings ini(configFilePath(), QSettings::IniFormat);
    ini.setValue(QStringLiteral("app/initialized"), data.initialized);
    ini.setValue(QStringLiteral("app/engine"), data.engine.trimmed().toUpper());
    ini.setValue(QStringLiteral("app/terminal_number"), qMax(1, data.terminalNumber));
    ini.setValue(QStringLiteral("app/terminal_name"),
                 data.terminalName.trimmed().isEmpty() ? QHostInfo::localHostName() : data.terminalName.trimmed());

    ini.setValue(QStringLiteral("sqlite/path"),
                 data.sqlitePath.trimmed().isEmpty() ? defaultSqlitePath() : data.sqlitePath.trimmed());

    ini.setValue(QStringLiteral("mariadb/host"), data.dbHost.trimmed());
    ini.setValue(QStringLiteral("mariadb/port"), data.dbPort > 0 ? data.dbPort : 3306);
    ini.setValue(QStringLiteral("mariadb/database"),
                 data.dbName.trimmed().isEmpty() ? QStringLiteral("qglobalmanager") : data.dbName.trimmed());
    ini.setValue(QStringLiteral("mariadb/user"), data.dbUser.trimmed());
    ini.setValue(QStringLiteral("mariadb/password"), encodeSecret(data.dbPassword));

    ini.sync();
    if (ini.status() != QSettings::NoError) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No se pudo guardar el archivo local de configuración en:\n%1")
                                .arg(configFilePath());
        }
        return false;
    }

    QFile::setPermissions(configFilePath(), QFile::ReadOwner | QFile::WriteOwner);
    return true;
}
