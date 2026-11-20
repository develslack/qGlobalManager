#include "mailer.h"
#include "db.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
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


// ============================================================================================================================================= //


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
            // Una respuesta SMTP termina cuando el 4to carácter es espacio ('250 OK')
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


// ============================================================================================================================================= //


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
            *errorMessage = QStringLiteral("El servidor SMTP respondió (%1): %2")
                                .arg(code).arg(response);
        }
        return false;
    }
    return true;
}


// ============================================================================================================================================= //


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

        // Parte 1: cuerpo de texto
        msg += "--" + boundary + "\r\n";
        msg += "Content-Type: text/plain; charset=UTF-8\r\n";
        msg += "Content-Transfer-Encoding: base64\r\n\r\n";
        msg += bodyText.toUtf8().toBase64() + "\r\n";

        // Partes adjuntas
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

    // Escapar líneas que comiencen con punto según RFC 5321
    msg.replace("\r\n.", "\r\n..");
    return msg;
}

} // namespace


// ============================================================================================================================================= //


bool SmtpConfig::isComplete() const
{
    return !fromEmail.trimmed().isEmpty() && !host.trimmed().isEmpty() && port > 0;
}


// ============================================================================================================================================= //


QString Mailer::encodeSecret(const QString &plainText)
{
    if (plainText.isEmpty()) {
        return QString();
    }
    const QByteArray key = maskKey();
    QByteArray data = plainText.toUtf8();
    for (int i = 0; i < data.size(); ++i) {
        data[i] = static_cast<char>(data.at(i) ^ key.at(i % key.size()));
    }
    return QStringLiteral("enc1:") + QString::fromLatin1(data.toBase64());
}


// ============================================================================================================================================= //


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


// ============================================================================================================================================= //


SmtpConfig Mailer::loadConfig()
{
    const Database &db = Database::instance();
    SmtpConfig cfg;
    cfg.fromEmail = db.getSetting(QStringLiteral("smtp.from_email"));
    cfg.fromName  = db.getSetting(QStringLiteral("smtp.from_name"), QStringLiteral("Global Manager"));
    cfg.host      = db.getSetting(QStringLiteral("smtp.host"));
    cfg.port      = db.getSetting(QStringLiteral("smtp.port"), QStringLiteral("587")).toInt();
    cfg.security  = db.getSetting(QStringLiteral("smtp.security"), QStringLiteral("STARTTLS"));
    cfg.username  = db.getSetting(QStringLiteral("smtp.username"));
    cfg.password  = decodeSecret(db.getSetting(QStringLiteral("smtp.password")));
    return cfg;
}


// ============================================================================================================================================= //


bool Mailer::saveConfig(const SmtpConfig &config, QString *errorMessage)
{
    Database &db = Database::instance();
    const bool ok =
        db.setSetting(QStringLiteral("smtp.from_email"), config.fromEmail.trimmed(), errorMessage) &&
        db.setSetting(QStringLiteral("smtp.from_name"),  config.fromName.trimmed(), errorMessage) &&
        db.setSetting(QStringLiteral("smtp.host"),       config.host.trimmed(), errorMessage) &&
        db.setSetting(QStringLiteral("smtp.port"),       QString::number(config.port), errorMessage) &&
        db.setSetting(QStringLiteral("smtp.security"),   config.security.trimmed(), errorMessage) &&
        db.setSetting(QStringLiteral("smtp.username"),   config.username.trimmed(), errorMessage) &&
        db.setSetting(QStringLiteral("smtp.password"),   encodeSecret(config.password), errorMessage);
    return ok;
}


// ============================================================================================================================================= //


bool Mailer::sendMail(const QString &toEmail,
                      const QString &subject,
                      const QString &bodyText,
                      const QStringList &attachmentPaths,
                      QString *errorMessage)
{
    const SmtpConfig cfg = loadConfig();
    return sendMailWithConfig(cfg, toEmail, subject, bodyText, attachmentPaths, errorMessage);
}


// ============================================================================================================================================= //


bool Mailer::sendMailWithConfig(const SmtpConfig &config,
                                const QString &toEmail,
                                const QString &subject,
                                const QString &bodyText,
                                const QStringList &attachmentPaths,
                                QString *errorMessage)
{
    if (!config.isComplete()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("El servidor SMTP no está configurado completamente en el Panel de Control.");
        }
        return false;
    }

    if (toEmail.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("El correo de destino no puede estar vacío.");
        }
        return false;
    }

    QSslSocket socket;
    const QString sec = config.security.toUpper();

    if (sec == QStringLiteral("SSL")) {
        socket.connectToHostEncrypted(config.host.trimmed(), static_cast<quint16>(config.port));
        if (!socket.waitForEncrypted(kSmtpTimeoutMs)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("No se pudo conectar por SSL a %1:%2 (%3)")
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
                *errorMessage = QStringLiteral("Error negociando TLS con el servidor SMTP: %1")
                                    .arg(socket.errorString());
            }
            return false;
        }
        if (!sendSmtpCommand(socket, "EHLO globalmanager.local", 250, errorMessage)) {
            return false;
        }
    }

    if (!config.username.trimmed().isEmpty()) {
        if (!sendSmtpCommand(socket, "AUTH LOGIN", 334, errorMessage)) {
            return false;
        }
        if (!sendSmtpCommand(socket, config.username.trimmed().toUtf8().toBase64(), 334, errorMessage)) {
            return false;
        }
        if (!sendSmtpCommand(socket, config.password.toUtf8().toBase64(), 235, errorMessage)) {
            return false;
        }
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
            *errorMessage = QStringLiteral("El correo fue rechazado (%1): %2").arg(code).arg(response);
        }
        return false;
    }

    sendSmtpCommand(socket, "QUIT", 221, nullptr);
    socket.disconnectFromHost();
    return true;
}
