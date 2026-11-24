#include "control_panel.h"
#include "dbAdminWidget.h"
#include "supportTablesWidget.h"

#include "core/db.h"
#include "core/icons.h"
#include "core/licenseToken.h"
#include "core/mailer.h"
#include "core/session.h"
#include "core/oauth2Google.h"
#include "core/bootstrapConfig.h"
#include "core/elevationManager.h"
#include "ui/elevationDialog.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPageLayout>
#include <QPainter>
#include <QPixmap>
#include <QPrinter>
#include <QPrinterInfo>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSpinBox>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QVariant>
#include <QDesktopServices>
#include <QRadioButton>
#include <QUrl>

namespace {

QScrollArea *wrapInScroll(QWidget *content, QWidget *parent)
{
    auto *scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    return scroll;
}


struct MailProviderPreset
{
    const char *id;
    const char *label;
    const char *host;
    int port;
    const char *security;
    const char *helpText;
    const char *helpUrl;
};

const MailProviderPreset kMailPresets[] = {
    {
        "GMAIL",
        "Gmail (Google)",
        "smtp.gmail.com",
        465,
        "SSL",
        "Para enviar correos desde Gmail necesitas una <b>Contraseña de aplicación</b> de 16 letras "
        "(con Verificación en 2 pasos activada). Puedes pegarla con o sin espacios: el sistema la ajusta solo.",
        "https://myaccount.google.com/apppasswords"
    },
    {
        "OUTLOOK",
        "Outlook / Hotmail / Live (Microsoft)",
        "smtp-mail.outlook.com",
        587,
        "STARTTLS",
        "Para Outlook o Hotmail, ingresa tu cuenta Microsoft y una <b>Contraseña de aplicación</b> "
        "generada desde las opciones de seguridad avanzada de tu cuenta.",
        "https://account.live.com/proofs/AppPassword"
    },
    {
        "YAHOO",
        "Yahoo Mail",
        "smtp.mail.yahoo.com",
        465,
        "SSL",
        "Yahoo exige generar una <b>Contraseña de aplicación</b> desde Seguridad de la cuenta "
        "para permitir envíos desde programas externos.",
        "https://login.yahoo.com/myaccount/security/"
    },
    {
        "ICLOUD",
        "iCloud Mail (Apple)",
        "smtp.mail.me.com",
        587,
        "STARTTLS",
        "En iCloud debes generar una <b>Contraseña específica para la app</b> desde tu Apple ID.",
        "https://appleid.apple.com/account/manage"
    }
};

const MailProviderPreset *findPresetById(const QString &providerId)
{
    for (const auto &preset : kMailPresets) {
        if (providerId.compare(QString::fromLatin1(preset.id), Qt::CaseInsensitive) == 0) {
            return &preset;
        }
    }
    return &kMailPresets[0];
}

} // namespace

PanelControlModule::PanelControlModule(QWidget *parent)
    : ModuleBase(parent)
{
    initBanner(
        QStringLiteral("Panel de Control"),
        QStringLiteral("Configuración corporativa del establecimiento, correo SMTP, impresoras, seguridad y tablas soporte."));

    setupUi();
    loadAllSettings();
}

void PanelControlModule::setupUi()
{
    m_tabs = new QTabWidget(this);
    m_tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    const QColor tabIconColor(QStringLiteral("#1E3A5F"));

    m_companyTabIndex = m_tabs->addTab(
    wrapInScroll(buildCompanyTab(), m_tabs),
    Icons::get(Icons::Name::Company, tabIconColor, 16),
    QStringLiteral("Empresa"));

    m_smtpTabIndex = m_tabs->addTab(
        wrapInScroll(buildSmtpTab(), m_tabs),
        Icons::get(Icons::Name::Mail, tabIconColor, 16),
        QStringLiteral("Correo / SMTP"));

    m_printersTabIndex = m_tabs->addTab(
        wrapInScroll(buildPrintersTab(), m_tabs),
        Icons::get(Icons::Name::Printer, tabIconColor, 16),
        QStringLiteral("Impresoras"));

    m_securityTabIndex = m_tabs->addTab(
        wrapInScroll(buildSecurityTab(), m_tabs),
        Icons::get(Icons::Name::Shield, tabIconColor, 16),
        QStringLiteral("Seguridad y Licencia"));

    m_supportTablesWidget = new SupportTablesWidget(m_tabs);
    m_supportTablesTabIndex = m_tabs->addTab(
        m_supportTablesWidget,
        Icons::get(Icons::Name::Table, tabIconColor, 16),
        QStringLiteral("Tablas Soporte"));

    m_backupAuditTabIndex = m_tabs->addTab(
        wrapInScroll(buildBackupAuditTab(), m_tabs),
        Icons::get(Icons::Name::Backup, tabIconColor, 16),
        QStringLiteral("Respaldo y Auditoría"));

    m_dbAdminWidget = new DbAdminWidget(m_tabs);
    m_dbAdminTabIndex = m_tabs->addTab(
        m_dbAdminWidget,
        Icons::get(Icons::Name::Database, tabIconColor, 16),
        QStringLiteral("Base de Datos"));

    connect(m_tabs, &QTabWidget::currentChanged, this, &PanelControlModule::onTabChanged);
    connect(&ElevationManager::instance(), &ElevationManager::elevationChanged,
            this, [this](bool) { applyTabPermissions(); });

    bodyLayout()->addWidget(m_tabs, 1);

    applyTabPermissions();
}

QWidget *PanelControlModule::buildCompanyTab()
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(14);

    auto *group = new QGroupBox(QStringLiteral("Datos Fiscales y del Establecimiento"), page);
    auto *form = new QFormLayout(group);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_companyLegalName = new QLineEdit(group);
    m_companyLegalName->setPlaceholderText(QStringLiteral("Razón social registrada"));

    m_companyTradeName = new QLineEdit(group);
    m_companyTradeName->setPlaceholderText(QStringLiteral("Nombre comercial o de fantasía"));

    m_companyCuit = new QLineEdit(group);
    m_companyCuit->setPlaceholderText(QStringLiteral("Ej: 20-12345678-9"));

    m_companyBusinessType = new QComboBox(group);
    m_companyBusinessType->addItems({QStringLiteral("Comercio"), QStringLiteral("Consultorio")});

    m_companySubRubro = new QLineEdit(group);
    m_companySubRubro->setPlaceholderText(QStringLiteral("Ej: Pet Shop, Repuestos, Odontología, Clínica..."));

    m_companyIvaCombo = new QComboBox(group);
    m_companyAddress = new QLineEdit(group);
    m_companyAddress->setPlaceholderText(QStringLiteral("Calle, altura, localidad"));

    m_companyPhone = new QLineEdit(group);
    m_companyPhone->setPlaceholderText(QStringLiteral("Teléfono / WhatsApp de contacto"));

    m_companyCurrency = new QLineEdit(group);
    m_companyCurrency->setMaximumWidth(100);
    m_companyCurrency->setPlaceholderText(QStringLiteral("$"));

    auto *logoRow = new QHBoxLayout();
    m_companyLogoPath = new QLineEdit(group);
    m_companyLogoPath->setReadOnly(true);
    m_companyLogoPath->setPlaceholderText(QStringLiteral("Opcional: logotipo para comprobantes y reportes"));

    auto *browseLogoBtn = new QPushButton(QStringLiteral("Seleccionar"), group);
    browseLogoBtn->setIcon(Icons::get(Icons::Name::Edit, QColor(QStringLiteral("#1E3A5F")), 15));
    browseLogoBtn->setProperty("secondary", true);

    logoRow->addWidget(m_companyLogoPath, 1);
    logoRow->addWidget(browseLogoBtn);

    m_companyLogoPreview = new QLabel(group);
    m_companyLogoPreview->setFixedSize(72, 72);
    m_companyLogoPreview->setAlignment(Qt::AlignCenter);
    m_companyLogoPreview->setStyleSheet(QStringLiteral("border: 1px dashed #CBD2D9; border-radius: 6px;"));
    m_companyLogoPreview->setText(QStringLiteral("Sin logo"));

    form->addRow(QStringLiteral("Razón Social *:"), m_companyLegalName);
    form->addRow(QStringLiteral("Nombre de Fantasía:"), m_companyTradeName);
    form->addRow(QStringLiteral("CUIT / CUIL *:"), m_companyCuit);
    form->addRow(QStringLiteral("Tipo de Actividad:"), m_companyBusinessType);
    form->addRow(QStringLiteral("Rubro / Especialidad:"), m_companySubRubro);
    form->addRow(QStringLiteral("Condición frente al IVA:"), m_companyIvaCombo);
    form->addRow(QStringLiteral("Domicilio:"), m_companyAddress);
    form->addRow(QStringLiteral("Teléfono:"), m_companyPhone);
    form->addRow(QStringLiteral("Símbolo Moneda:"), m_companyCurrency);
    form->addRow(QStringLiteral("Logotipo del cliente:"), logoRow);
    form->addRow(QStringLiteral("Vista previa:"), m_companyLogoPreview);

    layout->addWidget(group);

    auto *btnRow = new QHBoxLayout();
    auto *saveBtn = new QPushButton(QStringLiteral("Guardar datos de empresa"), page);
    saveBtn->setIcon(Icons::get(Icons::Name::Save, QColor(QStringLiteral("#FFFFFF")), 16));
    btnRow->addWidget(saveBtn);
    btnRow->addStretch();
    layout->addLayout(btnRow);
    layout->addStretch();

    connect(browseLogoBtn, &QPushButton::clicked, this, &PanelControlModule::onSelectCompanyLogo);
    connect(saveBtn, &QPushButton::clicked, this, &PanelControlModule::onSaveCompany);
    return page;
}

