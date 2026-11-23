#include "setupWizard.h"
#include "core/db.h"
#include "core/icons.h"
#include "core/userCredentials.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHostInfo>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScreen>
#include <QScrollArea>
#include <QSpinBox>
#include <QSqlDatabase>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QButtonGroup>

namespace {

QScrollArea *wrapPageInScroll(QWidget *content, QWidget *parent)
{
    auto *scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    return scroll;
}

} // namespace

SetupWizard::SetupWizard(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Global Manager - Asistente de Configuración Inicial"));
    setModal(true);
    setMinimumSize(380, 420);

    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        resize(qMin(720, static_cast<int>(avail.width() * 0.85)),
               qMin(640, static_cast<int>(avail.height() * 0.85)));
    } else {
        resize(680, 580);
    }

    setupUi();
    goToPage(PageModeSelection);
}

void SetupWizard::setupUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 14, 16, 14);
    rootLayout->setSpacing(12);

    // Banner corporativo superior
    auto *banner = new QFrame(this);
    banner->setObjectName(QStringLiteral("ModuleBanner"));
    auto *bannerLayout = new QHBoxLayout(banner);
    bannerLayout->setContentsMargins(16, 12, 16, 12);
    bannerLayout->setSpacing(14);

    auto *logoLbl = new QLabel(banner);
    logoLbl->setFixedSize(44, 44);
    logoLbl->setScaledContents(true);
    logoLbl->setPixmap(QIcon(QStringLiteral(":/icons/qGlobalManager.svg")).pixmap(44, 44));
    bannerLayout->addWidget(logoLbl);

    auto *bannerText = new QVBoxLayout();
    auto *titleLbl = new QLabel(QStringLiteral("Configuración Inicial de Global Manager"), banner);
    titleLbl->setStyleSheet(QStringLiteral("font-size: 14pt; font-weight: 700; color: #FFFFFF;"));
    titleLbl->setWordWrap(true);

    m_stepSubtitle = new QLabel(banner);
    m_stepSubtitle->setStyleSheet(QStringLiteral("font-size: 9.5pt; color: #DCE6F1;"));
    m_stepSubtitle->setWordWrap(true);

    bannerText->addWidget(titleLbl);
    bannerText->addWidget(m_stepSubtitle);
    bannerLayout->addLayout(bannerText, 1);
    rootLayout->addWidget(banner);

    // Contenedor de pasos
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(wrapPageInScroll(buildModeSelectionPage(), m_stack));
    m_stack->addWidget(wrapPageInScroll(buildSinglePcUsersPage(), m_stack));
    m_stack->addWidget(wrapPageInScroll(buildNetworkConfigPage(), m_stack));
    m_stack->addWidget(wrapPageInScroll(buildNetworkAdminPage(), m_stack));
    rootLayout->addWidget(m_stack, 1);

    // Botonera inferior
    auto *btnRow = new QHBoxLayout();
    m_cancelBtn = new QPushButton(QStringLiteral("Cancelar"), this);
    m_cancelBtn->setIcon(Icons::get(Icons::Name::Close, QColor(QStringLiteral("#1E3A5F")), 15));
    m_cancelBtn->setProperty("secondary", true);

    m_backBtn = new QPushButton(QStringLiteral("Anterior"), this);
    m_backBtn->setIcon(Icons::get(Icons::Name::Refresh, QColor(QStringLiteral("#1E3A5F")), 15));
    m_backBtn->setProperty("secondary", true);

    m_nextBtn = new QPushButton(QStringLiteral("Siguiente"), this);
    m_nextBtn->setIcon(Icons::get(Icons::Name::Check, QColor(QStringLiteral("#FFFFFF")), 15));
    m_nextBtn->setDefault(true);

    btnRow->addWidget(m_cancelBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_backBtn);
    btnRow->addWidget(m_nextBtn);
    rootLayout->addLayout(btnRow);

    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_backBtn, &QPushButton::clicked, this, &SetupWizard::onBackClicked);
    connect(m_nextBtn, &QPushButton::clicked, this, &SetupWizard::onNextOrFinishClicked);
}

