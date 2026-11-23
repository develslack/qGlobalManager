#include "mailer.h"
#include "db.h"
#include "oauth2Google.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSslSocket>
#include <QStringList>

namespace {

constexpr int kSmtpTimeoutMs = 15000;

QByteArray maskKey()
{
    return QCryptographicHash::hash(
        QByteArrayLiteral("qGlobalManager::SMTP::v1::SaltKey"),
        QCryptographicHash::Sha256);
}

QString friendlySmtpError(int code, const QString &rawResponse)
{
    if (code == 534 && rawResponse.contains(QStringLiteral("5.7.9"), Qt::CaseInsensitive)) {
        return QStringLiteral(
            "Gmail requiere una \"Contraseña de aplicación\" de 16 letras (no tu contraseña habitual).\n\n"
            "Genera una desde tu cuenta de Google (Seguridad → Contraseñas de aplicaciones) y pégala en el campo Contraseña.\n\n"
            "Detalle técnico: %1").arg(rawResponse);
    }

    if (code == 535 && (rawResponse.contains(QStringLiteral("5.7.8"), Qt::CaseInsensitive) ||
                        rawResponse.contains(QStringLiteral("BadCredentials"), Qt::CaseInsensitive))) {
        return QStringLiteral(
            "El servidor rechazó el correo o la contraseña ingresados.\n\n"
            "Verifica que el email esté bien escrito y, si usas Gmail/Outlook/Yahoo, asegúrate de utilizar "
            "una Contraseña de aplicación vigente.\n\n"
            "Detalle técnico: %1").arg(rawResponse);
    }

    if (code == 535 && rawResponse.contains(QStringLiteral("5.7.139"), Qt::CaseInsensitive)) {
        return QStringLiteral(
            "Microsoft bloqueó el acceso básico o exige una Contraseña de aplicación en esta cuenta de Outlook/Hotmail.\n\n"
            "Detalle técnico: %1").arg(rawResponse);
    }

    return QStringLiteral("El servidor SMTP respondió (%1): %2").arg(code).arg(rawResponse);
}

bool readSmtpResponse(QSslSocket &socket, int *statusCode, QString *fullResponse)
{
    QByteArray buffer;
    while (true) {
        if (!socket.canReadLine()) {
            if (!socket.waitForReadyRead(kSmtpTimeoutMs)) {
                if (fullResponse) {
                    *fullResponse = QStringLiteral("Tiempo de espera agotado esperando respuesta del servidor SMTP.");
                }
                return false;
            }
        }

        while (socket.canReadLine()) {
            const QByteArray line = socket.readLine();
            buffer.append(line);
            if (line.size() >= 4 && line.at(3) == ' ') {
                bool ok = false;
                const int code = QString::fromLatin1(line.left(3)).toInt(&ok);
                if (statusCode) *statusCode = ok ? code : -1;
                if (fullResponse) *fullResponse = QString::fromUtf8(buffer).trimmed();
                return ok;
            }
        }
    }
}

bool sendSmtpCommand(QSslSocket &socket,
                     const QByteArray &command,
                     int expectedCode,
                     QString *errorMessage)
{
    socket.write(command);
    socket.write("\r\n");
    if (!socket.waitForBytesWritten(kSmtpTimeoutMs)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Error enviando comando SMTP: %1").arg(socket.errorString());
        }
        return false;
    }

    int code = -1;
    QString response;
    if (!readSmtpResponse(socket, &code, &response)) {
        if (errorMessage) *errorMessage = response;
        return false;
    }

    if (code != expectedCode) {
        if (errorMessage) {
            *errorMessage = friendlySmtpError(code, response);
        }
        return false;
    }
    return true;
}

