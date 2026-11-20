#include "userCredentials.h"
#include "db.h"
#include "mailer.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTextStream>
#include <QVariant>


// ============================================================================================================================================= //


bool UserCredentials::isValidEmail(const QString &email)
{
    static const QRegularExpression rx(
        QStringLiteral(R"(^[A-Za-z0-9._%+\-]+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,}$)"),
        QRegularExpression::CaseInsensitiveOption);
    return rx.match(email.trimmed()).hasMatch();
}


// ============================================================================================================================================= //


bool UserCredentials::isStrongPassword(const QString &password, QString *reasonOut)
{
    const bool hasUpper = password.contains(QRegularExpression(QStringLiteral("[A-Z]")));
    const bool hasLower = password.contains(QRegularExpression(QStringLiteral("[a-z]")));
    const bool hasDigit = password.contains(QRegularExpression(QStringLiteral("[0-9]")));

    if (password.size() < 8 || !hasUpper || !hasLower || !hasDigit) {
        if (reasonOut) {
            *reasonOut = QStringLiteral(
                "La contraseña debe tener al menos 8 caracteres, incluir mayúsculas, "
                "minúsculas y al menos un número.");
        }
        return false;
    }
    return true;
}


// ============================================================================================================================================= //


QString UserCredentials::sanitizeFileName(const QString &name)
{
    QString clean = name.trimmed().normalized(QString::NormalizationForm_KD);
    QString ascii;
    ascii.reserve(clean.size());
    for (const QChar &ch : clean) {
        if (ch.category() == QChar::Mark_NonSpacing) {
            continue;
        }
        if (ch.isLetterOrNumber()) {
            ascii.append(ch);
        } else if (ch.isSpace() || ch == QLatin1Char('_') || ch == QLatin1Char('-')) {
            if (!ascii.isEmpty() && ascii.back() != QLatin1Char('_')) {
                ascii.append(QLatin1Char('_'));
            }
        }
    }
    while (ascii.endsWith(QLatin1Char('_'))) {
        ascii.chop(1);
    }
    return ascii.isEmpty() ? QStringLiteral("usuario") : ascii;
}


// ============================================================================================================================================= //


QString UserCredentials::resolveTempCredentialsDir()
{
#ifdef APP_DEBUG_BUILD
    const QString dir = QCoreApplication::applicationDirPath() + QStringLiteral("/exports/credenciales");
#else
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QCoreApplication::applicationDirPath();
    }
    const QString dir = base + QStringLiteral("/exports/credenciales");
#endif
    QDir().mkpath(dir);
    return dir;
}


// ============================================================================================================================================= //


CredentialDeliveryResult UserCredentials::deliverTemporaryPassword(
    const QString &fullName,
    const QString &email,
    const QString &plainPassword,
    const QString &subject,
    const QString &introLine,
    bool keepLocalFileOnFailure)
{
    CredentialDeliveryResult result;

    const QString companyName = Database::instance().getSetting(
        QStringLiteral("company.legal_name"), QStringLiteral("Global Manager"));

    const QString dirPath = resolveTempCredentialsDir();
    const QString fileName = sanitizeFileName(fullName) + QStringLiteral(".txt");
    const QString fullPath = QDir(dirPath).filePath(fileName);

    QFile file(fullPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        result.errorMessage = QStringLiteral("No se pudo generar el archivo de credenciales en:\n%1").arg(fullPath);
        return result;
    }

    QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
#endif
    out << "============================================================\n";
    out << "  GLOBAL MANAGER - CREDENCIALES DE ACCESO\n";
    out << "============================================================\n\n";
    out << "Establecimiento : " << companyName << "\n";
    out << "Nombre completo : " << fullName << "\n";
    out << "Usuario (Email) : " << email << "\n";
    out << "Password temp.  : " << plainPassword << "\n";
    out << "Fecha emisión   : " << QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yyyy HH:mm:ss")) << "\n\n";
    out << "IMPORTANTE:\n";
    out << "- Al iniciar sesión por primera vez, el sistema te solicitará\n";
    out << "  esta contraseña temporal y te pedirá definir una nueva clave personal.\n";
    out << "- Conserva tus credenciales en un lugar seguro y elimina este archivo\n";
    out << "  una vez que hayas cambiado tu contraseña.\n";
    file.close();

    const QString body = QStringLiteral(
        "Hola %1,\n\n"
        "%2\n\n"
        "Adjuntamos el archivo \"%3\" con tu usuario (email) y contraseña temporal de ingreso.\n"
        "Por seguridad, al acceder al sistema deberás cambiar esta contraseña por una personal.\n\n"
        "Saludos,\n%4")
                             .arg(fullName, introLine, fileName, companyName);

    QString mailErr;
    if (Mailer::sendMail(email, subject, body, {fullPath}, &mailErr)) {
        result.emailSent = true;
        result.txtDeleted = QFile::remove(fullPath);
        return result;
    }

    result.emailSent = false;
    result.errorMessage = mailErr;

    if (keepLocalFileOnFailure) {
        result.txtPath = fullPath;
        result.txtDeleted = false;
    } else {
        result.txtDeleted = QFile::remove(fullPath);
    }

    return result;
}


