#ifndef CORE_LICENSETOKEN_H
#define CORE_LICENSETOKEN_H

#include <QDate>
#include <QString>

struct LicenseInfo
{
    bool valid = false;
    QString scope;       // "client", "db_admin", "full"
    QString cuit;        // CUIT autorizado o "ANY"
    QDate expiresOn;
    QString statusText;
};

class LicenseToken
{
public:
    static LicenseInfo validate(const QString &tokenText, const QString &expectedCuit = QString());
    static LicenseInfo loadCurrent();
    static bool saveToken(const QString &tokenText, QString *errorMessage = nullptr);

    // En modo --debug siempre retorna true; en --release requiere token válido con scope db_admin o full
    static bool isDbAdminAllowed();

    // Generador para entorno de desarrollo/soporte
    static QString generate(const QString &scope, const QString &cuit, const QDate &expiresOn);

private:
    static QByteArray signingKey();
    static QString normalizeCuit(const QString &cuit);
};

#endif // CORE_LICENSETOKEN_H