QWidget *SetupWizard::buildModeSelectionPage()
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(14);

    auto *intro = new QLabel(
        QStringLiteral("Antes de crear la base de datos, indícanos cómo se utilizará <b>Global Manager</b> "
                       "en tu comercio o consultorio:"),
        page);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    // Opción 1: Puesto único (SQLite)
    auto *singleGroup = new QGroupBox(QStringLiteral("Opción 1: Instalación de Puesto Único (1 sola PC)"), page);
    auto *singleLayout = new QVBoxLayout(singleGroup);

    m_singlePcRadio = new QRadioButton(
        QStringLiteral("Usar en una única computadora (Base de datos local SQLite protegida)"), singleGroup);
    m_singlePcRadio->setChecked(true);

    auto *singleDesc = new QLabel(
        QStringLiteral("Ideal para kioskos, comercios pequeños o consultorios individuales. "
                       "El sistema creará automáticamente la base local oculta, un usuario Administrador "
                       "y un usuario separado para la operación diaria (principio de seguridad UNIX)."),
        singleGroup);
    singleDesc->setWordWrap(true);
    singleDesc->setStyleSheet(QStringLiteral("color: #52606D; margin-left: 22px;"));

    singleLayout->addWidget(m_singlePcRadio);
    singleLayout->addWidget(singleDesc);
    layout->addWidget(singleGroup);

    // Opción 2: Múltiples terminales en red (MariaDB)
    auto *multiGroup = new QGroupBox(QStringLiteral("Opción 2: Múltiples Puestos en Red (Servidor MariaDB)"), page);
    auto *multiLayout = new QVBoxLayout(multiGroup);

    m_multiPcRadio = new QRadioButton(
        QStringLiteral("Conectar varias PCs / terminales a un servidor central MariaDB"), multiGroup);

    auto *multiDesc = new QLabel(
        QStringLiteral("Ideal para consultorios compartidos (varios profesionales y secretarias) o comercios con varias cajas. "
                       "El Puesto #1 crea la base de datos en el servidor (ej. Raspberry Pi o PC servidor) y los puestos "
                       "adicionales (#2, #3...) se enlazan por única vez para operar sobre los mismos datos en tiempo real."),
        multiGroup);
    multiDesc->setWordWrap(true);
    multiDesc->setStyleSheet(QStringLiteral("color: #52606D; margin-left: 22px;"));

    multiLayout->addWidget(m_multiPcRadio);
    multiLayout->addWidget(multiDesc);
    layout->addWidget(multiGroup);

    // Exclusividad entre ambos QRadioButton al estar en QGroupBox distintos
    auto *modeGroup = new QButtonGroup(page);
    modeGroup->setExclusive(true);
    modeGroup->addButton(m_singlePcRadio);
    modeGroup->addButton(m_multiPcRadio);

    // Identificación amigable de esta PC
    auto *pcGroup = new QGroupBox(QStringLiteral("Identificación de esta Computadora"), page);
    auto *pcForm = new QFormLayout(pcGroup);
    pcForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_terminalNameEdit = new QLineEdit(pcGroup);
    m_terminalNameEdit->setText(QHostInfo::localHostName());
    m_terminalNameEdit->setPlaceholderText(QStringLiteral("Ej: Caja Principal, Recepción, Consultorio 2"));
    pcForm->addRow(QStringLiteral("Nombre de este puesto:"), m_terminalNameEdit);
    layout->addWidget(pcGroup);

    layout->addStretch();
    return page;
}