// ============================================================================================================================================= //


bool UserCredentials::resetPasswordByEmail(const QString &email, QString *feedbackMessage)
{
    const QString cleanEmail = email.trimmed().toLower();
    if (!isValidEmail(cleanEmail)) {
        if (feedbackMessage) {
            *feedbackMessage = QStringLiteral("Ingresa un correo electrónico válido.");
        }
        return false;
    }

    if (!Mailer::loadConfig().isComplete()) {
        if (feedbackMessage) {
            *feedbackMessage = QStringLiteral(
                "El servicio de correo corporativo (SMTP) aún no está configurado.\n"
                "Solicita al administrador que restablezca tu contraseña.");
        }
        return false;
    }

    QSqlQuery query(Database::instance().connection());
    query.prepare(QStringLiteral(
        "SELECT u.id, u.full_name, u.is_active, r.is_system "
        "FROM users u "
        "INNER JOIN roles r ON r.id = u.role_id "
        "WHERE u.email = :email LIMIT 1;"));
    query.bindValue(QStringLiteral(":email"), cleanEmail);

    if (!query.exec() || !query.next()) {
        if (feedbackMessage) {
            *feedbackMessage = QStringLiteral("No existe ningún usuario registrado con el email \"%1\".").arg(cleanEmail);
        }
        return false;
    }

    const int userId = query.value(0).toInt();
    const QString fullName = query.value(1).toString();
    const bool isActive = (query.value(2).toInt() == 1);
    const bool isSystemAdmin = (query.value(3).toInt() == 1);

    if (!isActive) {
        if (feedbackMessage) {
            *feedbackMessage = QStringLiteral("Tu cuenta se encuentra bloqueada. Contacta al administrador.");
        }
        return false;
    }

    if (isSystemAdmin) {
        if (feedbackMessage) {
            *feedbackMessage = QStringLiteral(
                "Por política de seguridad, la contraseña del Administrador solo puede "
                "modificarse desde el Panel de Control.");
        }
        return false;
    }

    const QString tempPass = Database::generateRandomPassword(12);

    // Primero enviamos el email; el archivo .txt siempre se borra al finalizar
    const CredentialDeliveryResult delivery = deliverTemporaryPassword(
        fullName,
        cleanEmail,
        tempPass,
        QStringLiteral("Global Manager - Recuperación de contraseña"),
        QStringLiteral("Recibimos una solicitud de recuperación de contraseña para tu cuenta en Global Manager."),
        false);

    if (!delivery.emailSent) {
        if (feedbackMessage) {
            *feedbackMessage = QStringLiteral(
                "No se pudo enviar el correo de recuperación, por lo que tu contraseña no fue modificada.\n\nDetalle: %1")
                                   .arg(delivery.errorMessage);
        }
        return false;
    }

    QSqlQuery updQ(Database::instance().connection());
    updQ.prepare(QStringLiteral(
        "UPDATE users SET password_hash = :hash, must_change_password = 1 WHERE id = :id;"));
    updQ.bindValue(QStringLiteral(":hash"), Database::hashPassword(tempPass));
    updQ.bindValue(QStringLiteral(":id"), userId);

    if (!updQ.exec()) {
        if (feedbackMessage) {
            *feedbackMessage = QStringLiteral("Error actualizando contraseña en la base de datos: %1")
                                   .arg(updQ.lastError().text());
        }
        return false;
    }

    Database::instance().logAudit(
        cleanEmail,
        QStringLiteral("session"),
        QStringLiteral("forgot_password_reset"),
        QStringLiteral("Nueva contraseña temporal enviada por email"));

    if (feedbackMessage) {
        *feedbackMessage = QStringLiteral(
            "Se generó una nueva contraseña temporal y se envió el archivo de acceso a \"%1\".\n"
            "Revisa tu casilla de correo para volver a ingresar.").arg(cleanEmail);
    }
    return true;
}



// ============================================================================================================================================= //
