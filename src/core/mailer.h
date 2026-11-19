#ifndef CORE_MAILER_H
#define CORE_MAILER_H

#include <QString>
#include <QStringList>

struct SmtpConfig
{
    QString fromEmail;
    QString fromName;
    QString host;
    int port = 587;
    QString security = QStringLiteral("STARTTLS"); // "STARTTLS", "SSL", "NONE"
    QString username;
    QString password;

    bool isComplete() const;
};

class Mailer
{
public:
    static SmtpConfig loadConfig();
    static bool saveConfig(const SmtpConfig &config, QString *errorMessage = nullptr);

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