QWidget *PanelControlModule::buildSmtpTab()
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(14);

    // =========================================================================
    // 1. Selector de modo + Datos comunes del remitente
    // =========================================================================
    auto *commonGroup = new QGroupBox(QStringLiteral("Cuenta de Correo Institucional"), page);
    auto *commonLayout = new QVBoxLayout(commonGroup);
    commonLayout->setSpacing(10);

    auto *modeRow = new QHBoxLayout();
    m_smtpModeBasicRadio = new QRadioButton(QStringLiteral("Configuración Básica (Guiada: Gmail, Hotmail, Yahoo...)"), commonGroup);
    m_smtpModeAdvancedRadio = new QRadioButton(QStringLiteral("Configuración Avanzada (Servidor propio / TI)"), commonGroup);
    m_smtpModeBasicRadio->setChecked(true);

    modeRow->addWidget(m_smtpModeBasicRadio);
    modeRow->addSpacing(16);
    modeRow->addWidget(m_smtpModeAdvancedRadio);
    modeRow->addStretch();
    commonLayout->addLayout(modeRow);

    auto *commonForm = new QFormLayout();
    commonForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_smtpFromEmail = new QLineEdit(commonGroup);
    m_smtpFromEmail->setPlaceholderText(QStringLiteral("tuempresa@gmail.com o notificaciones@tudominio.com"));

    m_smtpFromName = new QLineEdit(commonGroup);
    m_smtpFromName->setPlaceholderText(QStringLiteral("Nombre visible en los correos (ej: Global Manager - Mi Comercio)"));

    commonForm->addRow(QStringLiteral("Email Institucional (Remitente) *:"), m_smtpFromEmail);
    commonForm->addRow(QStringLiteral("Nombre del Remitente:"), m_smtpFromName);
    commonLayout->addLayout(commonForm);
    layout->addWidget(commonGroup);

    // =========================================================================
    // 2. Bloque MODO BÁSICO (Guiado para usuarios utilitarios)
    // =========================================================================
    m_smtpBasicContainer = new QGroupBox(QStringLiteral("Asistente Rápido de Correo"), page);
    auto *basicLayout = new QVBoxLayout(m_smtpBasicContainer);
    basicLayout->setSpacing(10);

    auto *basicForm = new QFormLayout();
    basicForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_smtpBasicProvider = new QComboBox(m_smtpBasicContainer);
    for (const auto &preset : kMailPresets) {
        m_smtpBasicProvider->addItem(QString::fromUtf8(preset.label), QString::fromLatin1(preset.id));
    }

    auto *basicPassRow = new QHBoxLayout();
    m_smtpBasicPass = new QLineEdit(m_smtpBasicContainer);
    m_smtpBasicPass->setEchoMode(QLineEdit::Password);
    m_smtpBasicPass->setPlaceholderText(QStringLiteral("Pega aquí tu Contraseña de aplicación (16 letras)"));

    m_smtpBasicShowPass = new QCheckBox(QStringLiteral("Mostrar"), m_smtpBasicContainer);
    basicPassRow->addWidget(m_smtpBasicPass, 1);
    basicPassRow->addWidget(m_smtpBasicShowPass);

    basicForm->addRow(QStringLiteral("Servicio de correo *:"), m_smtpBasicProvider);
    basicForm->addRow(QStringLiteral("Contraseña de aplicación *:"), basicPassRow);
    basicLayout->addLayout(basicForm);

    m_smtpBasicHelpLabel = new QLabel(m_smtpBasicContainer);
    m_smtpBasicHelpLabel->setWordWrap(true);
    m_smtpBasicHelpLabel->setStyleSheet(
        QStringLiteral("background-color: #EEF4FA; color: #1E3A5F; border: 1px solid #D9E2EC; "
                       "padding: 10px; border-radius: 6px;"));
    basicLayout->addWidget(m_smtpBasicHelpLabel);

    auto *helpActionRow = new QHBoxLayout();
    m_smtpProviderHelpBtn = new QPushButton(QStringLiteral("Abrir página de Claves de Aplicación"), m_smtpBasicContainer);
    m_smtpProviderHelpBtn->setIcon(Icons::get(Icons::Name::Key, QColor(QStringLiteral("#1E3A5F")), 15));
    m_smtpProviderHelpBtn->setProperty("secondary", true);

    m_smtpBasicSummaryLabel = new QLabel(m_smtpBasicContainer);
    m_smtpBasicSummaryLabel->setWordWrap(true);
    m_smtpBasicSummaryLabel->setStyleSheet(QStringLiteral("color: #52606D; font-size: 9pt;"));

    helpActionRow->addWidget(m_smtpProviderHelpBtn);
    helpActionRow->addSpacing(10);
    helpActionRow->addWidget(m_smtpBasicSummaryLabel, 1);
    basicLayout->addLayout(helpActionRow);

    layout->addWidget(m_smtpBasicContainer);

    // =========================================================================
    // 3. Bloque MODO AVANZADO (Servidor propio / parámetros completos)
    // =========================================================================
    m_smtpAdvancedContainer = new QWidget(page);
    auto *advLayout = new QVBoxLayout(m_smtpAdvancedContainer);
    advLayout->setContentsMargins(0, 0, 0, 0);
    advLayout->setSpacing(12);

    auto *serverGroup = new QGroupBox(QStringLiteral("Parámetros del Servidor SMTP Propio"), m_smtpAdvancedContainer);
    auto *advForm = new QFormLayout(serverGroup);
    advForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_smtpHost = new QLineEdit(serverGroup);
    m_smtpHost->setPlaceholderText(QStringLiteral("Ej: mail.tuempresa.com o smtp.gmail.com"));

    m_smtpSecurity = new QComboBox(serverGroup);
    m_smtpSecurity->addItem(QStringLiteral("SSL/TLS (Puerto 465)"), QStringLiteral("SSL"));
    m_smtpSecurity->addItem(QStringLiteral("STARTTLS (Puerto 587)"), QStringLiteral("STARTTLS"));
    m_smtpSecurity->addItem(QStringLiteral("Sin cifrado (Puerto 25)"), QStringLiteral("NONE"));

    m_smtpPort = new QSpinBox(serverGroup);
    m_smtpPort->setRange(1, 65535);
    m_smtpPort->setValue(465);
    m_smtpPort->setMaximumWidth(120);

    m_smtpAuthMethod = new QComboBox(serverGroup);
    m_smtpAuthMethod->addItem(QStringLiteral("Usuario y Contraseña / Clave de App"), QStringLiteral("PASSWORD"));
    m_smtpAuthMethod->addItem(QStringLiteral("OAuth2 (Google Cloud personalizado)"), QStringLiteral("OAUTH2"));
    m_smtpAuthMethod->addItem(QStringLiteral("Sin autenticación (Relay local)"), QStringLiteral("NONE"));

    advForm->addRow(QStringLiteral("Servidor SMTP (Host) *:"), m_smtpHost);
    advForm->addRow(QStringLiteral("Seguridad:"), m_smtpSecurity);
    advForm->addRow(QStringLiteral("Puerto *:"), m_smtpPort);
    advForm->addRow(QStringLiteral("Método de autenticación:"), m_smtpAuthMethod);
    advLayout->addWidget(serverGroup);

    // Sub-bloque Usuario / Contraseña
    m_smtpPasswordGroup = new QGroupBox(QStringLiteral("Credenciales de Acceso SMTP"), m_smtpAdvancedContainer);
    auto *passForm = new QFormLayout(m_smtpPasswordGroup);
    passForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_smtpUser = new QLineEdit(m_smtpPasswordGroup);
    m_smtpUser->setPlaceholderText(QStringLiteral("Si se deja vacío se usa el Email Institucional"));

    auto *passRow = new QHBoxLayout();
    m_smtpPass = new QLineEdit(m_smtpPasswordGroup);
    m_smtpPass->setEchoMode(QLineEdit::Password);
    m_smtpPass->setPlaceholderText(QStringLiteral("Contraseña de la casilla o Clave de aplicación"));

    m_smtpShowPass = new QCheckBox(QStringLiteral("Mostrar"), m_smtpPasswordGroup);
    passRow->addWidget(m_smtpPass, 1);
    passRow->addWidget(m_smtpShowPass);

    passForm->addRow(QStringLiteral("Usuario SMTP:"), m_smtpUser);
    passForm->addRow(QStringLiteral("Contraseña SMTP:"), passRow);
    advLayout->addWidget(m_smtpPasswordGroup);

    // Sub-bloque OAuth2 (solo si elige OAuth2 en modo avanzado)
    m_smtpOAuthGroup = new QGroupBox(QStringLiteral("Vinculación OAuth2 (Google Cloud)"), m_smtpAdvancedContainer);
    auto *oauthLayout = new QVBoxLayout(m_smtpOAuthGroup);
    oauthLayout->setSpacing(10);

    auto *oauthForm = new QFormLayout();
    oauthForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_oauthClientId = new QLineEdit(m_smtpOAuthGroup);
    m_oauthClientId->setPlaceholderText(QStringLiteral("xxxx.apps.googleusercontent.com"));

    m_oauthClientSecret = new QLineEdit(m_smtpOAuthGroup);
    m_oauthClientSecret->setEchoMode(QLineEdit::Password);
    m_oauthClientSecret->setPlaceholderText(QStringLiteral("Client Secret"));

    m_oauthStatusLabel = new QLabel(QStringLiteral("Pendiente de autorización en Google."), m_smtpOAuthGroup);
    m_oauthStatusLabel->setWordWrap(true);

    oauthForm->addRow(QStringLiteral("Client ID *:"), m_oauthClientId);
    oauthForm->addRow(QStringLiteral("Client Secret:"), m_oauthClientSecret);
    oauthForm->addRow(QStringLiteral("Estado:"), m_oauthStatusLabel);
    oauthLayout->addLayout(oauthForm);

    auto *oauthBtnRow = new QHBoxLayout();
    auto *authGoogleBtn = new QPushButton(QStringLiteral("Autorizar con Google (OAuth2)"), m_smtpOAuthGroup);
    authGoogleBtn->setIcon(Icons::get(Icons::Name::Shield, QColor(QStringLiteral("#FFFFFF")), 16));

    auto *revokeGoogleBtn = new QPushButton(QStringLiteral("Desvincular cuenta"), m_smtpOAuthGroup);
    revokeGoogleBtn->setIcon(Icons::get(Icons::Name::Close, QColor(QStringLiteral("#1E3A5F")), 15));
    revokeGoogleBtn->setProperty("secondary", true);

    oauthBtnRow->addWidget(authGoogleBtn);
    oauthBtnRow->addWidget(revokeGoogleBtn);
    oauthBtnRow->addStretch();
    oauthLayout->addLayout(oauthBtnRow);
    advLayout->addWidget(m_smtpOAuthGroup);

    layout->addWidget(m_smtpAdvancedContainer);

    // =========================================================================
    // 4. Acciones globales (Guardar + Probar envío)
    // =========================================================================
    auto *btnRow = new QHBoxLayout();
    auto *saveBtn = new QPushButton(QStringLiteral("Guardar configuración de correo"), page);
    saveBtn->setIcon(Icons::get(Icons::Name::Save, QColor(QStringLiteral("#FFFFFF")), 16));

    auto *testBtn = new QPushButton(QStringLiteral("Probar envío de correo"), page);
    testBtn->setIcon(Icons::get(Icons::Name::Send, QColor(QStringLiteral("#1E3A5F")), 16));
    testBtn->setProperty("secondary", true);

    btnRow->addWidget(saveBtn);
    btnRow->addWidget(testBtn);
    btnRow->addStretch();
    layout->addLayout(btnRow);
    layout->addStretch();

    // Conexiones
    connect(m_smtpModeBasicRadio, &QRadioButton::toggled, this, &PanelControlModule::onSmtpModeToggled);
    connect(m_smtpBasicProvider, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PanelControlModule::onSmtpBasicProviderChanged);
    connect(m_smtpProviderHelpBtn, &QPushButton::clicked, this, &PanelControlModule::onOpenProviderHelpUrl);
    connect(m_smtpBasicShowPass, &QCheckBox::toggled, this, [this](bool checked) {
        m_smtpBasicPass->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
    });
    connect(m_smtpShowPass, &QCheckBox::toggled, this, [this](bool checked) {
        m_smtpPass->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
    });
    connect(m_smtpSecurity, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PanelControlModule::onSmtpSecurityChanged);
    connect(m_smtpAuthMethod, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PanelControlModule::onSmtpAuthMethodChanged);
    connect(authGoogleBtn, &QPushButton::clicked, this, &PanelControlModule::onAuthorizeGoogleOAuth);
    connect(revokeGoogleBtn, &QPushButton::clicked, this, &PanelControlModule::onClearGoogleOAuth);
    connect(saveBtn, &QPushButton::clicked, this, &PanelControlModule::onSaveSmtp);
    connect(testBtn, &QPushButton::clicked, this, &PanelControlModule::onTestSmtp);

    onSmtpBasicProviderChanged(0);
    onSmtpModeToggled(true);
    return page;
}

