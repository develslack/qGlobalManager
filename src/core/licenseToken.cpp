#include "licenseToken.h"
#include "db.h"

#include <QByteArray>
#include <QMessageAuthenticationCode>
#include <QStringList>

namespace {
bool constantTimeEqual(const QByteArray &a, const QByteArray &b)
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
} // namespace

QByteArray LicenseToken::signingKey()
{
    // Cambia esta clave privada antes de compilar tu instalador comercial definitivo
    return QByteArrayLiteral("GM_SECRET_SIGNING_KEY_2026_CHANGE_FOR_PROD");
}

QString LicenseToken::normalizeCuit(const QString &cuit)
{
    QString out;
    for (const QChar &ch : cuit) {
        if (ch.isDigit()) {
            out.append(ch);
        }
    }
    return out;
}

QString LicenseToken::generate(const QString &scope, const QString &cuit, const QDate &expiresOn)
{
    const QString normCuit = cuit.trimmed().compare(QStringLiteral("ANY"), Qt::CaseInsensitive) == 0
                                 ? QStringLiteral("ANY")
                                 : normalizeCuit(cuit);

    const QByteArray payload = QStringLiteral("scope=%1;cuit=%2;exp=%3")
                                   .arg(scope.trimmed().toLower(),
                                        normCuit.isEmpty() ? QStringLiteral("ANY") : normCuit,
                                        expiresOn.toString(Qt::ISODate))
                                   .toUtf8();

    const QByteArray encodedPayload = payload.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    const QByteArray sig = QMessageAuthenticationCode::hash(encodedPayload, signingKey(), QCryptographicHash::Sha256)
                               .toHex()
                               .left(16)
                               .toUpper();

    return QStringLiteral("GM1.%1.%2")
        .arg(QString::fromLatin1(encodedPayload), QString::fromLatin1(sig));
}

LicenseInfo LicenseToken::validate(const QString &tokenText, const QString &expectedCuit)
{
    LicenseInfo info;
    const QString clean = tokenText.trimmed();
    if (clean.isEmpty()) {
        info.statusText = QStringLiteral("Sin token cargado.");
        return info;
    }

    const QStringList parts = clean.split(QLatin1Char('.'));
    if (parts.size() != 3 || parts.at(0) != QStringLiteral("GM1")) {
        info.statusText = QStringLiteral("Formato de token inválido.");
        return info;
    }

    const QByteArray encodedPayload = parts.at(1).toLatin1();
    const QByteArray providedSig = parts.at(2).toLatin1().toUpper();
    const QByteArray expectedSig = QMessageAuthenticationCode::hash(encodedPayload, signingKey(), QCryptographicHash::Sha256)
                                       .toHex()
                                       .left(16)
                                       .toUpper();

    if (!constantTimeEqual(providedSig, expectedSig)) {
        info.statusText = QStringLiteral("Firma de token inválida.");
        return info;
    }

    const QString decoded = QString::fromUtf8(QByteArray::fromBase64(encodedPayload, QByteArray::Base64UrlEncoding));
    const QStringList pairs = decoded.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &pair : pairs) {
        const int eq = pair.indexOf(QLatin1Char('='));
        if (eq <= 0) continue;
        const QString k = pair.left(eq);
        const QString v = pair.mid(eq + 1);
        if (k == QStringLiteral("scope")) info.scope = v;
        else if (k == QStringLiteral("cuit")) info.cuit = v;
        else if (k == QStringLiteral("exp")) info.expiresOn = QDate::fromString(v, Qt::ISODate);
    }

    if (!info.expiresOn.isValid() || info.scope.isEmpty()) {
        info.statusText = QStringLiteral("Contenido del token incompleto.");
        return info;
    }

    if (QDate::currentDate() > info.expiresOn) {
        info.statusText = QStringLiteral("Token vencido el %1.").arg(info.expiresOn.toString(QStringLiteral("dd/MM/yyyy")));
        return info;
    }

    const QString cleanExpected = normalizeCuit(expectedCuit);
    if (info.cuit != QStringLiteral("ANY") && !cleanExpected.isEmpty() && info.cuit != cleanExpected) {
        info.statusText = QStringLiteral("El token no corresponde al CUIT configurado (%1).").arg(expectedCuit);
        return info;
    }

    info.valid = true;
    info.statusText = QStringLiteral("Token válido (alcance: %1 · vence: %2)")
                          .arg(info.scope, info.expiresOn.toString(QStringLiteral("dd/MM/yyyy")));
    return info;
}

LicenseInfo LicenseToken::loadCurrent()
{
    const QString token = Database::instance().getSetting(QStringLiteral("license.token"));
    const QString cuit  = Database::instance().getSetting(QStringLiteral("company.cuit"));
    return validate(token, cuit);
}

bool LicenseToken::saveToken(const QString &tokenText, QString *errorMessage)
{
    const QString cuit = Database::instance().getSetting(QStringLiteral("company.cuit"));
    const LicenseInfo info = validate(tokenText, cuit);
    if (!info.valid) {
        if (errorMessage) *errorMessage = info.statusText;
        return false;
    }
    return Database::instance().setSetting(QStringLiteral("license.token"), tokenText.trimmed(), errorMessage);
}

bool LicenseToken::isDbAdminAllowed()
{
#ifdef APP_DEBUG_BUILD
    return true;
#else
    const LicenseInfo info = loadCurrent();
    return info.valid && (info.scope == QStringLiteral("db_admin") || info.scope == QStringLiteral("full"));
#endif
}