bool authenticateXOAuth2(QSslSocket &socket,
                         const QString &accountEmail,
                         const QString &accessToken,
                         QString *errorMessage)
{
    QByteArray rawSasl;
    rawSasl.append("user=");
    rawSasl.append(accountEmail.trimmed().toUtf8());
    rawSasl.append('\x01');
    rawSasl.append("auth=Bearer ");
    rawSasl.append(accessToken.toUtf8());
    rawSasl.append("\x01\x01");

    const QByteArray cmd = "AUTH XOAUTH2 " + rawSasl.toBase64();
    socket.write(cmd);
    socket.write("\r\n");
    if (!socket.waitForBytesWritten(kSmtpTimeoutMs)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Error enviando autenticación OAuth2: %1").arg(socket.errorString());
        }
        return false;
    }

    int code = -1;
    QString response;
    if (!readSmtpResponse(socket, &code, &response)) {
        if (errorMessage) *errorMessage = response;
        return false;
    }

    if (code == 235) {
        return true;
    }

    if (code == 334) {
        socket.write("\r\n");
        socket.waitForBytesWritten(kSmtpTimeoutMs);
        int finalCode = -1;
        QString finalResp;
        readSmtpResponse(socket, &finalCode, &finalResp);
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                "Error de autenticación OAuth2 (%1): %2\n\n"
                "Vuelve a pulsar \"Autorizar con Google (OAuth2)\" en la configuración avanzada.")
                                .arg(finalCode).arg(finalResp);
        }
        return false;
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("El servidor SMTP rechazó OAuth2 (%1): %2").arg(code).arg(response);
    }
    return false;
}

QByteArray buildMimePayload(const SmtpConfig &config,
                            const QString &toEmail,
                            const QString &subject,
                            const QString &bodyText,
                            const QStringList &attachmentPaths)
{
    const QString senderDisplay = config.fromName.trimmed().isEmpty()
                                      ? config.fromEmail
                                      : QStringLiteral("=?UTF-8?B?%1?= <%2>")
                                            .arg(QString::fromLatin1(config.fromName.toUtf8().toBase64()),
                                                 config.fromEmail);

    const QString encodedSubject = QStringLiteral("=?UTF-8?B?%1?=")
                                       .arg(QString::fromLatin1(subject.toUtf8().toBase64()));

    QByteArray msg;
    msg += "From: " + senderDisplay.toUtf8() + "\r\n";
    msg += "To: <" + toEmail.trimmed().toUtf8() + ">\r\n";
    msg += "Subject: " + encodedSubject.toUtf8() + "\r\n";
    msg += "Date: " + QDateTime::currentDateTime().toString(Qt::RFC2822Date).toUtf8() + "\r\n";
    msg += "MIME-Version: 1.0\r\n";

    if (attachmentPaths.isEmpty()) {
        msg += "Content-Type: text/plain; charset=UTF-8\r\n";
        msg += "Content-Transfer-Encoding: base64\r\n\r\n";
        msg += bodyText.toUtf8().toBase64() + "\r\n";
    } else {
        const QByteArray boundary = "----qGlobalManagerBoundary_"
                                    + QByteArray::number(QDateTime::currentMSecsSinceEpoch());
        msg += "Content-Type: multipart/mixed; boundary=\"" + boundary + "\"\r\n\r\n";

        msg += "--" + boundary + "\r\n";
        msg += "Content-Type: text/plain; charset=UTF-8\r\n";
        msg += "Content-Transfer-Encoding: base64\r\n\r\n";
        msg += bodyText.toUtf8().toBase64() + "\r\n";

        for (const QString &filePath : attachmentPaths) {
            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly)) {
                continue;
            }
            const QString fileName = QFileInfo(filePath).fileName();
            const QByteArray encodedData = file.readAll().toBase64();

            msg += "--" + boundary + "\r\n";
            msg += "Content-Type: application/octet-stream; name=\"" + fileName.toUtf8() + "\"\r\n";
            msg += "Content-Disposition: attachment; filename=\"" + fileName.toUtf8() + "\"\r\n";
            msg += "Content-Transfer-Encoding: base64\r\n\r\n";

            for (int pos = 0; pos < encodedData.size(); pos += 76) {
                msg += encodedData.mid(pos, 76) + "\r\n";
            }
        }
        msg += "--" + boundary + "--\r\n";
    }

    msg.replace("\r\n.", "\r\n..");
    return msg;
}

} // namespace

bool SmtpConfig::isComplete() const
{
    if (fromEmail.trimmed().isEmpty() || host.trimmed().isEmpty() || port <= 0) {
        return false;
    }
    if (authMethod.compare(QStringLiteral("OAUTH2"), Qt::CaseInsensitive) == 0) {
        return !oauthClientId.trimmed().isEmpty() && hasOAuthSession();
    }
    if (authMethod.compare(QStringLiteral("PASSWORD"), Qt::CaseInsensitive) == 0) {
        return !password.isEmpty();
    }
    return true;
}

