#ifndef CORE_OAUTH2GOOGLE_H
#define CORE_OAUTH2GOOGLE_H

#include <QString>
#include <QtGlobal>

class QWidget;

struct OAuth2Tokens
{
    QString accessToken;
    QString refreshToken;
    qint64 expiresAtEpoch = 0;
};

class OAuth2Google
{
public:
    // Abre el navegador en accounts.google.com para que el usuario autorice explícitamente la app
    static bool authorizeInteractive(const QString &clientId,
                                     const QString &clientSecret,
                                     const QString &loginHintEmail,
                                     QWidget *parentWidget,
                                     OAuth2Tokens *tokensOut,
                                     QString *errorMessage = nullptr);

    // Renueva el access_token en segundo plano usando el refresh_token almacenado
    static bool refreshAccessToken(const QString &clientId,
                                   const QString &clientSecret,
                                   const QString &refreshToken,
                                   OAuth2Tokens *tokensOut,
                                   QString *errorMessage = nullptr);

private:
    static QByteArray randomBase64Url(int byteCount);
    static bool postTokenEndpoint(const QByteArray &formEncodedBody,
                                  OAuth2Tokens *tokensOut,
                                  QString *errorMessage);
};

#endif // CORE_OAUTH2GOOGLE_H
