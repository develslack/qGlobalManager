#include "changePasswordDialog.h"
#include "core/db.h"
#include "core/icons.h"
#include "core/session.h"
#include "core/userCredentials.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlError>
#include <QSqlQuery>
#include <QVBoxLayout>
#include <QVariant>

ChangePasswordDialog::ChangePasswordDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Global Manager - Cambio obligatorio de contraseña"));
    setModal(true);
    setMinimumWidth(340);
    resize(440, 310);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 18, 20, 18);

    auto *titleLabel = new QLabel(
        QStringLiteral("<h3 style='color:#1E3A5F; margin:0;'>Actualiza tu contraseña</h3>"), this);
    mainLayout->addWidget(titleLabel);

    auto *infoLabel = new QLabel(
        QStringLiteral("Estás ingresando con una contraseña temporal. Por tu seguridad, ingresa tu "
                       "<b>contraseña actual</b> y define una nueva contraseña personal para continuar."),
        this);
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet(QStringLiteral("color: #52606D;"));
    mainLayout->addWidget(infoLabel);

    auto *form = new QFormLayout();
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_currentEdit = new QLineEdit(this);
    m_currentEdit->setEchoMode(QLineEdit::Password);
    m_currentEdit->setPlaceholderText(QStringLiteral("Contraseña temporal actual"));

    m_newEdit = new QLineEdit(this);
    m_newEdit->setEchoMode(QLineEdit::Password);
    m_newEdit->setPlaceholderText(QStringLiteral("Mín. 8 caracteres, mayúscula, minúscula y número"));

    m_confirmEdit = new QLineEdit(this);
    m_confirmEdit->setEchoMode(QLineEdit::Password);
    m_confirmEdit->setPlaceholderText(QStringLiteral("Repite la nueva contraseña"));

    form->addRow(QStringLiteral("Password actual:"), m_currentEdit);
    form->addRow(QStringLiteral("Nuevo password:"), m_newEdit);
    form->addRow(QStringLiteral("Confirmar nuevo:"), m_confirmEdit);
    mainLayout->addLayout(form);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setStyleSheet(QStringLiteral("color: #C53030; font-weight: 600;"));
    m_errorLabel->hide();
    mainLayout->addWidget(m_errorLabel);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();

    auto *cancelBtn = new QPushButton(QStringLiteral("Cancelar y salir"), this);
    cancelBtn->setIcon(Icons::get(Icons::Name::Logout, QColor(QStringLiteral("#1E3A5F")), 15));
    cancelBtn->setProperty("secondary", true);

    auto *saveBtn = new QPushButton(QStringLiteral("Cambiar contraseña"), this);
    saveBtn->setIcon(Icons::get(Icons::Name::Key, QColor(QStringLiteral("#FFFFFF")), 15));
    saveBtn->setDefault(true);

    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(saveBtn);
    mainLayout->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, this, &ChangePasswordDialog::onSaveClicked);

    m_currentEdit->setFocus();
}

void ChangePasswordDialog::onSaveClicked()
{
    m_errorLabel->hide();

    const QString currentPass = m_currentEdit->text();
    const QString newPass = m_newEdit->text();
    const QString confirmPass = m_confirmEdit->text();

    if (currentPass.isEmpty() || newPass.isEmpty() || confirmPass.isEmpty()) {
        m_errorLabel->setText(QStringLiteral("Completa todos los campos."));
        m_errorLabel->show();
        return;
    }

    if (newPass != confirmPass) {
        m_errorLabel->setText(QStringLiteral("La nueva contraseña y su confirmación no coinciden."));
        m_errorLabel->show();
        return;
    }

    if (newPass == currentPass) {
        m_errorLabel->setText(QStringLiteral("La nueva contraseña debe ser diferente a la actual."));
        m_errorLabel->show();
        return;
    }

    QString strengthReason;
    if (!UserCredentials::isStrongPassword(newPass, &strengthReason)) {
        m_errorLabel->setText(strengthReason);
        m_errorLabel->show();
        return;
    }

    const UserSessionInfo &user = Session::instance().currentUser();

    QSqlQuery checkQ(Database::instance().connection());
    checkQ.prepare(QStringLiteral("SELECT password_hash FROM users WHERE id = :id LIMIT 1;"));
    checkQ.bindValue(QStringLiteral(":id"), user.id);

    if (!checkQ.exec() || !checkQ.next()) {
        m_errorLabel->setText(QStringLiteral("No se pudo verificar el usuario activo."));
        m_errorLabel->show();
        return;
    }

    if (!Database::verifyPassword(currentPass, checkQ.value(0).toString())) {
        m_errorLabel->setText(QStringLiteral("La contraseña actual ingresada es incorrecta."));
        m_errorLabel->show();
        m_currentEdit->clear();
        m_currentEdit->setFocus();
        return;
    }

    QSqlQuery updQ(Database::instance().connection());
    updQ.prepare(QStringLiteral(
        "UPDATE users SET password_hash = :hash, must_change_password = 0 WHERE id = :id;"));
    updQ.bindValue(QStringLiteral(":hash"), Database::hashPassword(newPass));
    updQ.bindValue(QStringLiteral(":id"), user.id);

    if (!updQ.exec()) {
        m_errorLabel->setText(QStringLiteral("Error al guardar la nueva contraseña: %1")
                                  .arg(updQ.lastError().text()));
        m_errorLabel->show();
        return;
    }

    Session::instance().markPasswordChanged();
    Database::instance().logAudit(
        user.email,
        QStringLiteral("session"),
        QStringLiteral("first_login_password_change"),
        QStringLiteral("El usuario actualizó su contraseña temporal"));

    QMessageBox::information(
        this,
        QStringLiteral("Contraseña actualizada"),
        QStringLiteral("Tu contraseña personal se actualizó correctamente. ¡Bienvenido a Global Manager!"));
    accept();
}