bool SmtpConfig::hasOAuthSession() const
{
    return !oauthRefreshToken.trimmed().isEmpty() || !oauthAccessToken.trimmed().isEmpty();
}

QString Mailer::normalizePasswordIfAppPassword(const QString &rawPassword, const QString &providerOrHost)
{
    const QString trimmed = rawPassword.trimmed();
    // Si coincide con formato de clave de aplicación de 16 caracteres en 4 bloques (ej: "abcd efgh ijkl mnop")
    static const QRegularExpression appPassRx(
        QStringLiteral(R"(^[A-Za-z0-9]{4}\s+[A-Za-z0-9]{4}\s+[A-Za-z0-9]{4}\s+[A-Za-z0-9]{4}$)"));
    if (appPassRx.match(trimmed).hasMatch()) {
        QString compact = trimmed;
        compact.remove(QRegularExpression(QStringLiteral(R"(\s+)")));
        return compact;
    }

    // En Gmail (siempre son claves de aplicación sin espacios), limpiar espacios accidentales
    if (providerOrHost.contains(QStringLiteral("GMAIL"), Qt::CaseInsensitive)) {
        QString compact = trimmed;
        compact.remove(QRegularExpression(QStringLiteral(R"(\s+)")));
        return compact;
    }

    return rawPassword;
}

QString Mailer::encodeSecret(const QString &plainText)
{
    if (plainText.isEmpty()) {
        return QStringLiteral("");
    }
    const QByteArray key = maskKey();
    QByteArray data = plainText.toUtf8();
    for (int i = 0; i < data.size(); ++i) {
        data[i] = static_cast<char>(data.at(i) ^ key.at(i % key.size()));
    }
    return QStringLiteral("enc1:") + QString::fromLatin1(data.toBase64());
}

QString Mailer::decodeSecret(const QString &encodedText)
{
    if (!encodedText.startsWith(QStringLiteral("enc1:"))) {
        return encodedText;
    }
    const QByteArray key = maskKey();
    QByteArray data = QByteArray::fromBase64(encodedText.mid(5).toLatin1());
    for (int i = 0; i < data.size(); ++i) {
        data[i] = static_cast<char>(data.at(i) ^ key.at(i % key.size()));
    }
    return QString::fromUtf8(data);
}

SmtpConfig Mailer::loadConfig()
{
    const Database &db = Database::instance();
    SmtpConfig cfg;
    cfg.mode              = db.getSetting(QStringLiteral("smtp.mode"), QStringLiteral("BASIC"));
    cfg.provider          = db.getSetting(QStringLiteral("smtp.provider"), QStringLiteral("GMAIL"));
    cfg.fromEmail         = db.getSetting(QStringLiteral("smtp.from_email"));
    cfg.fromName          = db.getSetting(QStringLiteral("smtp.from_name"), QStringLiteral("Global Manager"));
    cfg.host              = db.getSetting(QStringLiteral("smtp.host"));
    cfg.port              = db.getSetting(QStringLiteral("smtp.port"), QStringLiteral("465")).toInt();
    cfg.security          = db.getSetting(QStringLiteral("smtp.security"), QStringLiteral("SSL"));
    cfg.authMethod        = db.getSetting(QStringLiteral("smtp.auth_method"), QStringLiteral("PASSWORD"));
    cfg.username          = db.getSetting(QStringLiteral("smtp.username"));
    cfg.password          = decodeSecret(db.getSetting(QStringLiteral("smtp.password")));
    cfg.oauthClientId     = db.getSetting(QStringLiteral("smtp.oauth_client_id"));
    cfg.oauthClientSecret = decodeSecret(db.getSetting(QStringLiteral("smtp.oauth_client_secret")));
    cfg.oauthAccessToken  = decodeSecret(db.getSetting(QStringLiteral("smtp.oauth_access_token")));
    cfg.oauthRefreshToken = decodeSecret(db.getSetting(QStringLiteral("smtp.oauth_refresh_token")));
    cfg.oauthTokenExpiry  = db.getSetting(QStringLiteral("smtp.oauth_token_expiry"), QStringLiteral("0")).toLongLong();
    return cfg;
}