QWidget *SetupWizard::buildSinglePcUsersPage()
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(12);

    auto *unixNote = new QLabel(
        QStringLiteral("<b>Separación de privilegios (estilo UNIX):</b> Aunque seas la única persona que utilice el sistema, "
                       "crearás una cuenta <b>Administrador</b> para configuraciones sensibles y una cuenta <b>de uso diario</b> "
                       "para trabajar todos los días con mayor seguridad."),
        page);
    unixNote->setWordWrap(true);
    unixNote->setStyleSheet(
        QStringLiteral("background-color: #EEF4FA; color: #1E3A5F; border: 1px solid #D9E2EC; "
                       "padding: 10px; border-radius: 6px;"));
    layout->addWidget(unixNote);

    // Bloque 1: Usuario Administrador
    auto *adminGroup = new QGroupBox(QStringLiteral("1. Cuenta de Administrador del Sistema"), page);
    auto *adminForm = new QFormLayout(adminGroup);
    adminForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_singleAdminName = new QLineEdit(adminGroup);
    m_singleAdminName->setPlaceholderText(QStringLiteral("Nombre y Apellido del Administrador"));

    m_singleAdminEmail = new QLineEdit(adminGroup);
    m_singleAdminEmail->setPlaceholderText(QStringLiteral("admin@tucomercio.com"));

    m_singleAdminPass = new QLineEdit(adminGroup);
    m_singleAdminPass->setEchoMode(QLineEdit::Password);
    m_singleAdminPass->setPlaceholderText(QStringLiteral("Mín. 8 caracteres, mayúscula, minúscula y número"));

    m_singleAdminConfirm = new QLineEdit(adminGroup);
    m_singleAdminConfirm->setEchoMode(QLineEdit::Password);
    m_singleAdminConfirm->setPlaceholderText(QStringLiteral("Repite la contraseña del Administrador"));

    adminForm->addRow(QStringLiteral("Nombre y Apellido *:"), m_singleAdminName);
    adminForm->addRow(QStringLiteral("Email (Usuario) *:"), m_singleAdminEmail);
    adminForm->addRow(QStringLiteral("Contraseña *:"), m_singleAdminPass);
    adminForm->addRow(QStringLiteral("Confirmar contraseña *:"), m_singleAdminConfirm);
    layout->addWidget(adminGroup);

    // Bloque 2: Usuario común para operación diaria
    auto *dailyGroup = new QGroupBox(QStringLiteral("2. Cuenta de Uso Diario (Operación Cotidiana)"), page);
    auto *dailyForm = new QFormLayout(dailyGroup);
    dailyForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_dailyUserName = new QLineEdit(dailyGroup);
    m_dailyUserName->setPlaceholderText(QStringLiteral("Nombre y Apellido (puede ser tu mismo nombre)"));

    m_dailyUserEmail = new QLineEdit(dailyGroup);
    m_dailyUserEmail->setPlaceholderText(QStringLiteral("operacion@tucomercio.com (distinto al email Admin)"));

    m_dailyUserRole = new QComboBox(dailyGroup);
    m_dailyUserRole->addItem(QStringLiteral("Responsable / Supervisor"), QStringLiteral("Responsable / Supervisor"));
    m_dailyUserRole->addItem(QStringLiteral("Operador"), QStringLiteral("Operador"));

    m_dailyUserPass = new QLineEdit(dailyGroup);
    m_dailyUserPass->setEchoMode(QLineEdit::Password);
    m_dailyUserPass->setPlaceholderText(QStringLiteral("Mín. 8 caracteres, mayúscula, minúscula y número"));

    m_dailyUserConfirm = new QLineEdit(dailyGroup);
    m_dailyUserConfirm->setEchoMode(QLineEdit::Password);
    m_dailyUserConfirm->setPlaceholderText(QStringLiteral("Repite la contraseña de uso diario"));

    dailyForm->addRow(QStringLiteral("Nombre y Apellido *:"), m_dailyUserName);
    dailyForm->addRow(QStringLiteral("Email (Usuario) *:"), m_dailyUserEmail);
    dailyForm->addRow(QStringLiteral("Rol operativo *:"), m_dailyUserRole);
    dailyForm->addRow(QStringLiteral("Contraseña *:"), m_dailyUserPass);
    dailyForm->addRow(QStringLiteral("Confirmar contraseña *:"), m_dailyUserConfirm);
    layout->addWidget(dailyGroup);

    layout->addStretch();
    return page;
}

