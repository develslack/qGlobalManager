#include "loginDialog.h"
#include "core/icons.h"
#include "core/session.h"
#include "core/userCredentials.h"

#include <QApplication>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Global Manager - Iniciar sesión"));
    setModal(true);
    setMinimumWidth(320);
    resize(420, 380);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(22, 20, 22, 20);

    auto *logoLabel = new QLabel(this);
    logoLabel->setAlignment(Qt::AlignCenter);
    logoLabel->setPixmap(QIcon(QStringLiteral(":/icons/qGlobalManager.svg")).pixmap(72, 72));
    mainLayout->addWidget(logoLabel);

    auto *titleLabel = new QLabel(QStringLiteral("<h2 style='color:#1E3A5F; margin:0;'>Global Manager</h2>"), this);
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    auto *subtitleLabel = new QLabel(
        QStringLiteral("Ingresa tus credenciales para acceder al sistema."), this);
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setWordWrap(true);
    subtitleLabel->setStyleSheet(QStringLiteral("color: #52606D;"));
    mainLayout->addWidget(subtitleLabel);

    m_bannerLabel = new QLabel(this);
    m_bannerLabel->setWordWrap(true);
    m_bannerLabel->setStyleSheet(
        QStringLiteral("QLabel { background-color: #FFF3CD; color: #664D03; "
                       "border: 1px solid #FFECB5; padding: 8px; border-radius: 5px; }"));
    m_bannerLabel->hide();
    mainLayout->addWidget(m_bannerLabel);

    auto *formLayout = new QFormLayout();
    formLayout->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_userEdit = new QLineEdit(this);
    m_userEdit->setPlaceholderText(QStringLiteral("usuario@empresa.com"));

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(QStringLiteral("Contraseña"));

    formLayout->addRow(QStringLiteral("Email:"), m_userEdit);
    formLayout->addRow(QStringLiteral("Contraseña:"), m_passwordEdit);
    mainLayout->addLayout(formLayout);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setStyleSheet(QStringLiteral("QLabel { color: #C53030; font-weight: 600; }"));
    m_errorLabel->hide();
    mainLayout->addWidget(m_errorLabel);

    auto *forgotRow = new QHBoxLayout();
    auto *forgotButton = new QPushButton(QStringLiteral("Olvidé mi password"), this);
    forgotButton->setIcon(Icons::get(Icons::Name::Key, QColor(QStringLiteral("#1E3A5F")), 15));
    forgotButton->setProperty("secondary", true);
    forgotRow->addWidget(forgotButton);
    forgotRow->addStretch();
    mainLayout->addLayout(forgotRow);

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    auto *exitButton = new QPushButton(QStringLiteral("Salir"), this);
    exitButton->setIcon(Icons::get(Icons::Name::Close, QColor(QStringLiteral("#1E3A5F")), 16));
    exitButton->setProperty("secondary", true);

    m_loginButton = new QPushButton(QStringLiteral("Ingresar"), this);
    m_loginButton->setIcon(Icons::get(Icons::Name::Login, QColor(QStringLiteral("#FFFFFF")), 16));
    m_loginButton->setDefault(true);

    buttonLayout->addWidget(exitButton);
    buttonLayout->addWidget(m_loginButton);
    mainLayout->addLayout(buttonLayout);

    connect(forgotButton, &QPushButton::clicked, this, &LoginDialog::onForgotPasswordClicked);
    connect(exitButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_loginButton, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);

    m_userEdit->setFocus();
}

void LoginDialog::setInfoBanner(const QString &message)
{
    if (message.trimmed().isEmpty()) {
        m_bannerLabel->hide();
    } else {
        m_bannerLabel->setText(message);
        m_bannerLabel->show();
    }
}

void LoginDialog::onLoginClicked()
{
    m_errorLabel->hide();
    QString errorMessage;
    if (Session::instance().login(m_userEdit->text(), m_passwordEdit->text(), &errorMessage)) {
        m_passwordEdit->clear();
        accept();
        return;
    }

    m_errorLabel->setText(errorMessage);
    m_errorLabel->show();
    m_passwordEdit->clear();
    m_passwordEdit->setFocus();
}

void LoginDialog::onForgotPasswordClicked()
{
    bool ok = false;
    const QString email = QInputDialog::getText(
        this,
        QStringLiteral("Recuperar contraseña"),
        QStringLiteral("Ingresa tu email registrado para enviarte un nuevo archivo de acceso:"),
        QLineEdit::Normal,
        m_userEdit->text().trimmed(),
        &ok).trimmed();

    if (!ok || email.isEmpty()) {
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString feedback;
    const bool resetOk = UserCredentials::resetPasswordByEmail(email, &feedback);
    QApplication::restoreOverrideCursor();

    if (resetOk) {
        m_userEdit->setText(email.toLower());
        m_passwordEdit->clear();
        QMessageBox::information(this, QStringLiteral("Contraseña enviada"), feedback);
    } else {
        QMessageBox::warning(this, QStringLiteral("No se pudo recuperar"), feedback);
    }
}