bool Mailer::saveConfig(const SmtpConfig &config, QString *errorMessage)
{
    const QString cleanPass = normalizePasswordIfAppPassword(
        config.password,
        config.provider + QLatin1Char(' ') + config.host);

    Database &db = Database::instance();
    const bool ok =
        db.setSetting(QStringLiteral("smtp.mode"),                config.mode.trimmed(), errorMessage) &&
        db.setSetting(QStringLiteral("smtp.provider"),            config.provider.trimmed(), errorMessage) &&
        db.setSetting(QStringLiteral("smtp.from_email"),          config.fromEmail.trimmed(), errorMessage) &&
        db.setSetting(QStringLiteral("smtp.from_name"),           config.fromName.trimmed(), errorMessage) &&
        db.setSetting(QStringLiteral("smtp.host"),                config.host.trimmed(), errorMessage) &&
        db.setSetting(QStringLiteral("smtp.port"),                QString::number(config.port), errorMessage) &&
        db.setSetting(QStringLiteral("smtp.security"),            config.security.trimmed(), errorMessage) &&
        db.setSetting(QStringLiteral("smtp.auth_method"),         config.authMethod.trimmed(), errorMessage) &&
        db.setSetting(QStringLiteral("smtp.username"),            config.username.trimmed(), errorMessage) &&
        db.setSetting(QStringLiteral("smtp.password"),            encodeSecret(cleanPass), errorMessage) &&
        db.setSetting(QStringLiteral("smtp.oauth_client_id"),     config.oauthClientId.trimmed(), errorMessage) &&
        db.setSetting(QStringLiteral("smtp.oauth_client_secret"), encodeSecret(config.oauthClientSecret.trimmed()), errorMessage) &&
        db.setSetting(QStringLiteral("smtp.oauth_access_token"),  encodeSecret(config.oauthAccessToken), errorMessage) &&
        db.setSetting(QStringLiteral("smtp.oauth_refresh_token"), encodeSecret(config.oauthRefreshToken), errorMessage) &&
        db.setSetting(QStringLiteral("smtp.oauth_token_expiry"),  QString::number(config.oauthTokenExpiry), errorMessage);
    return ok;
}

bool Mailer::sendMail(const QString &toEmail,
                      const QString &subject,
                      const QString &bodyText,
                      const QStringList &attachmentPaths,
                      QString *errorMessage)
{
    const SmtpConfig cfg = loadConfig();
    return sendMailWithConfig(cfg, toEmail, subject, bodyText, attachmentPaths, errorMessage);
}