QWidget *SetupWizard::buildNetworkConfigPage()
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(12);

    // Verificación del driver QMYSQL
    m_mysqlDriverStatus = new QLabel(page);
    m_mysqlDriverStatus->setWordWrap(true);
    if (QSqlDatabase::isDriverAvailable(QStringLiteral("QMYSQL"))) {
        m_mysqlDriverStatus->setText(QStringLiteral("Driver MariaDB/MySQL (QMYSQL) disponible en este equipo."));
        m_mysqlDriverStatus->setStyleSheet(
            QStringLiteral("background-color: #E8F5E9; color: #2E7D32; border: 1px solid #C8E6C9; "
                           "padding: 8px; border-radius: 6px; font-weight: 600;"));
    } else {
        m_mysqlDriverStatus->setText(
            QStringLiteral("Atención: El driver QMYSQL de Qt no se detectó en este equipo. "
                           "Instala el soporte MariaDB de Qt antes de conectar en modo red."));
        m_mysqlDriverStatus->setStyleSheet(
            QStringLiteral("background-color: #FFF3CD; color: #664D03; border: 1px solid #FFECB5; "
                           "padding: 8px; border-radius: 6px; font-weight: 600;"));
    }
    layout->addWidget(m_mysqlDriverStatus);

    // Aviso de detección automática
    auto *autoGroup = new QGroupBox(QStringLiteral("Asignación Automática de Puesto de Trabajo"), page);
    auto *autoLayout = new QVBoxLayout(autoGroup);

    m_terminalHintLabel = new QLabel(
        QStringLiteral("Al conectar con el servidor MariaDB, <b>Global Manager</b> detectará automáticamente:\n"
                       "• Si la base todavía no existe: la creará como <b>Puesto #1</b> y te pedirá configurar el Administrador.\n"
                       "• Si la base ya existe: consultará los puestos activos y asignará automáticamente el <b>siguiente puesto disponible</b> (#2, #3...)."),
        autoGroup);
    m_terminalHintLabel->setWordWrap(true);
    m_terminalHintLabel->setStyleSheet(QStringLiteral("color: #1E3A5F;"));
    autoLayout->addWidget(m_terminalHintLabel);
    layout->addWidget(autoGroup);

    // Datos del servidor MariaDB
    auto *serverGroup = new QGroupBox(QStringLiteral("Conexión al Servidor Central MariaDB"), page);
    auto *serverForm = new QFormLayout(serverGroup);
    serverForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_dbHostEdit = new QLineEdit(serverGroup);
    m_dbHostEdit->setPlaceholderText(QStringLiteral("Ej: localhost, 127.0.0.1, 192.168.1.50 o servidor.local"));

    m_dbPortSpin = new QSpinBox(serverGroup);
    m_dbPortSpin->setRange(1, 65535);
    m_dbPortSpin->setValue(3306);
    m_dbPortSpin->setMaximumWidth(120);

    m_dbNameEdit = new QLineEdit(serverGroup);
    m_dbNameEdit->setText(QStringLiteral("qglobalmanager"));

    m_dbUserEdit = new QLineEdit(serverGroup);
    m_dbUserEdit->setPlaceholderText(QStringLiteral("Usuario de MariaDB con permisos sobre la base"));

    auto *passRow = new QHBoxLayout();
    m_dbPassEdit = new QLineEdit(serverGroup);
    m_dbPassEdit->setEchoMode(QLineEdit::Password);
    m_dbPassEdit->setPlaceholderText(QStringLiteral("Contraseña de MariaDB"));
    m_dbShowPassCheck = new QCheckBox(QStringLiteral("Mostrar"), serverGroup);
    passRow->addWidget(m_dbPassEdit, 1);
    passRow->addWidget(m_dbShowPassCheck);

    serverForm->addRow(QStringLiteral("IP o URL del servidor *:"), m_dbHostEdit);
    serverForm->addRow(QStringLiteral("Puerto *:"), m_dbPortSpin);
    serverForm->addRow(QStringLiteral("Nombre de la base *:"), m_dbNameEdit);
    serverForm->addRow(QStringLiteral("Usuario MariaDB *:"), m_dbUserEdit);
    serverForm->addRow(QStringLiteral("Contraseña MariaDB:"), passRow);
    layout->addWidget(serverGroup);

    auto *testRow = new QHBoxLayout();
    auto *testBtn = new QPushButton(QStringLiteral("Probar conexión y detectar puesto"), page);
    testBtn->setIcon(Icons::get(Icons::Name::Database, QColor(QStringLiteral("#1E3A5F")), 16));
    testBtn->setProperty("secondary", true);

    m_connectionTestResult = new QLabel(page);
    m_connectionTestResult->setWordWrap(true);

    testRow->addWidget(testBtn);
    testRow->addWidget(m_connectionTestResult, 1);
    layout->addLayout(testRow);
    layout->addStretch();

    connect(m_dbShowPassCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_dbPassEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
    });
    connect(testBtn, &QPushButton::clicked, this, &SetupWizard::onTestMariaDbClicked);

    return page;
}

