#ifndef CORE_USERCREDENTIALS_H
#define CORE_USERCREDENTIALS_H

#include <QString>

struct CredentialDeliveryResult
{
    bool emailSent = false;
    bool txtDeleted = false;
    QString txtPath;      // Solo poblado si el email falló y se conservó copia para el admin
    QString errorMessage;
};

class UserCredentials
{
public:
    static bool isValidEmail(const QString &email);
    static bool isStrongPassword(const QString &password, QString *reasonOut = nullptr);

    // Genera <Nombre_Usuario>.txt, lo envía por SMTP y lo elimina si el envío fue exitoso.
    // Si keepLocalFileOnFailure == false (ej: "Olvidé mi password"), el .txt se borra siempre.
    static CredentialDeliveryResult deliverTemporaryPassword(
        const QString &fullName,
        const QString &email,
        const QString &plainPassword,
        const QString &subject,
        const QString &introLine,
        bool keepLocalFileOnFailure = true);

    // Flujo autoservicio desde Login ("Olvidé mi password")
    static bool resetPasswordByEmail(const QString &email, QString *feedbackMessage);

private:
    static QString sanitizeFileName(const QString &name);
    static QString resolveTempCredentialsDir();
};

#endif // CORE_USERCREDENTIALS_H