QWidget *PanelControlModule::buildPrintersTab()
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(14);

    auto *group = new QGroupBox(QStringLiteral("Impresoras Locales y Compartidas"), page);
    auto *form = new QFormLayout(group);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_mainPrinterCombo = new QComboBox(group);
    m_ticketPrinterCombo = new QComboBox(group);
    m_ticketWidthCombo = new QComboBox(group);
    m_ticketWidthCombo->addItems({
        QStringLiteral("80 mm (Estándar)"),
        QStringLiteral("58 mm (Compacta)"),
        QStringLiteral("A4 / Carta")
    });

    m_printersInfoLabel = new QLabel(group);
    m_printersInfoLabel->setWordWrap(true);
    m_printersInfoLabel->setStyleSheet(QStringLiteral("color: #52606D;"));

    form->addRow(QStringLiteral("Impresora Principal (A4 / Reportes):"), m_mainPrinterCombo);
    form->addRow(QStringLiteral("Impresora de Tickets / Comprobantes:"), m_ticketPrinterCombo);
    form->addRow(QStringLiteral("Formato de Ticket:"), m_ticketWidthCombo);
    form->addRow(QStringLiteral("Estado detectado:"), m_printersInfoLabel);

    layout->addWidget(group);

    auto *btnRow = new QHBoxLayout();
    auto *saveBtn = new QPushButton(QStringLiteral("Guardar impresoras"), page);
    saveBtn->setIcon(Icons::get(Icons::Name::Save, QColor(QStringLiteral("#FFFFFF")), 16));

    auto *refreshBtn = new QPushButton(QStringLiteral("Actualizar lista"), page);
    refreshBtn->setIcon(Icons::get(Icons::Name::Refresh, QColor(QStringLiteral("#1E3A5F")), 16));
    refreshBtn->setProperty("secondary", true);

    auto *testBtn = new QPushButton(QStringLiteral("Imprimir página de prueba"), page);
    testBtn->setIcon(Icons::get(Icons::Name::Printer, QColor(QStringLiteral("#1E3A5F")), 16));
    testBtn->setProperty("secondary", true);

    btnRow->addWidget(saveBtn);
    btnRow->addWidget(refreshBtn);
    btnRow->addWidget(testBtn);
    btnRow->addStretch();
    layout->addLayout(btnRow);
    layout->addStretch();

    connect(saveBtn, &QPushButton::clicked, this, &PanelControlModule::onSavePrinters);
    connect(refreshBtn, &QPushButton::clicked, this, &PanelControlModule::onRefreshPrinters);
    connect(testBtn, &QPushButton::clicked, this, &PanelControlModule::onPrintTestPage);
    return page;
}

QWidget *PanelControlModule::buildSecurityTab()
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(14);

    // Banner de cambio obligatorio en el primer arranque
    m_firstLoginAlert = new QLabel(
        QStringLiteral("<b>Cambio obligatorio de contraseña inicial:</b> Esta es la contraseña "
                       "predeterminada de instalación. Debes definir una nueva contraseña de administrador "
                       "desde este formulario para habilitar el resto de los módulos."),
        page);
    m_firstLoginAlert->setWordWrap(true);
    m_firstLoginAlert->setStyleSheet(
        QStringLiteral("background-color: #FFF3CD; color: #664D03; border: 1px solid #FFECB5; "
                       "padding: 10px; border-radius: 6px;"));
    m_firstLoginAlert->hide();
    layout->addWidget(m_firstLoginAlert);

    // 1. Contraseña exclusiva del administrador
    auto *passGroup = new QGroupBox(QStringLiteral("Contraseña del Administrador (Exclusivo Panel de Control)"), page);
    auto *passForm = new QFormLayout(passGroup);
    passForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_adminCurrentPass = new QLineEdit(passGroup);
    m_adminCurrentPass->setEchoMode(QLineEdit::Password);
    m_adminCurrentPass->setPlaceholderText(QStringLiteral("Contraseña actual del administrador"));

    m_adminNewPass = new QLineEdit(passGroup);
    m_adminNewPass->setEchoMode(QLineEdit::Password);
    m_adminNewPass->setPlaceholderText(QStringLiteral("Mínimo 8 caracteres, mayúscula, minúscula y número"));

    m_adminConfirmPass = new QLineEdit(passGroup);
    m_adminConfirmPass->setEchoMode(QLineEdit::Password);
    m_adminConfirmPass->setPlaceholderText(QStringLiteral("Repite la nueva contraseña"));

    passForm->addRow(QStringLiteral("Contraseña actual:"), m_adminCurrentPass);
    passForm->addRow(QStringLiteral("Nueva contraseña:"), m_adminNewPass);
    passForm->addRow(QStringLiteral("Confirmar nueva:"), m_adminConfirmPass);

    auto *passBtnRow = new QHBoxLayout();
    auto *changePassBtn = new QPushButton(QStringLiteral("Actualizar contraseña del administrador"), passGroup);
    changePassBtn->setIcon(Icons::get(Icons::Name::Key, QColor(QStringLiteral("#FFFFFF")), 16));
    passBtnRow->addWidget(changePassBtn);
    passBtnRow->addStretch();
    passForm->addRow(QString(), passBtnRow);
    layout->addWidget(passGroup);

    // 2. Control de inactividad de sesión
    auto *timerGroup = new QGroupBox(QStringLiteral("Cierre Automático por Inactividad"), page);
    auto *timerForm = new QFormLayout(timerGroup);
    timerForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_idleMinutesSpin = new QSpinBox(timerGroup);
    m_idleMinutesSpin->setRange(1, 240);
    m_idleMinutesSpin->setSuffix(QStringLiteral(" minutos"));
    m_idleMinutesSpin->setMaximumWidth(160);

    m_warningSecondsSpin = new QSpinBox(timerGroup);
    m_warningSecondsSpin->setRange(10, 180);
    m_warningSecondsSpin->setSuffix(QStringLiteral(" segundos"));
    m_warningSecondsSpin->setMaximumWidth(160);

    auto *saveTimerBtn = new QPushButton(QStringLiteral("Guardar tiempos de seguridad"), timerGroup);
    saveTimerBtn->setIcon(Icons::get(Icons::Name::Save, QColor(QStringLiteral("#FFFFFF")), 16));

    timerForm->addRow(QStringLiteral("Tiempo sin actividad antes del aviso:"), m_idleMinutesSpin);
    timerForm->addRow(QStringLiteral("Tiempo de respuesta del aviso:"), m_warningSecondsSpin);
    timerForm->addRow(QString(), saveTimerBtn);
    layout->addWidget(timerGroup);

    // 3. Token de habilitación / Licencia
    auto *licenseGroup = new QGroupBox(QStringLiteral("Token de Habilitación / Licencia"), page);
    auto *licenseLayout = new QVBoxLayout(licenseGroup);

    auto *licForm = new QFormLayout();
    licForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_licenseTokenEdit = new QLineEdit(licenseGroup);
    m_licenseTokenEdit->setPlaceholderText(QStringLiteral("Pega aquí el token GM1.xxxx.yyyy proporcionado"));

    m_licenseStatusLabel = new QLabel(licenseGroup);
    m_licenseStatusLabel->setWordWrap(true);

    licForm->addRow(QStringLiteral("Token activo:"), m_licenseTokenEdit);
    licForm->addRow(QStringLiteral("Estado:"), m_licenseStatusLabel);
    licenseLayout->addLayout(licForm);

    auto *licBtnRow = new QHBoxLayout();
    auto *saveTokenBtn = new QPushButton(QStringLiteral("Validar y guardar token"), licenseGroup);
    saveTokenBtn->setIcon(Icons::get(Icons::Name::Check, QColor(QStringLiteral("#FFFFFF")), 16));
    licBtnRow->addWidget(saveTokenBtn);
    licBtnRow->addStretch();
    licenseLayout->addLayout(licBtnRow);

#ifdef APP_DEBUG_BUILD
    // Herramienta exclusiva de compilación DEBUG para generar tokens de cliente/soporte
    auto *devBox = new QGroupBox(QStringLiteral("Generador de Tokens (Solo compilación Debug)"), licenseGroup);
    auto *devForm = new QFormLayout(devBox);
    devForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_devTokenScope = new QComboBox(devBox);
    m_devTokenScope->addItems({QStringLiteral("client"), QStringLiteral("db_admin"), QStringLiteral("full")});

    m_devTokenCuit = new QLineEdit(devBox);
    m_devTokenCuit->setPlaceholderText(QStringLiteral("CUIT del cliente o ANY"));
    m_devTokenCuit->setText(QStringLiteral("ANY"));

    m_devTokenExpiry = new QDateEdit(QDate::currentDate().addYears(1), devBox);
    m_devTokenExpiry->setCalendarPopup(true);
    m_devTokenExpiry->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));

    auto *genBtn = new QPushButton(QStringLiteral("Generar token"), devBox);
    genBtn->setIcon(Icons::get(Icons::Name::Key, QColor(QStringLiteral("#1E3A5F")), 15));
    genBtn->setProperty("secondary", true);

    devForm->addRow(QStringLiteral("Alcance:"), m_devTokenScope);
    devForm->addRow(QStringLiteral("CUIT autorizado:"), m_devTokenCuit);
    devForm->addRow(QStringLiteral("Válido hasta:"), m_devTokenExpiry);
    devForm->addRow(QString(), genBtn);
    licenseLayout->addWidget(devBox);

    connect(genBtn, &QPushButton::clicked, this, &PanelControlModule::onGenerateDevToken);