QWidget *SetupWizard::buildNetworkAdminPage()
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(12);

    auto *okBanner = new QLabel(
        QStringLiteral("<b>Base de datos MariaDB creada correctamente.</b><br>"
                       "Define ahora la cuenta del <b>Administrador General</b>. Una vez que inicies sesión, "
                       "podrás dar de alta a todos los usuarios (secretarias, profesionales, vendedores) "
                       "desde el módulo <b>Usuarios</b>."),
        page);
    okBanner->setWordWrap(true);
    okBanner->setStyleSheet(
        QStringLiteral("background-color: #E8F5E9; color: #2E7D32; border: 1px solid #C8E6C9; "
                       "padding: 10px; border-radius: 6px;"));
    layout->addWidget(okBanner);

    auto *adminGroup = new QGroupBox(QStringLiteral("Cuenta de Administrador General (Puesto #1)"), page);
    auto *form = new QFormLayout(adminGroup);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_netAdminName = new QLineEdit(adminGroup);
    m_netAdminName->setPlaceholderText(QStringLiteral("Nombre y Apellido del Administrador"));

    m_netAdminEmail = new QLineEdit(adminGroup);
    m_netAdminEmail->setPlaceholderText(QStringLiteral("admin@tuempresa.com"));

    m_netAdminPass = new QLineEdit(adminGroup);
    m_netAdminPass->setEchoMode(QLineEdit::Password);
    m_netAdminPass->setPlaceholderText(QStringLiteral("Mín. 8 caracteres, mayúscula, minúscula y número"));

    m_netAdminConfirm = new QLineEdit(adminGroup);
    m_netAdminConfirm->setEchoMode(QLineEdit::Password);
    m_netAdminConfirm->setPlaceholderText(QStringLiteral("Repite la contraseña del Administrador"));

    form->addRow(QStringLiteral("Nombre y Apellido *:"), m_netAdminName);
    form->addRow(QStringLiteral("Email (Usuario) *:"), m_netAdminEmail);
    form->addRow(QStringLiteral("Contraseña *:"), m_netAdminPass);
    form->addRow(QStringLiteral("Confirmar contraseña *:"), m_netAdminConfirm);
    layout->addWidget(adminGroup);
    layout->addStretch();

    return page;
}

void SetupWizard::goToPage(int pageIndex)
{
    m_stack->setCurrentIndex(pageIndex);

    switch (pageIndex) {
    case PageModeSelection:
        m_stepSubtitle->setText(QStringLiteral("Paso 1 de 2 · Selecciona el modo de instalación (Puesto Único o Red)"));
        m_backBtn->setVisible(false);
        m_nextBtn->setText(QStringLiteral("Siguiente"));
        break;
    case PageSinglePcUsers:
        m_stepSubtitle->setText(QStringLiteral("Paso 2 de 2 · Creación de base local SQLite y usuarios iniciales (Admin + Uso Diario)"));
        m_backBtn->setVisible(true);
        m_nextBtn->setText(QStringLiteral("Crear base e inicializar"));
        m_singleAdminName->setFocus();
        break;
    case PageNetworkConfig:
        m_stepSubtitle->setText(QStringLiteral("Paso 2 · Conexión al servidor MariaDB (detección automática de base y puesto)"));
        m_backBtn->setVisible(true);
        m_nextBtn->setText(QStringLiteral("Conectar y continuar"));
        m_dbHostEdit->setFocus();
        break;
    case PageNetworkAdmin:
        m_stepSubtitle->setText(QStringLiteral("Paso 3 · Define el Administrador General para el servidor MariaDB"));
        m_backBtn->setVisible(false);
        m_nextBtn->setText(QStringLiteral("Finalizar e ir al Login"));
        m_netAdminName->setFocus();
        break;
    }
}

void SetupWizard::onTerminalNumberChanged(int value)
{
    if (value == 1) {
        m_terminalHintLabel->setText(
            QStringLiteral("<b>Puesto #1 (Inicializador):</b> El sistema se conectará al servidor MariaDB, "
                           "creará la base de datos con todas sus tablas y te pedirá definir el Administrador."));
        if (m_stack->currentIndex() == PageNetworkConfig) {
            m_nextBtn->setText(QStringLiteral("Conectar y crear base"));
        }
    } else {
        m_terminalHintLabel->setText(
            QStringLiteral("<b>Puesto #%1 (Terminal adicional):</b> La base MariaDB ya debe estar creada desde el Puesto #1. "
                           "Este equipo se enlazará por única vez y te llevará directo a la pantalla de Login.")
                .arg(value));
        if (m_stack->currentIndex() == PageNetworkConfig) {
            m_nextBtn->setText(QStringLiteral("Enlazar puesto e ir al Login"));
        }
    }
}

BootstrapData SetupWizard::collectMariaDbConfig() const
{
    BootstrapData cfg;
    cfg.initialized    = true;
    cfg.engine         = QStringLiteral("MARIADB");
    cfg.terminalNumber = qMax(1, m_detectedTerminalNumber);
    cfg.terminalName   = m_terminalNameEdit->text().trimmed();
    cfg.dbHost         = m_dbHostEdit->text().trimmed();
    cfg.dbPort         = m_dbPortSpin->value();
    cfg.dbName         = m_dbNameEdit->text().trimmed();
    cfg.dbUser         = m_dbUserEdit->text().trimmed();
    cfg.dbPassword     = m_dbPassEdit->text();
    return cfg;
}

