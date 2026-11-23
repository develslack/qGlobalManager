#include "elevationDialog.h"
#include "core/elevationManager.h"
#include "core/icons.h"
#include "core/session.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

ElevationDialog::ElevationDialog(const QString &targetScope, QWidget *parent)
    : QDialog(parent)
    , m_targetScope(targetScope)
{
    const ElevationPolicy policy = ElevationManager::loadPolicy();
    const UserSessionInfo &user = Session::instance().currentUser();
    const QString scopeTitle = ElevationManager::scopeDisplayName(targetScope);

    setWindowTitle(QStringLiteral("Elevación temporal de privilegios - %1").arg(scopeTitle));
    setModal(true);
    setMinimumWidth(360);
    resize(480, 320);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 18, 20, 18);

    auto *titleLbl = new QLabel(
        QStringLiteral("<h3 style='color:#1E3A5F; margin:0;'>Acceso temporal a %1</h3>")
            .arg(scopeTitle.toHtmlEscaped()),
        this);
    mainLayout->addWidget(titleLbl);

    auto *warnBox = new QLabel(
        QStringLiteral("Usted opera como <b>%1 (%2)</b> y puede generar un <b>token temporal de elevación</b> "
                       "para acceder a esta sección sensible.<br><br>"
                       "• Duración máxima del token: <b>%3 minutos</b>.<br>"
                       "• Todos sus movimientos quedarán sellados en la <b>bitácora de auditoría inmutable</b>.<br><br>"
                       "¿Está seguro de continuar?")
            .arg(user.fullName.toHtmlEscaped(),
                 user.roleName.toHtmlEscaped())
            .arg(policy.durationMinutes),
        this);
    warnBox->setWordWrap(true);
    warnBox->setStyleSheet(
        QStringLiteral("background-color: #FFF3CD; color: #664D03; border: 1px solid #FFECB5; "
                       "padding: 10px; border-radius: 6px;"));
    mainLayout->addWidget(warnBox);

    auto *form = new QFormLayout();
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(QStringLiteral("Reingresa tu contraseña de acceso"));

    m_reasonEdit = new QLineEdit(this);
    m_reasonEdit->setPlaceholderText(QStringLiteral("Ej: Consulta de auditoría, alta de rol, respaldo SQL..."));

    form->addRow(QStringLiteral("Tu contraseña *:"), m_passwordEdit);
    form->addRow(QStringLiteral("Motivo del ingreso *:"), m_reasonEdit);
    mainLayout->addLayout(form);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->setStyleSheet(QStringLiteral("color: #C53030; font-weight: 600;"));
    m_errorLabel->hide();
    mainLayout->addWidget(m_errorLabel);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();

    auto *cancelBtn = new QPushButton(QStringLiteral("Cancelar"), this);
    cancelBtn->setIcon(Icons::get(Icons::Name::Close, QColor(QStringLiteral("#1E3A5F")), 15));
    cancelBtn->setProperty("secondary", true);

    auto *okBtn = new QPushButton(QStringLiteral("Generar token y acceder"), this);
    okBtn->setIcon(Icons::get(Icons::Name::Key, QColor(QStringLiteral("#FFFFFF")), 15));
    okBtn->setDefault(true);

    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    mainLayout->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, this, &ElevationDialog::onConfirmClicked);

    m_passwordEdit->setFocus();
}

void ElevationDialog::onConfirmClicked()
{
    m_errorLabel->hide();

    QString err;
    if (ElevationManager::instance().requestSelfElevation(
            m_targetScope,
            m_passwordEdit->text(),
            m_reasonEdit->text(),
            &err)) {
        m_passwordEdit->clear();
        accept();
        return;
    }

    m_errorLabel->setText(err);
    m_errorLabel->show();
    m_passwordEdit->clear();
    m_passwordEdit->setFocus();
}