#endif

    layout->addWidget(licenseGroup);

    // Política de Elevación Desatendida (extensión de políticas del Administrador)
    auto *elevGroup = new QGroupBox(QStringLiteral("Política de Elevación Desatendida (Supervisores)"), page);
    auto *elevLayout = new QVBoxLayout(elevGroup);
    elevLayout->setSpacing(8);

    auto *elevInfo = new QLabel(
        QStringLiteral("Define qué secciones sensibles pueden desbloquear temporalmente los usuarios con nivel "
                    "<b>Responsable / Supervisor</b> mediante autogeneración de token auditado:"),
        elevGroup);
    elevInfo->setWordWrap(true);
    elevInfo->setStyleSheet(QStringLiteral("color: #52606D;"));
    elevLayout->addWidget(elevInfo);

    auto *elevForm = new QFormLayout();
    elevForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_elevDurationSpin = new QSpinBox(elevGroup);
    m_elevDurationSpin->setRange(5, 120);
    m_elevDurationSpin->setSuffix(QStringLiteral(" minutos"));
    m_elevDurationSpin->setMaximumWidth(150);
    elevForm->addRow(QStringLiteral("Duración máxima del token:"), m_elevDurationSpin);
    elevLayout->addLayout(elevForm);

    m_elevAllowSupportCheck = new QCheckBox(QStringLiteral("Permitir elevación en Tablas Soporte"), elevGroup);
    m_elevAllowBackupCheck  = new QCheckBox(QStringLiteral("Permitir elevación en Respaldo y Auditoría"), elevGroup);
    m_elevAllowDbAdminCheck = new QCheckBox(QStringLiteral("Permitir elevación en Base de Datos / SQL"), elevGroup);
    m_elevNotifyEmailCheck  = new QCheckBox(QStringLiteral("Enviar aviso automático al Email Institucional ante cada elevación"), elevGroup);

    elevLayout->addWidget(m_elevAllowSupportCheck);
    elevLayout->addWidget(m_elevAllowBackupCheck);
    elevLayout->addWidget(m_elevAllowDbAdminCheck);
    elevLayout->addWidget(m_elevNotifyEmailCheck);

    auto *elevBtnRow = new QHBoxLayout();
    auto *saveElevBtn = new QPushButton(QStringLiteral("Guardar política de elevación"), elevGroup);
    saveElevBtn->setIcon(Icons::get(Icons::Name::Save, QColor(QStringLiteral("#FFFFFF")), 15));
    elevBtnRow->addWidget(saveElevBtn);
    elevBtnRow->addStretch();
    elevLayout->addLayout(elevBtnRow);

    layout->addWidget(elevGroup);
    connect(saveElevBtn, &QPushButton::clicked, this, &PanelControlModule::onSaveElevationPolicy);


    layout->addStretch();

    connect(changePassBtn, &QPushButton::clicked, this, &PanelControlModule::onChangeAdminPassword);
    connect(saveTimerBtn, &QPushButton::clicked, this, &PanelControlModule::onSaveSecurityTimers);
    connect(saveTokenBtn, &QPushButton::clicked, this, &PanelControlModule::onSaveLicenseToken);
    return page;
}

QWidget *PanelControlModule::buildBackupAuditTab()
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(14);

    // 1. Respaldos (adaptado a SQLite o MariaDB)
    auto *backupGroup = new QGroupBox(QStringLiteral("Copia de Seguridad de la Base de Datos"), page);
    auto *backupLayout = new QVBoxLayout(backupGroup);

    auto *backupDesc = new QLabel(
        QStringLiteral("Genera un respaldo manual completo de la base activa. En modo SQLite también "
                       "se ejecutan copias rotativas automáticas al cerrar la aplicación."),
        backupGroup);
    backupDesc->setWordWrap(true);
    backupDesc->setStyleSheet(QStringLiteral("color: #52606D;"));
    backupLayout->addWidget(backupDesc);

    auto *bRow = new QHBoxLayout();
    m_backupBtn = new QPushButton(QStringLiteral("Generar respaldo ahora"), backupGroup);
    m_backupBtn->setIcon(Icons::get(Icons::Name::Backup, QColor(QStringLiteral("#FFFFFF")), 16));
    bRow->addWidget(m_backupBtn);
    bRow->addStretch();
    backupLayout->addLayout(bRow);
    layout->addWidget(backupGroup);

    // 2. Bitácora inmutable de auditoría + Verificador SHA-256
    auto *auditGroup = new QGroupBox(QStringLiteral("Bitácora de Auditoría Inmutable (Hash Chaining SHA-256)"), page);
    auto *auditLayout = new QVBoxLayout(auditGroup);
    auditLayout->setSpacing(10);

    auto *auditBar = new QHBoxLayout();
    auto *verifyBtn = new QPushButton(QStringLiteral("Verificar integridad (SHA-256)"), auditGroup);
    verifyBtn->setIcon(Icons::get(Icons::Name::Shield, QColor(QStringLiteral("#FFFFFF")), 15));

    auto *refreshAuditBtn = new QPushButton(QStringLiteral("Actualizar bitácora"), auditGroup);
    refreshAuditBtn->setIcon(Icons::get(Icons::Name::Refresh, QColor(QStringLiteral("#1E3A5F")), 15));
    refreshAuditBtn->setProperty("secondary", true);

    m_auditIntegrityLabel = new QLabel(
        QStringLiteral("Pulsa \"Verificar integridad\" para auditar la cadena criptográfica SHA-256."),
        auditGroup);
    m_auditIntegrityLabel->setWordWrap(true);
    m_auditIntegrityLabel->setStyleSheet(QStringLiteral("color: #52606D;"));

    auditBar->addWidget(verifyBtn);
    auditBar->addWidget(refreshAuditBtn);
    auditBar->addSpacing(8);
    auditBar->addWidget(m_auditIntegrityLabel, 1);
    auditLayout->addLayout(auditBar);

    m_auditTable = new QTableWidget(auditGroup);
    m_auditTable->setColumnCount(7);
    m_auditTable->setHorizontalHeaderLabels({
        QStringLiteral("Fecha (UTC)"),
        QStringLiteral("Usuario"),
        QStringLiteral("Rol"),
        QStringLiteral("Módulo / Tabla"),
        QStringLiteral("Acción"),
        QStringLiteral("Contexto"),
        QStringLiteral("Detalle / Puesto")
    });
    m_auditTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_auditTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_auditTable->setAlternatingRowColors(true);
    m_auditTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_auditTable->verticalHeader()->setVisible(false);
    m_auditTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_auditTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_auditTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_auditTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_auditTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_auditTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_auditTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
    m_auditTable->setMinimumHeight(280);
    auditLayout->addWidget(m_auditTable, 1);

    layout->addWidget(auditGroup, 1);

    connect(m_backupBtn, &QPushButton::clicked, this, &PanelControlModule::onCreateBackup);
    connect(verifyBtn, &QPushButton::clicked, this, &PanelControlModule::onVerifyAuditIntegrity);
    connect(refreshAuditBtn, &QPushButton::clicked, this, &PanelControlModule::loadAuditLog);

    return page;
}

void PanelControlModule::onActivated()
{
    loadAllSettings();
    m_supportTablesWidget->reloadAll();
    applyTabPermissions();
    updateFirstLoginAlert();

}

