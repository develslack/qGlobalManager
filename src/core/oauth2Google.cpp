#include "oauth2Google.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QEventLoop>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressDialog>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QWidget>

namespace {
constexpr int kAuthTimeoutMs = 180000; // 3 minutos para que el usuario complete el login en Google
constexpr int kHttpTimeoutMs = 20000;
} // namespace

QByteArray OAuth2Google::randomBase64Url(int byteCount)
{
    QByteArray raw(byteCount, Qt::Uninitialized);
    for (int i = 0; i < byteCount; ++i) {
        raw[i] = static_cast<char>(QRandomGenerator::system()->generate() & 0xFF);
    }
    return raw.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

bool OAuth2Google::postTokenEndpoint(const QByteArray &formEncodedBody,
                                     OAuth2Tokens *tokensOut,
                                     QString *errorMessage)
{
    QNetworkAccessManager nam;
    QNetworkRequest req(QUrl(QStringLiteral("https://oauth2.googleapis.com/token")));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));

    QNetworkReply *reply = nam.post(req, formEncodedBody);

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
        reply->abort();
        loop.quit();
    });

    timeoutTimer.start(kHttpTimeoutMs);
    loop.exec();
    timeoutTimer.stop();

    const QByteArray rawResp = reply->readAll();
    const QNetworkReply::NetworkError netErr = reply->error();
    const QString netErrStr = reply->errorString();
    reply->deleteLater();

    const QJsonDocument doc = QJsonDocument::fromJson(rawResp);
    const QJsonObject obj = doc.object();

    if (netErr != QNetworkReply::NoError || obj.contains(QStringLiteral("error"))) {
        const QString errCode = obj.value(QStringLiteral("error")).toString();
        const QString errDesc = obj.value(QStringLiteral("error_description")).toString();
        if (errorMessage) {
            if (!errCode.isEmpty()) {
                *errorMessage = QStringLiteral("Google OAuth2 rechazó la solicitud (%1): %2")
                                    .arg(errCode, errDesc);
            } else {
                *errorMessage = QStringLiteral("Error de red comunicándose con Google OAuth2: %1")
                                    .arg(netErrStr);
            }
        }
        return false;
    }

    const QString accessToken = obj.value(QStringLiteral("access_token")).toString();
    if (accessToken.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Google no devolvió un access_token válido.");
        }
        return false;
    }

    if (tokensOut) {
        tokensOut->accessToken = accessToken;
        const QString refresh = obj.value(QStringLiteral("refresh_token")).toString();
        if (!refresh.isEmpty()) {
            tokensOut->refreshToken = refresh;
        }
        const qint64 expiresIn = obj.value(QStringLiteral("expires_in")).toInt(3600);
        // Renovar con 60 segundos de margen antes de vencer
        tokensOut->expiresAtEpoch = QDateTime::currentSecsSinceEpoch() + qMax<qint64>(60, expiresIn - 60);
    }

    return true;
}