bool Mailer::sendMailWithConfig(const SmtpConfig &config,
                                const QString &toEmail,
                                const QString &subject,
                                const QString &bodyText,
                                const QStringList &attachmentPaths,
                                QString *errorMessage)
{
    if (!config.isComplete()) {
        if (errorMessage) {
            if (config.authMethod.compare(QStringLiteral("OAUTH2"), Qt::CaseInsensitive) == 0 && !config.hasOAuthSession()) {
                *errorMessage = QStringLiteral("Aún no autorizaste la cuenta de Google con OAuth2 en la configuración avanzada.");
            } else {
                *errorMessage = QStringLiteral("El correo corporativo no está configurado completamente en el Panel de Control.");
            }
        }
        return false;
    }

    if (toEmail.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("El correo de destino no puede estar vacío.");
        }
        return false;
    }

    QString activeAccessToken = config.oauthAccessToken;
    const QString authMethod = config.authMethod.toUpper();
    const QString effectivePassword = normalizePasswordIfAppPassword(
        config.password,
        config.provider + QLatin1Char(' ') + config.host);

    if (authMethod == QStringLiteral("OAUTH2")) {
        const qint64 nowEpoch = QDateTime::currentSecsSinceEpoch();
        if (activeAccessToken.isEmpty() || nowEpoch >= config.oauthTokenExpiry) {
            OAuth2Tokens refreshed;
            if (!OAuth2Google::refreshAccessToken(config.oauthClientId,
                                                  config.oauthClientSecret,
                                                  config.oauthRefreshToken,
                                                  &refreshed,
                                                  errorMessage)) {
                return false;
            }
            activeAccessToken = refreshed.accessToken;
            Database::instance().setSetting(QStringLiteral("smtp.oauth_access_token"), encodeSecret(refreshed.accessToken));
            Database::instance().setSetting(QStringLiteral("smtp.oauth_token_expiry"), QString::number(refreshed.expiresAtEpoch));
        }
    }

    QSslSocket socket;
    const QString sec = config.security.toUpper();

    if (sec == QStringLiteral("SSL") || sec == QStringLiteral("SSL/TLS")) {
        socket.connectToHostEncrypted(config.host.trimmed(), static_cast<quint16>(config.port));
        if (!socket.waitForEncrypted(kSmtpTimeoutMs)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("No se pudo conectar por SSL/TLS a %1:%2 (%3)")
                                    .arg(config.host).arg(config.port).arg(socket.errorString());
            }
            return false;
        }
    } else {
        socket.connectToHost(config.host.trimmed(), static_cast<quint16>(config.port));
        if (!socket.waitForConnected(kSmtpTimeoutMs)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("No se pudo conectar a %1:%2 (%3)")
                                    .arg(config.host).arg(config.port).arg(socket.errorString());
            }
            return false;
        }
    }

    int code = -1;
    QString response;
    if (!readSmtpResponse(socket, &code, &response) || code != 220) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Saludo SMTP inesperado (%1): %2").arg(code).arg(response);
        }
        return false;
    }

    if (!sendSmtpCommand(socket, "EHLO globalmanager.local", 250, errorMessage)) {
        return false;
    }

    if (sec == QStringLiteral("STARTTLS")) {
        if (!sendSmtpCommand(socket, "STARTTLS", 220, errorMessage)) {
            return false;
        }
        socket.startClientEncryption();
        if (!socket.waitForEncrypted(kSmtpTimeoutMs)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Error negociando STARTTLS con el servidor SMTP: %1")
                                    .arg(socket.errorString());
            }
            return false;
        }
        if (!sendSmtpCommand(socket, "EHLO globalmanager.local", 250, errorMessage)) {
            return false;
        }
    }

    if (authMethod == QStringLiteral("OAUTH2")) {
        const QString authAccount = config.username.trimmed().isEmpty()
                                        ? config.fromEmail.trimmed()
                                        : config.username.trimmed();
        if (!authenticateXOAuth2(socket, authAccount, activeAccessToken, errorMessage)) {
            return false;
        }
    } else if (authMethod == QStringLiteral("PASSWORD")) {
        const QString authUser = config.username.trimmed().isEmpty()
                                     ? config.fromEmail.trimmed()
                                     : config.username.trimmed();
        if (!sendSmtpCommand(socket, "AUTH LOGIN", 334, errorMessage)) return false;
        if (!sendSmtpCommand(socket, authUser.toUtf8().toBase64(), 334, errorMessage)) return false;
        if (!sendSmtpCommand(socket, effectivePassword.toUtf8().toBase64(), 235, errorMessage)) return false;
    }

    const QByteArray mailFrom = "MAIL FROM:<" + config.fromEmail.trimmed().toUtf8() + ">";
    const QByteArray rcptTo   = "RCPT TO:<" + toEmail.trimmed().toUtf8() + ">";

    if (!sendSmtpCommand(socket, mailFrom, 250, errorMessage)) return false;
    if (!sendSmtpCommand(socket, rcptTo, 250, errorMessage)) return false;
    if (!sendSmtpCommand(socket, "DATA", 354, errorMessage)) return false;

    const QByteArray payload = buildMimePayload(config, toEmail, subject, bodyText, attachmentPaths);
    socket.write(payload);
    socket.write("\r\n.\r\n");
    if (!socket.waitForBytesWritten(kSmtpTimeoutMs)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Error enviando contenido del correo: %1").arg(socket.errorString());
        }
        return false;
    }

    if (!readSmtpResponse(socket, &code, &response) || code != 250) {
        if (errorMessage) {
            *errorMessage = friendlySmtpError(code, response);
        }
        return false;
    }

    sendSmtpCommand(socket, "QUIT", 221, nullptr);
    socket.disconnectFromHost();
    return true;
}