void PanelControlModule::loadAllSettings()
{
    const Database &db = Database::instance();

    // 1. Condiciones de IVA desde tabla soporte
    m_companyIvaCombo->clear();
    QSqlQuery ivaQ(db.connection());
    if (ivaQ.exec(QStringLiteral("SELECT name FROM iva_conditions WHERE is_active = 1 AND is_deprecated = 0 ORDER BY id;"))) {
        while (ivaQ.next()) {
            m_companyIvaCombo->addItem(ivaQ.value(0).toString());
        }
    }

    // Empresa
    m_companyLegalName->setText(db.getSetting(QStringLiteral("company.legal_name")));
    m_companyTradeName->setText(db.getSetting(QStringLiteral("company.trade_name")));
    m_companyCuit->setText(db.getSetting(QStringLiteral("company.cuit")));
    m_companyBusinessType->setCurrentText(db.getSetting(QStringLiteral("company.business_type"), QStringLiteral("Comercio")));
    m_companySubRubro->setText(db.getSetting(QStringLiteral("company.sub_rubro")));

    const QString savedIva = db.getSetting(QStringLiteral("company.iva_condition"));

    if (!savedIva.isEmpty()) {
        if (m_companyIvaCombo->findText(savedIva) < 0) {
            m_companyIvaCombo->addItem(savedIva + QStringLiteral(" [Deprecado]"));
        }
        m_companyIvaCombo->setCurrentText(savedIva);
    }

    m_companyAddress->setText(db.getSetting(QStringLiteral("company.address")));
    m_companyPhone->setText(db.getSetting(QStringLiteral("company.phone")));
    m_companyCurrency->setText(db.getSetting(QStringLiteral("regional.currency_symbol"), QStringLiteral("$")));

    const QString logoPath = db.getSetting(QStringLiteral("company.logo_path"));
    m_companyLogoPath->setText(logoPath);

    if (!logoPath.isEmpty() && QFile::exists(logoPath)) {
        const QPixmap pix(logoPath);
        if (!pix.isNull()) {
            m_companyLogoPreview->setPixmap(pix.scaled(68, 68, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    } else {
        m_companyLogoPreview->setPixmap(QPixmap());
        m_companyLogoPreview->setText(QStringLiteral("Sin logo"));
    }

    // SMTP
    const SmtpConfig smtp = Mailer::loadConfig();
    m_smtpFromEmail->setText(smtp.fromEmail);
    m_smtpFromName->setText(smtp.fromName);

    // Cargar modo básico
    const int provIdx = m_smtpBasicProvider->findData(smtp.provider.toUpper());
    m_smtpBasicProvider->setCurrentIndex(provIdx >= 0 ? provIdx : 0);
    m_smtpBasicPass->setText(smtp.password);
    onSmtpBasicProviderChanged(m_smtpBasicProvider->currentIndex());

    // Cargar modo avanzado
    m_smtpHost->setText(smtp.host);
    m_smtpSecurity->blockSignals(true);
    const int secIdx = m_smtpSecurity->findData(smtp.security.toUpper());
    m_smtpSecurity->setCurrentIndex(secIdx >= 0 ? secIdx : 0);
    m_smtpSecurity->blockSignals(false);
    m_smtpPort->setValue(smtp.port > 0 ? smtp.port : 465);

    const int authIdx = m_smtpAuthMethod->findData(smtp.authMethod.toUpper());
    m_smtpAuthMethod->setCurrentIndex(authIdx >= 0 ? authIdx : 0);
    m_smtpUser->setText(smtp.username);
    m_smtpPass->setText(smtp.password);

    m_oauthClientId->setText(smtp.oauthClientId);
    m_oauthClientSecret->setText(smtp.oauthClientSecret);
    m_cachedAccessToken = smtp.oauthAccessToken;
    m_cachedRefreshToken = smtp.oauthRefreshToken;
    m_cachedTokenExpiry = smtp.oauthTokenExpiry;

    if (smtp.hasOAuthSession()) {
        m_oauthStatusLabel->setText(QStringLiteral("Cuenta de Google vinculada correctamente."));
        m_oauthStatusLabel->setStyleSheet(QStringLiteral("color: #2E7D32; font-weight: 600;"));
    } else {
        m_oauthStatusLabel->setText(QStringLiteral("Pendiente de autorización en Google."));
        m_oauthStatusLabel->setStyleSheet(QStringLiteral("color: #B7791F; font-weight: 600;"));
    }

    const bool useBasic = (smtp.mode.compare(QStringLiteral("ADVANCED"), Qt::CaseInsensitive) != 0);
    m_smtpModeBasicRadio->blockSignals(true);
    m_smtpModeAdvancedRadio->blockSignals(true);
    m_smtpModeBasicRadio->setChecked(useBasic);
    m_smtpModeAdvancedRadio->setChecked(!useBasic);
    m_smtpModeBasicRadio->blockSignals(false);
    m_smtpModeAdvancedRadio->blockSignals(false);

    m_smtpBasicContainer->setVisible(useBasic);
    m_smtpAdvancedContainer->setVisible(!useBasic);
    onSmtpAuthMethodChanged(m_smtpAuthMethod->currentIndex());

    // Impresoras
    onRefreshPrinters();

    // Seguridad & Licencia
    m_idleMinutesSpin->setValue(db.getSetting(QStringLiteral("security.idle_minutes"), QStringLiteral("15")).toInt());
    m_warningSecondsSpin->setValue(db.getSetting(QStringLiteral("security.warning_seconds"), QStringLiteral("30")).toInt());

    m_licenseTokenEdit->setText(db.getSetting(QStringLiteral("license.token")));
    const LicenseInfo lic = LicenseToken::loadCurrent();
    m_licenseStatusLabel->setText(lic.statusText);
    m_licenseStatusLabel->setStyleSheet(
        lic.valid ? QStringLiteral("color: #2E7D32; font-weight: 600;")
                  : QStringLiteral("color: #B7791F; font-weight: 600;"));

    // Política de elevación desatendida (Supervisores)
    const ElevationPolicy elevPol = ElevationManager::loadPolicy();
    if (m_elevDurationSpin) {
        m_elevDurationSpin->setValue(elevPol.durationMinutes);
        m_elevAllowSupportCheck->setChecked(elevPol.allowSupportTables);
        m_elevAllowBackupCheck->setChecked(elevPol.allowBackupAudit);
        m_elevAllowDbAdminCheck->setChecked(elevPol.allowDbAdmin);
        m_elevNotifyEmailCheck->setChecked(elevPol.notifyByEmail);
    }

    // Respaldo y Auditoría
    loadAuditLog();
}

void PanelControlModule::updateDbAdminTabVisibility()
{
    applyTabPermissions();
}

void PanelControlModule::updateFirstLoginAlert()
{
    const bool mustChange = Session::instance().mustChangePassword();
    m_firstLoginAlert->setVisible(mustChange);
    if (mustChange && m_securityTabIndex >= 0) {
        m_tabs->setCurrentIndex(m_securityTabIndex);
        m_adminCurrentPass->setFocus();
    }
}

bool PanelControlModule::isValidCuitCuil(const QString &input, QString *formattedOut)
{
    QString digits;
    for (const QChar &ch : input) {
        if (ch.isDigit()) {
            digits.append(ch);
        }
    }
    if (digits.size() != 11) {
        return false;
    }

    static const int weights[10] = {5, 4, 3, 2, 7, 6, 5, 4, 3, 2};
    int sum = 0;
    for (int i = 0; i < 10; ++i) {
        sum += digits.at(i).digitValue() * weights[i];
    }

    int check = 11 - (sum % 11);
    if (check == 11) check = 0;
    else if (check == 10) check = 9;

    if (check != digits.at(10).digitValue()) {
        return false;
    }

    if (formattedOut) {
        *formattedOut = QStringLiteral("%1-%2-%3")
                            .arg(digits.left(2), digits.mid(2, 8), digits.right(1));
    }
    return true;
}

void PanelControlModule::onSelectCompanyLogo()
{
    const QString file = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Seleccionar logotipo del establecimiento"),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        QStringLiteral("Imágenes (*.png *.jpg *.jpeg *.svg)"));

    if (file.isEmpty()) {
        return;
    }

    m_companyLogoPath->setText(file);
    const QPixmap pix(file);
    if (!pix.isNull()) {
        m_companyLogoPreview->setPixmap(pix.scaled(68, 68, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void PanelControlModule::onSaveCompany()
{
    if (!requirePermission(QStringLiteral("company"), QStringLiteral("modificar datos de la empresa"))) {
        return;
    }

    const QString legalName = m_companyLegalName->text().trimmed();
    const QString rawCuit = m_companyCuit->text().trimmed();

    if (legalName.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Dato requerido"),
                             QStringLiteral("Ingresa la Razón Social del establecimiento."));
        m_companyLegalName->setFocus();
        return;
    }

    QString formattedCuit = rawCuit;
    if (!rawCuit.isEmpty() && !isValidCuitCuil(rawCuit, &formattedCuit)) {
        QMessageBox::warning(this, QStringLiteral("CUIT / CUIL inválido"),
                             QStringLiteral("El número de CUIT/CUIL ingresado no supera la validación "
                                            "de dígito verificador. Verifica los 11 dígitos."));
        m_companyCuit->setFocus();
        return;
    }

    m_companyCuit->setText(formattedCuit);

    Database &db = Database::instance();
    QString err;
    const bool ok =
        db.setSetting(QStringLiteral("company.legal_name"), legalName, &err) &&
        db.setSetting(QStringLiteral("company.trade_name"), m_companyTradeName->text().trimmed(), &err) &&
        db.setSetting(QStringLiteral("company.cuit"), formattedCuit, &err) &&
        db.setSetting(QStringLiteral("company.business_type"), m_companyBusinessType->currentText(), &err) &&
        db.setSetting(QStringLiteral("company.sub_rubro"), m_companySubRubro->text().trimmed(), &err) &&
        db.setSetting(QStringLiteral("company.iva_condition"), m_companyIvaCombo->currentText(), &err) &&
        db.setSetting(QStringLiteral("company.address"), m_companyAddress->text().trimmed(), &err) &&
        db.setSetting(QStringLiteral("company.phone"), m_companyPhone->text().trimmed(), &err) &&
        db.setSetting(QStringLiteral("regional.currency_symbol"),
                      m_companyCurrency->text().trimmed().isEmpty() ? QStringLiteral("$") : m_companyCurrency->text().trimmed(), &err) &&
        db.setSetting(QStringLiteral("company.logo_path"), m_companyLogoPath->text().trimmed(), &err);

    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("Error"), QStringLiteral("No se pudo guardar: %1").arg(err));
        return;
    }

    db.logAudit(Session::instance().currentUser().email, moduleId(),
                QStringLiteral("company_save"), QStringLiteral("Razón Social: %1").arg(legalName));
    QMessageBox::information(this, QStringLiteral("Datos guardados"),
                             QStringLiteral("Los datos de la empresa se actualizaron correctamente."));
}

// ============================================================================================================================================================= //

void PanelControlModule::onRefreshPrinters()
{
    const Database &db = Database::instance();
    const QString savedMain = db.getSetting(QStringLiteral("printer.main"));
    const QString savedTicket = db.getSetting(QStringLiteral("printer.ticket"));
    const QString savedWidth = db.getSetting(QStringLiteral("printer.ticket_width"), QStringLiteral("80 mm (Estándar)"));

    const QStringList printers = QPrinterInfo::availablePrinterNames();

    m_mainPrinterCombo->clear();
    m_ticketPrinterCombo->clear();

    m_mainPrinterCombo->addItem(QStringLiteral("(Predeterminada del sistema)"), QString());
    m_ticketPrinterCombo->addItem(QStringLiteral("(Sin asignar)"), QString());

    for (const QString &name : printers) {
        m_mainPrinterCombo->addItem(name, name);
        m_ticketPrinterCombo->addItem(name, name);
    }

    const int mainIdx = m_mainPrinterCombo->findData(savedMain);
    if (mainIdx >= 0) m_mainPrinterCombo->setCurrentIndex(mainIdx);

    const int ticketIdx = m_ticketPrinterCombo->findData(savedTicket);
    if (ticketIdx >= 0) m_ticketPrinterCombo->setCurrentIndex(ticketIdx);

    m_ticketWidthCombo->setCurrentText(savedWidth);

    if (printers.isEmpty()) {
        m_printersInfoLabel->setText(QStringLiteral("No se detectaron impresoras instaladas en el sistema operativo."));
    } else {
        m_printersInfoLabel->setText(
            QStringLiteral("Se detectaron %1 impresora(s) disponible(s) (locales/red).").arg(printers.size()));
    }
}


// ============================================================================================================================================================= //


void PanelControlModule::onSavePrinters()
{
    Database &db = Database::instance();
    QString err;
    const QString mainName = m_mainPrinterCombo->currentData().toString();
    const QString ticketName = m_ticketPrinterCombo->currentData().toString();

    const bool ok =
        db.setSetting(QStringLiteral("printer.main"), mainName, &err) &&
        db.setSetting(QStringLiteral("printer.ticket"), ticketName, &err) &&
        db.setSetting(QStringLiteral("printer.ticket_width"), m_ticketWidthCombo->currentText(), &err);

    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("Error"), err);
        return;
    }

    db.logAudit(Session::instance().currentUser().email, moduleId(),
                QStringLiteral("printers_save"), QStringLiteral("Principal: %1 | Tickets: %2").arg(mainName, ticketName));
    QMessageBox::information(this, QStringLiteral("Impresoras guardadas"),
                             QStringLiteral("La configuración de impresoras se guardó correctamente."));
}


// ============================================================================================================================================================= //


void PanelControlModule::onPrintTestPage()
{
    const QString selected = m_mainPrinterCombo->currentData().toString();
    QPrinter printer(QPrinter::HighResolution);
    if (!selected.isEmpty()) {
        printer.setPrinterName(selected);
    }
    printer.setDocName(QStringLiteral("Global Manager - Prueba de impresión"));

    if (!printer.isValid()) {
        QMessageBox::warning(this, QStringLiteral("Impresora no disponible"),
                             QStringLiteral("No hay una impresora válida seleccionada o disponible en el sistema."));
        return;
    }

    QPainter painter(&printer);
    if (!painter.isActive()) {
        QMessageBox::warning(this, QStringLiteral("Error de impresión"),
                             QStringLiteral("No se pudo iniciar el trabajo de impresión."));
        return;
    }

    const QRect pageRect = printer.pageLayout().paintRectPixels(printer.resolution());
    QFont titleFont = painter.font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.drawText(pageRect.adjusted(40, 40, -40, -40), Qt::AlignTop | Qt::AlignLeft,
                     QStringLiteral("Global Manager - Página de Prueba"));

    QFont bodyFont = painter.font();
    bodyFont.setPointSize(11);
    bodyFont.setBold(false);
    painter.setFont(bodyFont);

    const QString body = QStringLiteral(
        "Impresora: %1\n"
        "Establecimiento: %2\n"
        "Fecha y hora: %3\n\n"
        "Si puedes leer este documento, la impresora quedó correctamente configurada.")
                             .arg(printer.printerName(),
                                  Database::instance().getSetting(QStringLiteral("company.legal_name"), QStringLiteral("(Sin configurar)")),
                                  QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yyyy HH:mm:ss")));

    painter.drawText(pageRect.adjusted(40, 140, -40, -40), Qt::AlignTop | Qt::TextWordWrap, body);
    painter.end();

    QMessageBox::information(this, QStringLiteral("Prueba enviada"),
                             QStringLiteral("Se envió la página de prueba a \"%1\".").arg(printer.printerName()));
}


// ============================================================================================================================================================= //


void PanelControlModule::onChangeAdminPassword()
{
    const UserSessionInfo &user = Session::instance().currentUser();
    if (!user.isAdmin) {
        QMessageBox::warning(this, QStringLiteral("Permiso denegado"),
                             QStringLiteral("Solo el usuario Administrador puede cambiar esta contraseña."));
        return;
    }

    const QString currentPass = m_adminCurrentPass->text();
    const QString newPass = m_adminNewPass->text();
    const QString confirmPass = m_adminConfirmPass->text();

    if (currentPass.isEmpty() || newPass.isEmpty() || confirmPass.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Campos incompletos"),
                             QStringLiteral("Completa la contraseña actual, la nueva y su confirmación."));
        return;
    }

    if (newPass != confirmPass) {
        QMessageBox::warning(this, QStringLiteral("No coincide"),
                             QStringLiteral("La nueva contraseña y su confirmación no coinciden."));
        return;
    }

    if (newPass == currentPass) {
        QMessageBox::warning(this, QStringLiteral("Contraseña repetida"),
                             QStringLiteral("La nueva contraseña debe ser diferente a la actual."));
        return;
    }

    const bool hasUpper = newPass.contains(QRegularExpression(QStringLiteral("[A-Z]")));
    const bool hasLower = newPass.contains(QRegularExpression(QStringLiteral("[a-z]")));
    const bool hasDigit = newPass.contains(QRegularExpression(QStringLiteral("[0-9]")));
    if (newPass.size() < 8 || !hasUpper || !hasLower || !hasDigit) {
        QMessageBox::warning(this, QStringLiteral("Contraseña débil"),
                             QStringLiteral("La nueva contraseña debe tener al menos 8 caracteres, "
                                            "incluir mayúsculas, minúsculas y al menos un número."));
        return;
    }

    QSqlQuery checkQ(Database::instance().connection());
    checkQ.prepare(QStringLiteral("SELECT password_hash FROM users WHERE id = :id LIMIT 1;"));
    checkQ.bindValue(QStringLiteral(":id"), user.id);

    if (!checkQ.exec() || !checkQ.next()) {
        QMessageBox::critical(this, QStringLiteral("Error"),
                              QStringLiteral("No se pudo verificar el usuario administrador."));
        return;
    }

    if (!Database::verifyPassword(currentPass, checkQ.value(0).toString())) {
        QMessageBox::warning(this, QStringLiteral("Contraseña incorrecta"),
                             QStringLiteral("La contraseña actual ingresada no es válida."));
        m_adminCurrentPass->clear();
        m_adminCurrentPass->setFocus();
        return;
    }

    QSqlQuery updQ(Database::instance().connection());
    updQ.prepare(QStringLiteral(
        "UPDATE users SET password_hash = :hash, must_change_password = 0 WHERE id = :id;"));
    updQ.bindValue(QStringLiteral(":hash"), Database::hashPassword(newPass));
    updQ.bindValue(QStringLiteral(":id"), user.id);

    if (!updQ.exec()) {
        QMessageBox::critical(this, QStringLiteral("Error"),
                              QStringLiteral("No se pudo actualizar la contraseña:\n%1").arg(updQ.lastError().text()));
        return;
    }

    m_adminCurrentPass->clear();
    m_adminNewPass->clear();
    m_adminConfirmPass->clear();

    Session::instance().markPasswordChanged();
    updateFirstLoginAlert();

    Database::instance().logAudit(user.email, moduleId(),
                                  QStringLiteral("admin_password_change"),
                                  QStringLiteral("Contraseña de administrador actualizada"));

    QMessageBox::information(this, QStringLiteral("Contraseña actualizada"),
                             QStringLiteral("La contraseña del administrador se actualizó correctamente. "
                                            "Ya puedes operar en todos los módulos."));
}


// ============================================================================================================================================================= //


void PanelControlModule::onSaveSecurityTimers()
{
    Database &db = Database::instance();
    QString err;
    const bool ok =
        db.setSetting(QStringLiteral("security.idle_minutes"), QString::number(m_idleMinutesSpin->value()), &err) &&
        db.setSetting(QStringLiteral("security.warning_seconds"), QString::number(m_warningSecondsSpin->value()), &err);

    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("Error"), err);
        return;
    }

    Session::instance().reloadInactivityFromDatabase();
    db.logAudit(Session::instance().currentUser().email, moduleId(),
                QStringLiteral("security_timers_save"),
                QStringLiteral("Inactividad: %1 min | Aviso: %2 seg")
                    .arg(m_idleMinutesSpin->value())
                    .arg(m_warningSecondsSpin->value()));

    QMessageBox::information(this, QStringLiteral("Seguridad actualizada"),
                             QStringLiteral("Los tiempos de cierre automático por inactividad ya están vigentes."));
}