void SetupWizard::onTestMariaDbClicked()
{
    const BootstrapData cfg = collectMariaDbConfig();
    if (cfg.dbHost.isEmpty() || cfg.dbUser.isEmpty() || cfg.dbName.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Datos incompletos"),
                             QStringLiteral("Completa la IP/URL del servidor, el nombre de la base y el usuario MariaDB."));
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    MariaDbServerStatus status;
    QString err;
    const bool ok = Database::inspectMariaDbServer(cfg, &status, &err);
    QApplication::restoreOverrideCursor();

    if (!ok) {
        m_connectionTestResult->setText(err);
        m_connectionTestResult->setStyleSheet(QStringLiteral("color: #C53030; font-weight: 600;"));
        return;
    }

    if (status.schemaExists && status.hasUsers) {
        m_detectedTerminalNumber = status.nextAvailableTerminal;
        m_connectionTestResult->setText(
            QStringLiteral("Base existente detectada (%1 puesto(s) configurado(s)) · "
                           "Esta PC tomará automáticamente el Puesto #%2.")
                .arg(status.registeredWorkstations)
                .arg(m_detectedTerminalNumber));
        m_connectionTestResult->setStyleSheet(QStringLiteral("color: #2E7D32; font-weight: 600;"));
        m_nextBtn->setText(QStringLiteral("Enlazar como Puesto #%1 e ir al Login").arg(m_detectedTerminalNumber));
    } else {
        m_detectedTerminalNumber = 1;
        m_connectionTestResult->setText(
            QStringLiteral("Servidor listo · No existe una base inicializada. "
                           "Se creará la base como Puesto #1."));
        m_connectionTestResult->setStyleSheet(QStringLiteral("color: #1E3A5F; font-weight: 600;"));
        m_nextBtn->setText(QStringLiteral("Crear base y continuar (Puesto #1)"));
    }
}

void SetupWizard::onBackClicked()
{
    if (m_stack->currentIndex() == PageSinglePcUsers || m_stack->currentIndex() == PageNetworkConfig) {
        goToPage(PageModeSelection);
    }
}

void SetupWizard::onNextOrFinishClicked()
{
    switch (m_stack->currentIndex()) {
    case PageModeSelection:
        if (m_singlePcRadio->isChecked()) {
            goToPage(PageSinglePcUsers);
        } else {
            goToPage(PageNetworkConfig);
        }
        break;
    case PageSinglePcUsers:
        if (finishSinglePcSetup()) {
            accept();
        }
        break;
    case PageNetworkConfig:
        if (handleNetworkConfigNext()) {
            // Si era Puesto #2+ (o Puesto #1 enlazado a base existente), termina directo
            if (m_stack->currentIndex() == PageNetworkConfig) {
                accept();
            }
        }
        break;
    case PageNetworkAdmin:
        if (finishNetworkAdminSetup()) {
            accept();
        }
        break;
    }
}

