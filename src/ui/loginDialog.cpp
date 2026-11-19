#include "loginDialog.h"
#include "core/session.h"
#include "core/icons.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QIcon>
#include <QPixmap>


LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Global Manager - Iniciar sesión"));
    setModal(true);
    setMinimumWidth(300); resize(400, 340);


    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    auto *logoLabel = new QLabel(this);
    logoLabel->setAlignment(Qt::AlignCenter);
    logoLabel->setPixmap(QIcon(QStringLiteral(":/icons/qGlobalManager.svg")).pixmap(72, 72));
    mainLayout->addWidget(logoLabel);

    auto *subtitleLabel = new QLabel(
        QStringLiteral("Ingresa tus credenciales para acceder al sistema."), this);
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setWordWrap(true);
    mainLayout->addWidget(subtitleLabel);

    m_bannerLabel = new QLabel(this);
    m_bannerLabel->setWordWrap(true);
    m_bannerLabel->setStyleSheet(
        QStringLiteral("QLabel { background-color: #fff3cd; color: #664d03; "
                       "border: 1px solid #ffecb5; padding: 8px; border-radius: 4px; }"));
    m_bannerLabel->hide();
    mainLayout->addWidget(m_bannerLabel);

    auto *formLayout = new QFormLayout();
    m_userEdit = new QLineEdit(this);
    m_userEdit->setPlaceholderText(QStringLiteral("admin@globalmanager.local"));

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(QStringLiteral("Contraseña"));

    formLayout->addRow(QStringLiteral("Email:"), m_userEdit);
    formLayout->addRow(QStringLiteral("Contraseña:"), m_passwordEdit);

    mainLayout->addLayout(formLayout);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setStyleSheet(QStringLiteral("QLabel { color: #b02a37; font-weight: bold; }"));
    m_errorLabel->hide();
    mainLayout->addWidget(m_errorLabel);

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