// ============================================================================================================================================================= //


void PanelControlModule::onSaveLicenseToken()
{
    QString err;
    const QString token = m_licenseTokenEdit->text().trimmed();
    if (!LicenseToken::saveToken(token, &err)) {
        m_licenseStatusLabel->setText(err);
        m_licenseStatusLabel->setStyleSheet(QStringLiteral("color: #C53030; font-weight: 600;"));
        QMessageBox::warning(this, QStringLiteral("Token inválido"), err);
        return;
    }

    const LicenseInfo lic = LicenseToken::loadCurrent();
    m_licenseStatusLabel->setText(lic.statusText);
    m_licenseStatusLabel->setStyleSheet(QStringLiteral("color: #2E7D32; font-weight: 600;"));
    updateDbAdminTabVisibility();

    Database::instance().logAudit(Session::instance().currentUser().email, moduleId(),
                                  QStringLiteral("license_token_save"), lic.statusText);

    QMessageBox::information(this, QStringLiteral("Token habilitado"), lic.statusText);
}


// ============================================================================================================================================================= //


void PanelControlModule::onGenerateDevToken()
{
#ifdef APP_DEBUG_BUILD
    const QString generated = LicenseToken::generate(
        m_devTokenScope->currentText(),
        m_devTokenCuit->text().trimmed(),
        m_devTokenExpiry->date());
    m_licenseTokenEdit->setText(generated);
    QMessageBox::information(
        this,
        QStringLiteral("Token generado"),
        QStringLiteral("Se generó un token firmado y se colocó en el campo 'Token activo'.\n"
                       "Pulsa 'Validar y guardar token' para aplicarlo."));
#endif
}


// ============================================================================================================================================================= //


void PanelControlModule::onCreateBackup()
{
    const bool hasAccess = Session::instance().currentUser().isAdmin ||
                           Session::instance().hasPermission(moduleId(), QStringLiteral("backup_audit")) ||
                           ElevationManager::instance().hasActiveElevationFor(QStringLiteral("backup_audit"));
    if (!hasAccess) {
        QMessageBox::warning(this, QStringLiteral("Acceso restringido"),
                             QStringLiteral("No tienes permisos activos para generar respaldos."));
        return;
    }

    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));

    if (Database::instance().isMariaDb()) {
        const QString suggested = QDir(BootstrapConfig::protectedDbDirPath())
                                      .filePath(QStringLiteral("respaldo_mariadb_%1.sql").arg(stamp));
        const QString dest = QFileDialog::getSaveFileName(
            this, QStringLiteral("Guardar respaldo SQL de MariaDB"), suggested,
            QStringLiteral("Script SQL (*.sql)"));
        if (dest.isEmpty()) return;

        QApplication::setOverrideCursor(Qt::WaitCursor);
        QString outPath, err;
        const bool ok = Database::instance().createSqlDump(true, &outPath, &err, dest);
        QApplication::restoreOverrideCursor();

        if (ok) {
            Database::instance().logAudit(
                Session::instance().currentUser().email, moduleId(),
                QStringLiteral("backup_mariadb"), QStringLiteral("Archivo: %1").arg(outPath));
            loadAuditLog();
            QMessageBox::information(this, QStringLiteral("Respaldo completado"),
                                     QStringLiteral("El dump SQL de MariaDB se guardó en:\n%1").arg(outPath));
        } else {
            QMessageBox::critical(this, QStringLiteral("Error de respaldo"), err);
        }
        return;
    }

    // Modo SQLite
    const QString suggested = QDir(BootstrapConfig::autoBackupDirPath())
                                  .filePath(QStringLiteral("qGlobalManager_backup_%1.sqlite").arg(stamp));
    const QString dest = QFileDialog::getSaveFileName(
        this, QStringLiteral("Guardar copia de seguridad SQLite"), suggested,
        QStringLiteral("Base de datos SQLite (*.sqlite)"));
    if (dest.isEmpty()) return;

    QString escaped = dest;
    escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
    QSqlQuery q(Database::instance().connection());
    bool ok = q.exec(QStringLiteral("VACUUM INTO '%1';").arg(escaped));
    if (!ok) {
        q.exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE);"));
        QFile::remove(dest);
        ok = QFile::copy(Database::instance().databasePath(), dest);
    }

    if (ok) {
        Database::instance().logAudit(
            Session::instance().currentUser().email, moduleId(),
            QStringLiteral("backup_sqlite"), QStringLiteral("Archivo: %1").arg(dest));
        loadAuditLog();
        QMessageBox::information(this, QStringLiteral("Respaldo completado"),
                                 QStringLiteral("Copia de seguridad creada correctamente en:\n%1").arg(dest));
    } else {
        QMessageBox::critical(this, QStringLiteral("Error de respaldo"),
                              QStringLiteral("No se pudo generar la copia de seguridad en:\n%1").arg(dest));
    }
}



void PanelControlModule::loadAuditLog()
{
    if (!m_auditTable) return;
    m_auditTable->setRowCount(0);

    if (m_backupBtn) {
        m_backupBtn->setText(Database::instance().isMariaDb()
                                 ? QStringLiteral("Generar Dump SQL de respaldo (MariaDB)")
                                 : QStringLiteral("Generar copia de seguridad (SQLite)"));
    }

    QSqlQuery query(Database::instance().connection());
    if (!query.exec(QStringLiteral(
            "SELECT created_at, user_email, user_full_name, user_role, module_id, "
            "       target_table, action, auth_context, workstation, details "
            "FROM audit_log ORDER BY id DESC LIMIT 200;"))) {
        return;
    }

    int row = 0;
    while (query.next()) {
        const QString createdAt  = query.value(0).toString();
        const QString email      = query.value(1).toString();
        const QString fullName   = query.value(2).toString();
        const QString role       = query.value(3).toString();
        const QString modId      = query.value(4).toString();
        const QString tbl        = query.value(5).toString();
        const QString action     = query.value(6).toString();
        const QString authCtx    = query.value(7).toString();
        const QString ws         = query.value(8).toString();
        const QString details    = query.value(9).toString();

        const QString userLabel = fullName.isEmpty() ? email : QStringLiteral("%1 (%2)").arg(fullName, email);
        const QString modTbl    = tbl.isEmpty() ? modId : QStringLiteral("%1 / %2").arg(modId, tbl);
        const QString detailAll = ws.isEmpty() ? details : QStringLiteral("[%1] %2").arg(ws, details);

        const bool isTokenEvent = authCtx.startsWith(QStringLiteral("TOKEN_ELEVATION")) ||
                                  action.startsWith(QStringLiteral("TOKEN_ELEVATION"));

        m_auditTable->insertRow(row);
        m_auditTable->setItem(row, 0, new QTableWidgetItem(createdAt));
        m_auditTable->setItem(row, 1, new QTableWidgetItem(userLabel));
        m_auditTable->setItem(row, 2, new QTableWidgetItem(role));
        m_auditTable->setItem(row, 3, new QTableWidgetItem(modTbl));
        m_auditTable->setItem(row, 4, new QTableWidgetItem(action));
        m_auditTable->setItem(row, 5, new QTableWidgetItem(authCtx));
        m_auditTable->setItem(row, 6, new QTableWidgetItem(detailAll));

        if (isTokenEvent) {
            const QBrush bg(QColor(QStringLiteral("#FFF8E1")));
            const QBrush fg(QColor(QStringLiteral("#744210")));
            for (int c = 0; c < 7; ++c) {
                if (QTableWidgetItem *it = m_auditTable->item(row, c)) {
                    it->setBackground(bg);
                    if (c == 4 || c == 5) {
                        it->setForeground(fg);
                    }
                }
            }
        }
        ++row;
    }
}