bool SetupWizard::finishSinglePcSetup()
{
    const QString adminName  = m_singleAdminName->text().trimmed();
    const QString adminEmail = m_singleAdminEmail->text().trimmed().toLower();
    const QString adminPass  = m_singleAdminPass->text();
    const QString adminConf  = m_singleAdminConfirm->text();

    const QString dailyName  = m_dailyUserName->text().trimmed();
    const QString dailyEmail = m_dailyUserEmail->text().trimmed().toLower();
    const QString dailyRole  = m_dailyUserRole->currentData().toString();
    const QString dailyPass  = m_dailyUserPass->text();
    const QString dailyConf  = m_dailyUserConfirm->text();

    if (adminName.isEmpty() || !UserCredentials::isValidEmail(adminEmail)) {
        QMessageBox::warning(this, QStringLiteral("Datos de Administrador inválidos"),
                             QStringLiteral("Ingresa Nombre y Apellido y un Email válido para el Administrador."));
        return false;
    }

    QString pwdReason;
    if (adminPass != adminConf) {
        QMessageBox::warning(this, QStringLiteral("Contraseña de Administrador"),
                             QStringLiteral("La contraseña del Administrador y su confirmación no coinciden."));
        return false;
    }
    if (!UserCredentials::isStrongPassword(adminPass, &pwdReason)) {
        QMessageBox::warning(this, QStringLiteral("Contraseña de Administrador débil"), pwdReason);
        return false;
    }

    if (dailyName.isEmpty() || !UserCredentials::isValidEmail(dailyEmail)) {
        QMessageBox::warning(this, QStringLiteral("Datos de Usuario Diario inválidos"),
                             QStringLiteral("Ingresa Nombre y Apellido y un Email válido para el usuario de operación diaria."));
        return false;
    }

    if (dailyEmail == adminEmail) {
        QMessageBox::warning(this, QStringLiteral("Emails iguales"),
                             QStringLiteral("El correo del usuario de operación diaria debe ser distinto al del Administrador "
                                            "para mantener separadas ambas cuentas."));
        return false;
    }

    if (dailyPass != dailyConf) {
        QMessageBox::warning(this, QStringLiteral("Contraseña de Usuario Diario"),
                             QStringLiteral("La contraseña del usuario diario y su confirmación no coinciden."));
        return false;
    }
    if (!UserCredentials::isStrongPassword(dailyPass, &pwdReason)) {
        QMessageBox::warning(this, QStringLiteral("Contraseña de Usuario Diario débil"), pwdReason);
        return false;
    }

    BootstrapData cfg;
    cfg.initialized    = true;
    cfg.engine         = QStringLiteral("SQLITE");
    cfg.terminalNumber = 1;
    cfg.terminalName   = m_terminalNameEdit->text().trimmed();
    cfg.sqlitePath     = BootstrapConfig::defaultSqlitePath();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString err;
    const bool initOk = Database::instance().initializeWithConfig(cfg, true, &err);
    if (!initOk) {
        QApplication::restoreOverrideCursor();
        QMessageBox::critical(this, QStringLiteral("Error creando base local"), err);
        return false;
    }

    if (!Database::instance().hasAnyUsers()) {
        if (!Database::instance().createInitialAccounts(
                adminName, adminEmail, adminPass,
                true, dailyName, dailyEmail, dailyPass, dailyRole, &err)) {
            QApplication::restoreOverrideCursor();
            QMessageBox::critical(this, QStringLiteral("Error creando usuarios"), err);
            return false;
        }
    }

    Database::instance().registerCurrentWorkstation(cfg.terminalNumber, cfg.terminalName);
    Database::instance().createAutomaticBackup();

    if (!BootstrapConfig::save(cfg, &err)) {
        QApplication::restoreOverrideCursor();
        QMessageBox::critical(this, QStringLiteral("Error guardando configuración"), err);
        return false;
    }
    QApplication::restoreOverrideCursor();

    QMessageBox::information(
        this,
        QStringLiteral("Instalación completada"),
        QStringLiteral("Global Manager se inicializó correctamente en modo Puesto Único.\n\n"
                       "• Administrador: %1\n"
                       "• Usuario de uso diario: %2 (%3)\n\n"
                       "Ahora puedes iniciar sesión.")
            .arg(adminEmail, dailyEmail, dailyRole));
    return true;
}

bool SetupWizard::handleNetworkConfigNext()
{
    BootstrapData cfg = collectMariaDbConfig();
    if (cfg.dbHost.isEmpty() || cfg.dbUser.isEmpty() || cfg.dbName.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Datos incompletos"),
                             QStringLiteral("Completa la IP/URL del servidor, nombre de base y usuario de MariaDB."));
        return false;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    MariaDbServerStatus status;
    QString err;
    if (!Database::inspectMariaDbServer(cfg, &status, &err)) {
        QApplication::restoreOverrideCursor();
        QMessageBox::critical(this, QStringLiteral("No se pudo conectar"), err);
        return false;
    }

    // CASO A: La base YA existe y ya tiene usuarios -> Enlace automático como siguiente puesto disponible
    if (status.schemaExists && status.hasUsers) {
        if (!Database::instance().initializeWithConfig(cfg, false, &err)) {
            QApplication::restoreOverrideCursor();
            QMessageBox::critical(this, QStringLiteral("Error enlazando puesto"), err);
            return false;
        }

        const int assignedTerminal = Database::instance().nextAvailableTerminalNumber();
        m_detectedTerminalNumber = assignedTerminal;
        cfg.terminalNumber = assignedTerminal;

        Database::instance().registerCurrentWorkstation(cfg.terminalNumber, cfg.terminalName);
        if (!BootstrapConfig::save(cfg, &err)) {
            QApplication::restoreOverrideCursor();
            QMessageBox::critical(this, QStringLiteral("Error guardando enlace"), err);
            return false;
        }
        QApplication::restoreOverrideCursor();

        QMessageBox::information(
            this,
            QStringLiteral("Puesto enlazado automáticamente"),
            QStringLiteral("Se detectó la base de datos \"%1\" en %2.\n\n"
                           "A esta computadora se le asignó automáticamente el Puesto #%3.\n\n"
                           "Ahora ingresa con tu usuario y contraseña.")
                .arg(cfg.dbName, cfg.dbHost)
                .arg(cfg.terminalNumber));
        return true;
    }

    // CASO B: La base NO existe (o no tiene usuarios aún) -> Inicialización como Puesto #1
    m_detectedTerminalNumber = 1;
    cfg.terminalNumber = 1;

    if (!Database::instance().initializeWithConfig(cfg, true, &err)) {
        QApplication::restoreOverrideCursor();
        QMessageBox::critical(this, QStringLiteral("Error inicializando MariaDB"), err);
        return false;
    }

    QApplication::restoreOverrideCursor();
    goToPage(PageNetworkAdmin);
    return true;
}