bool OAuth2Google::authorizeInteractive(const QString &clientId,
                                        const QString &clientSecret,
                                        const QString &loginHintEmail,
                                        QWidget *parentWidget,
                                        OAuth2Tokens *tokensOut,
                                        QString *errorMessage)
{
    if (clientId.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Debes ingresar el Client ID de Google OAuth2 antes de autorizar.");
        }
        return false;
    }

    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No se pudo abrir el puerto local para recibir la respuesta de Google: %1")
                                .arg(server.errorString());
        }
        return false;
    }

    const QString redirectUri = QStringLiteral("http://127.0.0.1:%1/callback").arg(server.serverPort());
    const QString state = QString::fromLatin1(randomBase64Url(16));
    const QByteArray codeVerifier = randomBase64Url(32);
    const QString codeChallenge = QString::fromLatin1(
        QCryptographicHash::hash(codeVerifier, QCryptographicHash::Sha256)
            .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));

    QUrl authUrl(QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"), clientId.trimmed());
    query.addQueryItem(QStringLiteral("redirect_uri"), redirectUri);
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("scope"), QStringLiteral("https://mail.google.com/"));
    query.addQueryItem(QStringLiteral("access_type"), QStringLiteral("offline"));
    query.addQueryItem(QStringLiteral("prompt"), QStringLiteral("consent"));
    query.addQueryItem(QStringLiteral("state"), state);
    query.addQueryItem(QStringLiteral("code_challenge"), codeChallenge);
    query.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    if (!loginHintEmail.trimmed().isEmpty()) {
        query.addQueryItem(QStringLiteral("login_hint"), loginHintEmail.trimmed());
    }
    authUrl.setQuery(query);

    if (!QDesktopServices::openUrl(authUrl)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No se pudo abrir el navegador predeterminado para iniciar sesión en Google.");
        }
        return false;
    }

    QProgressDialog waitDlg(
        QStringLiteral("Se abrió tu navegador en Google.\n"
                       "Inicia sesión, concede permiso a Global Manager y vuelve a esta ventana..."),
        QStringLiteral("Cancelar"),
        0,
        0,
        parentWidget);
    waitDlg.setWindowTitle(QStringLiteral("Autorización OAuth2 con Google"));
    waitDlg.setWindowModality(Qt::WindowModal);
    waitDlg.setMinimumDuration(0);
    waitDlg.show();

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    QString authCode;
    QString returnedState;
    QString oauthError;
    bool cancelled = false;
    bool timedOut = false;

    QObject::connect(&waitDlg, &QProgressDialog::canceled, &loop, [&]() {
        cancelled = true;
        loop.quit();
    });

    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        loop.quit();
    });

    QObject::connect(&server, &QTcpServer::newConnection, &loop, [&]() {
        QTcpSocket *socket = server.nextPendingConnection();
        if (!socket) {
            return;
        }
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket]() {
            const QByteArray requestData = socket->readAll();
            const int firstLineEnd = requestData.indexOf("\r\n");
            const QByteArray requestLine = firstLineEnd > 0 ? requestData.left(firstLineEnd) : requestData;
            const QList<QByteArray> parts = requestLine.split(' ');

            QByteArray htmlBody;
            if (parts.size() >= 2 && parts.at(1).startsWith("/callback")) {
                const QUrl cbUrl(QStringLiteral("http://127.0.0.1") + QString::fromLatin1(parts.at(1)));
                const QUrlQuery cbQuery(cbUrl);
                authCode = cbQuery.queryItemValue(QStringLiteral("code"));
                returnedState = cbQuery.queryItemValue(QStringLiteral("state"));
                oauthError = cbQuery.queryItemValue(QStringLiteral("error"));

                if (!authCode.isEmpty() && oauthError.isEmpty()) {
                    htmlBody = QByteArrayLiteral(
                        "<!doctype html><html lang='es'><head><meta charset='utf-8'>"
                        "<title>Global Manager</title></head>"
                        "<body style='font-family:sans-serif;text-align:center;padding:48px;background:#F4F7FA;color:#1E3A5F;'>"
                        "<h2>Autorización recibida correctamente</h2>"
                        "<p>Ya puedes cerrar esta pestaña y volver a <b>Global Manager</b>.</p>"
                        "</body></html>");
                } else {
                    htmlBody = QByteArrayLiteral(
                        "<!doctype html><html lang='es'><head><meta charset='utf-8'>"
                        "<title>Global Manager</title></head>"
                        "<body style='font-family:sans-serif;text-align:center;padding:48px;background:#F4F7FA;color:#C53030;'>"
                        "<h2>Autorización cancelada o rechazada</h2>"
                        "<p>Puedes cerrar esta pestaña y volver a <b>Global Manager</b>.</p>"
                        "</body></html>");
                }

                const QByteArray httpResp =
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/html; charset=UTF-8\r\n"
                    "Connection: close\r\n"
                    "Content-Length: " + QByteArray::number(htmlBody.size()) + "\r\n\r\n" + htmlBody;

                socket->write(httpResp);
                socket->flush();
                socket->disconnectFromHost();
                socket->deleteLater();
                loop.quit();
            } else {
                socket->write("HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n");
                socket->disconnectFromHost();
                socket->deleteLater();
            }
        });
    });

    timeoutTimer.start(kAuthTimeoutMs);
    loop.exec();
    timeoutTimer.stop();
    waitDlg.close();
    server.close();

    if (cancelled) {
        if (errorMessage) *errorMessage = QStringLiteral("Autorización con Google cancelada por el usuario.");
        return false;
    }
    if (timedOut) {
        if (errorMessage) *errorMessage = QStringLiteral("Se agotó el tiempo esperando la confirmación en el navegador.");
        return false;
    }
    if (!oauthError.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Google devolvió un error de autorización: %1").arg(oauthError);
        return false;
    }
    if (authCode.isEmpty() || returnedState != state) {
        if (errorMessage) *errorMessage = QStringLiteral("Respuesta de autorización inválida o parámetro de seguridad alterado.");
        return false;
    }

    QUrlQuery tokenParams;
    tokenParams.addQueryItem(QStringLiteral("client_id"), clientId.trimmed());
    if (!clientSecret.trimmed().isEmpty()) {
        tokenParams.addQueryItem(QStringLiteral("client_secret"), clientSecret.trimmed());
    }
    tokenParams.addQueryItem(QStringLiteral("code"), authCode);
    tokenParams.addQueryItem(QStringLiteral("code_verifier"), QString::fromLatin1(codeVerifier));
    tokenParams.addQueryItem(QStringLiteral("redirect_uri"), redirectUri);
    tokenParams.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));

    return postTokenEndpoint(tokenParams.toString(QUrl::FullyEncoded).toUtf8(), tokensOut, errorMessage);
}

bool OAuth2Google::refreshAccessToken(const QString &clientId,
                                      const QString &clientSecret,
                                      const QString &refreshToken,
                                      OAuth2Tokens *tokensOut,
                                      QString *errorMessage)
{
    if (clientId.trimmed().isEmpty() || refreshToken.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Falta el Client ID o el token de actualización OAuth2. Vuelve a autorizar con Google.");
        }
        return false;
    }

    QUrlQuery params;
    params.addQueryItem(QStringLiteral("client_id"), clientId.trimmed());
    if (!clientSecret.trimmed().isEmpty()) {
        params.addQueryItem(QStringLiteral("client_secret"), clientSecret.trimmed());
    }
    params.addQueryItem(QStringLiteral("refresh_token"), refreshToken.trimmed());
    params.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));

    if (tokensOut) {
        tokensOut->refreshToken = refreshToken.trimmed();
    }
    return postTokenEndpoint(params.toString(QUrl::FullyEncoded).toUtf8(), tokensOut, errorMessage);
}