// ============================================================================================================================================================= //

/*
void PanelControlModule::onRefreshAuditLog()
{
    m_auditTable->setRowCount(0);

    QSqlQuery query(Database::instance().connection());
    if (!query.exec(QStringLiteral(
            "SELECT created_at, user_email, module_id, action, details "
            "FROM audit_log ORDER BY id DESC LIMIT 200;"))) {
        return;
    }

    int row = 0;
    while (query.next()) {
        m_auditTable->insertRow(row);
        for (int col = 0; col < 5; ++col) {
            m_auditTable->setItem(row, col, new QTableWidgetItem(query.value(col).toString()));
        }
        ++row;
    }
}
*/

// ============================================================================================================================================================= //


void PanelControlModule::onSmtpSecurityChanged(int index)
{
    const QString sec = m_smtpSecurity->itemData(index).toString();
    if (sec == QStringLiteral("SSL")) {
        m_smtpPort->setValue(465);
    } else if (sec == QStringLiteral("STARTTLS")) {
        m_smtpPort->setValue(587);
    } else {
        m_smtpPort->setValue(25);
    }
}


// ============================================================================================================================================================= //


void PanelControlModule::onSmtpAuthMethodChanged(int index)
{
    const QString auth = m_smtpAuthMethod->itemData(index).toString();
    m_smtpPasswordGroup->setVisible(auth == QStringLiteral("PASSWORD"));
    m_smtpOAuthGroup->setVisible(auth == QStringLiteral("OAUTH2"));

    // Si el usuario elige OAuth2 y el host estaba vacío, prellenar smtp.gmail.com
    if (auth == QStringLiteral("OAUTH2") && m_smtpHost->text().trimmed().isEmpty()) {
        m_smtpHost->setText(QStringLiteral("smtp.gmail.com"));
        const int sslIdx = m_smtpSecurity->findData(QStringLiteral("SSL"));
        if (sslIdx >= 0) {
            m_smtpSecurity->setCurrentIndex(sslIdx);
        }
    }
}



// ============================================================================================================================================================= //


void PanelControlModule::onAuthorizeGoogleOAuth()
{
    const QString clientId = m_oauthClientId->text().trimmed();
    const QString clientSecret = m_oauthClientSecret->text().trimmed();
    const QString email = m_smtpFromEmail->text().trimmed();

    if (email.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Email requerido"),
                             QStringLiteral("Ingresa primero el Email Institucional de Gmail que deseas autorizar."));
        m_smtpFromEmail->setFocus();
        return;
    }

    OAuth2Tokens tokens;
    QString err;
    if (!OAuth2Google::authorizeInteractive(clientId, clientSecret, email, this, &tokens, &err)) {
        QMessageBox::warning(this, QStringLiteral("Autorización OAuth2 no completada"), err);
        return;
    }

    m_cachedAccessToken = tokens.accessToken;
    if (!tokens.refreshToken.isEmpty()) {
        m_cachedRefreshToken = tokens.refreshToken;
    }
    m_cachedTokenExpiry = tokens.expiresAtEpoch;

    m_oauthStatusLabel->setText(QStringLiteral("Cuenta de Google vinculada correctamente (lista para enviar)."));
    m_oauthStatusLabel->setStyleSheet(QStringLiteral("color: #2E7D32; font-weight: 600;"));

    // Guardar de inmediato para no perder el refresh_token
    onSaveSmtp();
}


// ============================================================================================================================================================= //


void PanelControlModule::onClearGoogleOAuth()
{
    m_cachedAccessToken.clear();
    m_cachedRefreshToken.clear();
    m_cachedTokenExpiry = 0;

    m_oauthStatusLabel->setText(QStringLiteral("Cuenta desvinculada. Pulsa \"Guardar configuración SMTP\" para confirmar."));
    m_oauthStatusLabel->setStyleSheet(QStringLiteral("color: #B7791F; font-weight: 600;"));
}


// ============================================================================================================================================================= //


void PanelControlModule::onSmtpModeToggled(bool basicSelected)
{
    if (!basicSelected) {
        // Sincronizar datos desde Básico → Avanzado para no reescribir nada
        const MailProviderPreset *preset = findPresetById(m_smtpBasicProvider->currentData().toString());
        if (m_smtpHost->text().trimmed().isEmpty()) {
            m_smtpHost->setText(QString::fromLatin1(preset->host));
            const int secIdx = m_smtpSecurity->findData(QString::fromLatin1(preset->security));
            if (secIdx >= 0) m_smtpSecurity->setCurrentIndex(secIdx);
            m_smtpPort->setValue(preset->port);
        }
        if (m_smtpUser->text().trimmed().isEmpty()) {
            m_smtpUser->setText(m_smtpFromEmail->text().trimmed());
        }
        if (!m_smtpBasicPass->text().isEmpty()) {
            m_smtpPass->setText(m_smtpBasicPass->text());
        }
    } else {
        // Sincronizar contraseña desde Avanzado → Básico
        if (!m_smtpPass->text().isEmpty()) {
            m_smtpBasicPass->setText(m_smtpPass->text());
        }
    }

    m_smtpBasicContainer->setVisible(basicSelected);
    m_smtpAdvancedContainer->setVisible(!basicSelected);
}


// ============================================================================================================================================================= //


void PanelControlModule::onSmtpBasicProviderChanged(int index)
{
    const QString providerId = m_smtpBasicProvider->itemData(index).toString();
    const MailProviderPreset *preset = findPresetById(providerId);

    m_smtpBasicHelpLabel->setText(QString::fromUtf8(preset->helpText));
    m_smtpBasicSummaryLabel->setText(
        QStringLiteral("Configuración automática: %1 · Puerto %2 (%3)")
            .arg(QString::fromLatin1(preset->host))
            .arg(preset->port)
            .arg(QString::fromLatin1(preset->security) == QStringLiteral("SSL")
                     ? QStringLiteral("SSL/TLS")
                     : QString::fromLatin1(preset->security)));
}


// ============================================================================================================================================================= //


void PanelControlModule::onOpenProviderHelpUrl()
{
    const QString providerId = m_smtpBasicProvider->currentData().toString();
    const MailProviderPreset *preset = findPresetById(providerId);
    QDesktopServices::openUrl(QUrl(QString::fromLatin1(preset->helpUrl)));
}


// ============================================================================================================================================================= //


SmtpConfig PanelControlModule::collectSmtpConfigFromUi() const
{
    SmtpConfig cfg;
    cfg.fromEmail = m_smtpFromEmail->text().trimmed();
    cfg.fromName  = m_smtpFromName->text().trimmed();

    if (m_smtpModeBasicRadio->isChecked()) {
        const QString providerId = m_smtpBasicProvider->currentData().toString();
        const MailProviderPreset *preset = findPresetById(providerId);

        cfg.mode       = QStringLiteral("BASIC");
        cfg.provider   = providerId;
        cfg.host       = QString::fromLatin1(preset->host);
        cfg.port       = preset->port;
        cfg.security   = QString::fromLatin1(preset->security);
        cfg.authMethod = QStringLiteral("PASSWORD");
        cfg.username   = cfg.fromEmail;
        cfg.password   = Mailer::normalizePasswordIfAppPassword(m_smtpBasicPass->text(), providerId);
    } else {
        cfg.mode              = QStringLiteral("ADVANCED");
        cfg.provider          = QStringLiteral("CUSTOM");
        cfg.host              = m_smtpHost->text().trimmed();
        cfg.port              = m_smtpPort->value();
        cfg.security          = m_smtpSecurity->currentData().toString();
        cfg.authMethod        = m_smtpAuthMethod->currentData().toString();
        cfg.username          = m_smtpUser->text().trimmed();
        cfg.password          = Mailer::normalizePasswordIfAppPassword(m_smtpPass->text(), cfg.host);
        cfg.oauthClientId     = m_oauthClientId->text().trimmed();
        cfg.oauthClientSecret = m_oauthClientSecret->text().trimmed();
        cfg.oauthAccessToken  = m_cachedAccessToken;
        cfg.oauthRefreshToken = m_cachedRefreshToken;
        cfg.oauthTokenExpiry  = m_cachedTokenExpiry;
    }

    return cfg;
}


// ============================================================================================================================================================= //


void PanelControlModule::onSaveSmtp()
{
    const SmtpConfig cfg = collectSmtpConfigFromUi();

    if (cfg.fromEmail.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Dato requerido"),
                             QStringLiteral("Ingresa el Email Institucional desde donde se enviarán los correos."));
        m_smtpFromEmail->setFocus();
        return;
    }

    QString err;
    if (!Mailer::saveConfig(cfg, &err)) {
        QMessageBox::warning(this, QStringLiteral("Error SMTP"),
                             QStringLiteral("No se pudo guardar la configuración de correo:\n%1").arg(err));
        return;
    }

    // Reflejar clave normalizada (sin espacios accidentales) en pantalla
    m_smtpBasicPass->setText(cfg.password);
    m_smtpPass->setText(cfg.password);

    Database::instance().logAudit(
        Session::instance().currentUser().email, moduleId(),
        QStringLiteral("smtp_save"),
        QStringLiteral("Modo: %1 | Proveedor: %2 | Host: %3:%4")
            .arg(cfg.mode, cfg.provider, cfg.host).arg(cfg.port));

    QMessageBox::information(this, QStringLiteral("Correo guardado"),
                             QStringLiteral("La configuración del correo corporativo fue guardada correctamente."));
}


// ============================================================================================================================================================= //