bool SetupWizard::finishNetworkAdminSetup()
{
    const QString adminName  = m_netAdminName->text().trimmed();
    const QString adminEmail = m_netAdminEmail->text().trimmed().toLower();
    const QString adminPass  = m_netAdminPass->text();
    const QString adminConf  = m_netAdminConfirm->text();

    if (adminName.isEmpty() || !UserCredentials::isValidEmail(adminEmail)) {
        QMessageBox::warning(this, QStringLiteral("Datos inválidos"),
                             QStringLiteral("Ingresa Nombre y Apellido y un Email válido para el Administrador."));
        return false;
    }

    QString pwdReason;
    if (adminPass != adminConf) {
        QMessageBox::warning(this, QStringLiteral("Contraseñas distintas"),
                             QStringLiteral("La contraseña y su confirmación no coinciden."));
        return false;
    }
    if (!UserCredentials::isStrongPassword(adminPass, &pwdReason)) {
        QMessageBox::warning(this, QStringLiteral("Contraseña débil"), pwdReason);
        return false;
    }

    const BootstrapData cfg = collectMariaDbConfig();
    QString err;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    if (!Database::instance().createInitialAccounts(
            adminName, adminEmail, adminPass,
            false, QString(), QString(), QString(), QString(), &err)) {
        QApplication::restoreOverrideCursor();
        QMessageBox::critical(this, QStringLiteral("Error creando Administrador"), err);
        return false;
    }

    Database::instance().setSetting(QStringLiteral("setup.remind_create_users"), QStringLiteral("1"));
    Database::instance().registerCurrentWorkstation(cfg.terminalNumber, cfg.terminalName);

    if (!BootstrapConfig::save(cfg, &err)) {
        QApplication::restoreOverrideCursor();
        QMessageBox::critical(this, QStringLiteral("Error guardando configuración"), err);
        return false;
    }
    QApplication::restoreOverrideCursor();

    // Proponer generar un respaldo SQL inicial (dump de MariaDB) en la carpeta protegida local (.db/)
    const auto wantDump = QMessageBox::question(
        this,
        QStringLiteral("Respaldo inicial de MariaDB"),
        QStringLiteral("El servidor MariaDB y la cuenta Administrador (%1) quedaron inicializados correctamente.\n\n"
                       "¿Deseas generar ahora un respaldo inicial (dump .sql) de la base de datos y guardarlo "
                       "en el directorio local protegido?\n\nUbicación: %2")
            .arg(adminEmail, BootstrapConfig::protectedDbDirPath()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);

    if (wantDump == QMessageBox::Yes) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        QString dumpPath;
        QString dumpErr;
        const bool dumpOk = Database::instance().createMariaDbDump(&dumpPath, &dumpErr);
        QApplication::restoreOverrideCursor();

        if (dumpOk) {
            Database::instance().logAudit(
                adminEmail,
                QStringLiteral("setup"),
                QStringLiteral("mariadb_initial_dump"),
                QStringLiteral("Dump inicial guardado en: %1").arg(dumpPath));

            QMessageBox::information(
                this,
                QStringLiteral("Dump generado correctamente"),
                QStringLiteral("El respaldo SQL de MariaDB se guardó en:\n%1\n\n"
                               "Ahora pasarás a la pantalla de inicio de sesión.")
                    .arg(dumpPath));
        } else {
            QMessageBox::warning(
                this,
                QStringLiteral("No se pudo generar el dump"),
                QStringLiteral("La base MariaDB quedó lista, pero falló la generación del respaldo inicial:\n%1\n\n"
                               "Podrás generarlo más adelante desde el Panel de Control. "
                               "Ahora pasarás a la pantalla de inicio de sesión.")
                    .arg(dumpErr));
        }
    } else {
        QMessageBox::information(
            this,
            QStringLiteral("Servidor inicializado"),
            QStringLiteral("El servidor MariaDB y la cuenta Administrador (%1) quedaron listos.\n\n"
                           "Inicia sesión ahora para crear el resto de los usuarios del sistema.")
                .arg(adminEmail));
    }

    return true;
}
