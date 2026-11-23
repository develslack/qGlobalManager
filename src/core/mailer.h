#ifndef CORE_MAILER_H
#define CORE_MAILER_H

#include <QString>
#include <QStringList>
#include <QtGlobal>

struct SmtpConfig
{
    QString mode = QStringLiteral("BASIC");            // "BASIC" o "ADVANCED"
    QString provider = QStringLiteral("GMAIL");        // "GMAIL", "OUTLOOK", "YAHOO", "ICLOUD", "CUSTOM"

    QString fromEmail;
    QString fromName;
    QString host;
    int port = 465;
    QString security = QStringLiteral("SSL");          // "SSL" (SSL/TLS), "STARTTLS", "NONE"
    QString authMethod = QStringLiteral("PASSWORD");   // "PASSWORD", "OAUTH2", "NONE"

    // Autenticación clásica / Clave de aplicación
    QString username;
    QString password;

    // Autenticación OAuth2 (Google / Gmail)
    QString oauthClientId;
    QString oauthClientSecret;
    QString oauthAccessToken;
    QString oauthRefreshToken;
    qint64 oauthTokenExpiry = 0;

    bool isComplete() const;
    bool hasOAuthSession() const;
};

class Mailer
{
public:
    static SmtpConfig loadConfig();
    static bool saveConfig(const SmtpConfig &config, QString *errorMessage = nullptr);
    static QString normalizePasswordIfAppPassword(const QString &rawPassword, const QString &providerOrHost);

    static bool sendMail(const QString &toEmail,
                         const QString &subject,
                         const QString &bodyText,
                         const QStringList &attachmentPaths = QStringList(),
                         QString *errorMessage = nullptr);

    static bool sendMailWithConfig(const SmtpConfig &config,
                                   const QString &toEmail,
                                   const QString &subject,
                                   const QString &bodyText,
                                   const QStringList &attachmentPaths = QStringList(),
                                   QString *errorMessage = nullptr);

private:
    static QString encodeSecret(const QString &plainText);
    static QString decodeSecret(const QString &encodedText);
};

#endif // CORE_MAILER_H