void PanelControlModule::onTestSmtp()
{
    const SmtpConfig cfg = collectSmtpConfigFromUi();

    if (!cfg.isComplete()) {
        QMessageBox::warning(this, QStringLiteral("Datos incompletos"),
                             QStringLiteral("Completa tu email y contraseña (o los datos del servidor en modo avanzado) antes de probar."));
        return;
    }

    bool okInput = false;
    const QString dest = QInputDialog::getText(
        this,
        QStringLiteral("Probar correo corporativo"),
        QStringLiteral("Ingresa el correo de destino para recibir el mensaje de prueba:"),
        QLineEdit::Normal,
        cfg.fromEmail,
        &okInput).trimmed();

    if (!okInput || dest.isEmpty()) {
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString err;
    const bool sent = Mailer::sendMailWithConfig(
        cfg,
        dest,
        QStringLiteral("Global Manager - Prueba de correo SMTP"),
        QStringLiteral("Este es un correo de verificación enviado desde el Panel de Control de Global Manager.\n"
                       "La configuración del motor de correo funciona correctamente."),
        QStringList(),
        &err);
    QApplication::restoreOverrideCursor();

    if (sent) {
        QMessageBox::information(this, QStringLiteral("Envío exitoso"),
                                 QStringLiteral("El correo de prueba fue enviado correctamente a %1.").arg(dest));
    } else {
        QMessageBox::critical(this, QStringLiteral("Falló el envío de correo"), err);
    }
}


// ============================================================================================================================================================= //


QString PanelControlModule::tabElevationScope(int index) const
{
    if (index == m_supportTablesTabIndex) return QStringLiteral("support_tables");
    if (index == m_backupAuditTabIndex)   return QStringLiteral("backup_audit");
    if (index == m_dbAdminTabIndex)       return QStringLiteral("db_admin");
    return QString();
}

QString PanelControlModule::baseTabTitle(int index) const
{
    if (index == m_companyTabIndex)       return QStringLiteral("Empresa");
    if (index == m_smtpTabIndex)          return QStringLiteral("Correo / SMTP");
    if (index == m_printersTabIndex)      return QStringLiteral("Impresoras");
    if (index == m_securityTabIndex)      return QStringLiteral("Seguridad y Licencia");
    if (index == m_supportTablesTabIndex) return QStringLiteral("Tablas Soporte");
    if (index == m_backupAuditTabIndex)   return QStringLiteral("Respaldo y Auditoría");
    if (index == m_dbAdminTabIndex)       return QStringLiteral("Base de Datos");
    return QString();
}

Icons::Name PanelControlModule::baseTabIcon(int index) const
{
    if (index == m_companyTabIndex)       return Icons::Name::Company;
    if (index == m_smtpTabIndex)          return Icons::Name::Mail;
    if (index == m_printersTabIndex)      return Icons::Name::Printer;
    if (index == m_securityTabIndex)      return Icons::Name::Shield;
    if (index == m_supportTablesTabIndex) return Icons::Name::Table;
    if (index == m_backupAuditTabIndex)   return Icons::Name::Backup;
    return Icons::Name::Database;
}

bool PanelControlModule::isTabUnlocked(int index) const
{
    const Session &s = Session::instance();
    const bool isAdmin = s.currentUser().isAdmin;

    if (index == m_companyTabIndex) {
        return isAdmin || s.hasPermission(moduleId(), QStringLiteral("company"));
    }
    if (index == m_smtpTabIndex) {
        return isAdmin || s.hasPermission(moduleId(), QStringLiteral("smtp"));
    }
    if (index == m_printersTabIndex) {
        return isAdmin || s.hasPermission(moduleId(), QStringLiteral("printers"));
    }
    if (index == m_securityTabIndex) {
        return isAdmin || s.hasPermission(moduleId(), QStringLiteral("security"));
    }
    if (index == m_supportTablesTabIndex) {
        return isAdmin ||
               s.hasPermission(moduleId(), QStringLiteral("support_tables")) ||
               ElevationManager::instance().hasActiveElevationFor(QStringLiteral("support_tables"));
    }
    if (index == m_backupAuditTabIndex) {
        return isAdmin ||
               s.hasPermission(moduleId(), QStringLiteral("backup_audit")) ||
               ElevationManager::instance().hasActiveElevationFor(QStringLiteral("backup_audit"));
    }
    if (index == m_dbAdminTabIndex) {
        if (!LicenseToken::isDbAdminAllowed()) {
            return false;
        }
        return isAdmin ||
               s.hasPermission(moduleId(), QStringLiteral("db_admin")) ||
               ElevationManager::instance().hasActiveElevationFor(QStringLiteral("db_admin"));
    }
    return false;
}

void PanelControlModule::applyTabPermissions()
{
    if (!m_tabs) return;

    const bool dbLicensed = LicenseToken::isDbAdminAllowed();
    m_tabs->setTabVisible(m_dbAdminTabIndex, dbLicensed);

    const QColor normalColor(QStringLiteral("#1E3A5F"));
    const QColor lockedColor(QStringLiteral("#9AA5B1"));
    const QColor tokenColor(QStringLiteral("#B7791F"));

    int firstUnlocked = -1;

    for (int i = 0; i < m_tabs->count(); ++i) {
        if (!m_tabs->isTabVisible(i)) {
            continue;
        }

        const QString title = baseTabTitle(i);
        const QString scope = tabElevationScope(i);
        const bool unlocked = isTabUnlocked(i);
        const bool viaToken = !scope.isEmpty() && ElevationManager::instance().hasActiveElevationFor(scope);

        if (unlocked && firstUnlocked < 0) {
            firstUnlocked = i;
        }

        if (viaToken) {
            m_tabs->setTabIcon(i, Icons::get(Icons::Name::Unlock, tokenColor, 16));
            m_tabs->setTabText(i, QStringLiteral("%1 [Token]").arg(title));
            m_tabs->setTabToolTip(i, QStringLiteral("Acceso temporal habilitado por token de elevación (auditado)."));
        } else if (unlocked) {
            m_tabs->setTabIcon(i, Icons::get(baseTabIcon(i), normalColor, 16));
            m_tabs->setTabText(i, title);
            m_tabs->setTabToolTip(i, title);
        } else {
            m_tabs->setTabIcon(i, Icons::get(Icons::Name::Lock, lockedColor, 16));
            m_tabs->setTabText(i, title);
            m_tabs->setTabToolTip(
                i,
                scope.isEmpty()
                    ? QStringLiteral("Sección restringida para tu perfil de usuario.")
                    : QStringLiteral("Sección protegida · Requiere rol Administrador o Token de elevación temporal."));
        }
    }

    // Si la solapa actual quedó bloqueada (por ejemplo, al vencer o revocar un token), volver a una permitida
    const int current = m_tabs->currentIndex();
    if (!isTabUnlocked(current) || !m_tabs->isTabVisible(current)) {
        m_handlingTabChange = true;
        const int target = (firstUnlocked >= 0) ? firstUnlocked : 0;
        m_tabs->setCurrentIndex(target);
        m_lastAllowedTabIndex = target;
        m_handlingTabChange = false;
    } else {
        m_lastAllowedTabIndex = current;
    }

    if (dbLicensed && isTabUnlocked(m_dbAdminTabIndex) && m_dbAdminWidget) {
        m_dbAdminWidget->refreshTables();
    }
}

void PanelControlModule::onTabChanged(int index)
{
    if (index < 0 || m_handlingTabChange) {
        return;
    }

    if (isTabUnlocked(index)) {
        m_lastAllowedTabIndex = index;
        if (index == m_dbAdminTabIndex && m_dbAdminWidget) {
            m_dbAdminWidget->refreshTables();
        } else if (index == m_backupAuditTabIndex) {
            loadAuditLog();
        } else if (index == m_supportTablesTabIndex && m_supportTablesWidget) {
            m_supportTablesWidget->reloadAll();
        }
        return;
    }

    // Volver inmediatamente a la solapa previa para no exponer ningún dato de la solapa bloqueada
    m_handlingTabChange = true;
    const int fallback = (m_lastAllowedTabIndex >= 0 && m_lastAllowedTabIndex < m_tabs->count())
                             ? m_lastAllowedTabIndex
                             : 0;
    m_tabs->setCurrentIndex(fallback);
    m_handlingTabChange = false;

    const QString scope = tabElevationScope(index);
    const QString title = baseTabTitle(index);

    // Solapas sin elevación por token (ej. Seguridad y Licencia exclusiva del Admin, o Empresa/SMTP para Operador)
    if (scope.isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("Acceso restringido"),
            QStringLiteral("No tienes permisos para acceder a la solapa \"%1\".\n\n"
                           "Si necesitas operar en esta sección, solicita autorización al Administrador.")
                .arg(title));
        return;
    }

    // Verificar si el usuario tiene perfil habilitado para autogenerar token temporal
    QString denialReason;
    if (!ElevationManager::instance().canCurrentUserRequestElevation(scope, &denialReason)) {
        QMessageBox::warning(this, QStringLiteral("Permisos insuficientes"), denialReason);
        return;
    }

    // Usuario Supervisor / Responsable -> abrir diálogo desatendido de elevación temporal
    ElevationDialog dlg(scope, this);
    if (dlg.exec() == QDialog::Accepted) {
        applyTabPermissions();
        m_tabs->setCurrentIndex(index);
    }
}

void PanelControlModule::onSaveElevationPolicy()
{
    if (!Session::instance().currentUser().isAdmin) {
        QMessageBox::warning(this, QStringLiteral("Solo Administrador"),
                             QStringLiteral("Solo el usuario Administrador puede modificar la política de elevación."));
        return;
    }

    ElevationPolicy pol;
    pol.durationMinutes    = m_elevDurationSpin->value();
    pol.allowSupportTables = m_elevAllowSupportCheck->isChecked();
    pol.allowBackupAudit   = m_elevAllowBackupCheck->isChecked();
    pol.allowDbAdmin       = m_elevAllowDbAdminCheck->isChecked();
    pol.notifyByEmail      = m_elevNotifyEmailCheck->isChecked();

    QString err;
    if (!ElevationManager::savePolicy(pol, &err)) {
        QMessageBox::warning(this, QStringLiteral("Error"),
                             QStringLiteral("No se pudo guardar la política de elevación:\n%1").arg(err));
        return;
    }

    Database::instance().logAudit(
        Session::instance().currentUser().email,
        moduleId(),
        QStringLiteral("elevation_policy_save"),
        QStringLiteral("Duración: %1 min | Soporte:%2 | Respaldo:%3 | SQL:%4 | Email:%5")
            .arg(pol.durationMinutes)
            .arg(pol.allowSupportTables ? 1 : 0)
            .arg(pol.allowBackupAudit ? 1 : 0)
            .arg(pol.allowDbAdmin ? 1 : 0)
            .arg(pol.notifyByEmail ? 1 : 0));

    QMessageBox::information(
        this,
        QStringLiteral("Política guardada"),
        QStringLiteral("La política de elevación desatendida para Supervisores se actualizó correctamente."));
}

void PanelControlModule::onVerifyAuditIntegrity()
{
    int brokenId = -1;
    QString report;
    const bool intact = Database::instance().verifyAuditChain(&brokenId, &report);

    m_auditIntegrityLabel->setText(report);
    if (intact) {
        m_auditIntegrityLabel->setStyleSheet(
            QStringLiteral("background-color: #E8F5E9; color: #2E7D32; border: 1px solid #C8E6C9; "
                           "padding: 6px 10px; border-radius: 5px; font-weight: 600;"));
    } else {
        m_auditIntegrityLabel->setStyleSheet(
            QStringLiteral("background-color: #FDE8E8; color: #C53030; border: 1px solid #F8B4B4; "
                           "padding: 6px 10px; border-radius: 5px; font-weight: 600;"));
        QMessageBox::critical(
            this,
            QStringLiteral("Alerta de integridad de bitácora"),
            QStringLiteral("Se detectó una alteración externa en la cadena SHA-256 de la bitácora.\n\n%1").arg(report));
    }
}
