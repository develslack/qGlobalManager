#include "mainWindow.h"
#include "core/icons.h"
#include "core/moduleBase.h"
#include "core/session.h"
#include "loginDialog.h"

#include <QApplication>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>

namespace {
constexpr int kCompactBreakpointPx = 800;
}

// Al añadir un módulo nuevo en src/modules/<nombre>/<nombre>.h, incluye su header aquí
// y agrega una sola llamada a registerModule(...) dentro de registerModules().

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Global Manager"));
    setMinimumSize(360, 320);

    setupUi();
    registerModules();
    applyResponsiveInitialGeometry();

    Session &session = Session::instance();
    connect(&session, &Session::sessionStarted, this, &MainWindow::onSessionStarted);
    connect(&session, &Session::sessionEnded, this, &MainWindow::onSessionEnded);
    connect(&session, &Session::passwordPolicyResolved, this, &MainWindow::onPasswordPolicyResolved);
    connect(&session, &Session::inactivityWarningStarted, this, &MainWindow::onInactivityWarningStarted);
    connect(&session, &Session::inactivityCountdownTick, this, &MainWindow::onInactivityCountdownTick);

    if (session.isAuthenticated()) {
        onSessionStarted();
    }
}

MainWindow::~MainWindow()
{
    closeWarningDialog();
}

void MainWindow::setupUi()
{
    auto *central = new QWidget(this);
    auto *outerLayout = new QVBoxLayout(central);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // Barra superior corporativa: [Menú] [Sección actual] ....... [Panel de Control]
    auto *topBar = new QFrame(central);
    topBar->setObjectName(QStringLiteral("TopBar"));
    topBar->setFrameShape(QFrame::NoFrame);
    auto *topBarLayout = new QHBoxLayout(topBar);
    topBarLayout->setContentsMargins(10, 8, 12, 8);
    topBarLayout->setSpacing(10);

    m_menuToggleBtn = new QPushButton(QStringLiteral("Menú"), topBar);
    m_menuToggleBtn->setIcon(Icons::get(Icons::Name::Menu, QColor(QStringLiteral("#FFFFFF")), 16));
    m_menuToggleBtn->setCheckable(true);
    m_menuToggleBtn->setChecked(true);
    m_menuToggleBtn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

    m_currentSectionLabel = new QLabel(QStringLiteral("<b>Inicio</b>"), topBar);
    m_currentSectionLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_controlPanelBtn = new QPushButton(QStringLiteral("Panel de Control"), topBar);
    m_controlPanelBtn->setObjectName(QStringLiteral("ControlPanelBtn"));
    m_controlPanelBtn->setIcon(Icons::get(Icons::Name::ControlPanel, QColor(QStringLiteral("#FFFFFF")), 16));
    m_controlPanelBtn->setToolTip(QStringLiteral("Configuración general, SMTP, impresoras, seguridad y tablas soporte"));
    m_controlPanelBtn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

    topBarLayout->addWidget(m_menuToggleBtn);
    topBarLayout->addWidget(m_currentSectionLabel, 1);
    topBarLayout->addWidget(m_controlPanelBtn);
    outerLayout->addWidget(topBar);

    // Cuerpo principal: panel lateral adaptable + área de módulos scrollable
    auto *bodyWidget = new QWidget(central);
    auto *bodyLayout = new QHBoxLayout(bodyWidget);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    m_navPanel = new QWidget(bodyWidget);
    m_navPanel->setObjectName(QStringLiteral("NavPanel"));
    m_navPanel->setMinimumWidth(180);
    m_navPanel->setMaximumWidth(240);
    m_navPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    auto *navLayout = new QVBoxLayout(m_navPanel);
    navLayout->setContentsMargins(10, 14, 10, 12);
    navLayout->setSpacing(10);

    auto *brandBox = new QHBoxLayout();
    brandBox->setSpacing(8);
    auto *brandIcon = new QLabel(m_navPanel);
    brandIcon->setFixedSize(28, 28);
    brandIcon->setScaledContents(true);
    brandIcon->setPixmap(QIcon(QStringLiteral(":/icons/qGlobalManager.svg")).pixmap(28, 28));

    auto *brandLabel = new QLabel(QStringLiteral("<b>Global Manager</b>"), m_navPanel);
    brandLabel->setWordWrap(true);

    brandBox->addWidget(brandIcon);
    brandBox->addWidget(brandLabel, 1);
    navLayout->addLayout(brandBox);

    m_sidebar = new QListWidget(m_navPanel);
    m_sidebar->setObjectName(QStringLiteral("SidebarList"));
    m_sidebar->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    navLayout->addWidget(m_sidebar, 1);

    auto *logoutButton = new QPushButton(QStringLiteral("Cerrar sesión"), m_navPanel);
    logoutButton->setIcon(Icons::get(Icons::Name::Logout, QColor(QStringLiteral("#1E3A5F")), 16));
    logoutButton->setProperty("secondary", true);
    navLayout->addWidget(logoutButton);

    m_stack = new QStackedWidget(bodyWidget);
    m_stack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Página de bienvenida envuelta en QScrollArea
    m_welcomePage = new QScrollArea(m_stack);
    m_welcomePage->setWidgetResizable(true);
    m_welcomePage->setFrameShape(QFrame::NoFrame);

    auto *welcomeContent = new QWidget(m_welcomePage);
    auto *welcomeLayout = new QVBoxLayout(welcomeContent);
    welcomeLayout->setContentsMargins(24, 24, 24, 24);
    m_welcomeLabel = new QLabel(welcomeContent);
    m_welcomeLabel->setAlignment(Qt::AlignCenter);
    m_welcomeLabel->setWordWrap(true);
    welcomeLayout->addStretch();
    welcomeLayout->addWidget(m_welcomeLabel);
    welcomeLayout->addStretch();
    m_welcomePage->setWidget(welcomeContent);

    m_stack->addWidget(m_welcomePage);

    bodyLayout->addWidget(m_navPanel);
    bodyLayout->addWidget(m_stack, 1);
    outerLayout->addWidget(bodyWidget, 1);

    setCentralWidget(central);

    m_userStatusLabel = new QLabel(this);
    m_userStatusLabel->setMinimumWidth(0);
    statusBar()->addPermanentWidget(m_userStatusLabel);

    connect(m_menuToggleBtn, &QPushButton::clicked, this, &MainWindow::onToggleSidebarClicked);
    connect(m_controlPanelBtn, &QPushButton::clicked, this, &MainWindow::onControlPanelClicked);
    connect(m_sidebar, &QListWidget::currentRowChanged, this, &MainWindow::onModuleSelected);
    connect(logoutButton, &QPushButton::clicked, this, &MainWindow::onLogoutClicked);
}

void MainWindow::applyResponsiveInitialGeometry()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        resize(960, 640);
        return;
    }

    const QRect avail = screen->availableGeometry();

    if (avail.width() <= 1024 || avail.height() <= 640) {
        resize(avail.size());
        setWindowState(windowState() | Qt::WindowMaximized);
    } else {
        const int targetW = qMin(1200, static_cast<int>(avail.width() * 0.85));
        const int targetH = qMin(760, static_cast<int>(avail.height() * 0.85));
        resize(targetW, targetH);
        move(avail.center() - rect().center());
    }

    updateResponsiveLayout(width());
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateResponsiveLayout(event->size().width());
}

void MainWindow::updateResponsiveLayout(int windowWidth)
{
    const bool shouldBeCompact = (windowWidth < kCompactBreakpointPx);
    if (shouldBeCompact == m_compactMode) {
        return;
    }

    m_compactMode = shouldBeCompact;
    m_navPanel->setVisible(!m_compactMode);
    m_menuToggleBtn->setChecked(!m_compactMode);
}

void MainWindow::onToggleSidebarClicked()
{
    const bool showSidebar = m_menuToggleBtn->isChecked();
    m_navPanel->setVisible(showSidebar);
}

void MainWindow::onControlPanelClicked()
{
    if (openModuleById(QStringLiteral("panel_control"))) {
        m_sidebar->blockSignals(true);
        m_sidebar->clearSelection();
        m_sidebar->setCurrentRow(-1);
        m_sidebar->blockSignals(false);
    }
}

void MainWindow::registerModules()
{
    // Registro centralizado de módulos (1 línea por módulo):
    // En el Paso 2B habilitaremos:
    // registerModule("panel_control", "Panel de Control", [](QWidget *p) { return new PanelControlModule(p); }, false);
    // registerModule("usuarios", "Usuarios", [](QWidget *p) { return new UsuariosModule(p); });
}

void MainWindow::registerModule(const QString &id,
                                const QString &title,
                                std::function<ModuleBase *(QWidget *)> factory,
                                bool showInSidebar)
{
    m_registry.append(ModuleEntry{id, title, showInSidebar, std::move(factory)});
}

void MainWindow::rebuildSidebar()
{
    m_sidebar->blockSignals(true);
    m_sidebar->clear();
    m_sidebarModuleIds.clear();

    auto *homeItem = new QListWidgetItem(
        Icons::get(Icons::Name::Home, QColor(QStringLiteral("#2B7A78")), 16),
        QStringLiteral("Inicio"));
    m_sidebar->addItem(homeItem);

    for (const ModuleEntry &entry : m_registry) {
        if (!entry.showInSidebar) {
            continue;
        }
        m_sidebarModuleIds.append(entry.id);
        m_sidebar->addItem(entry.title);
    }

    m_sidebar->setCurrentRow(0);
    m_sidebar->blockSignals(false);
}

bool MainWindow::openModuleById(const QString &moduleId)
{
    if (Session::instance().mustChangePassword() && moduleId != QStringLiteral("panel_control")) {
        QMessageBox::warning(
            this,
            QStringLiteral("Cambio de contraseña obligatorio"),
            QStringLiteral("Por seguridad, debes cambiar tu contraseña inicial en el Panel de Control "
                           "antes de abrir otros módulos."));
        return false;
    }

    const ModuleEntry *targetEntry = nullptr;
    for (const ModuleEntry &entry : m_registry) {
        if (entry.id == moduleId) {
            targetEntry = &entry;
            break;
        }
    }

    if (!targetEntry) {
        QMessageBox::information(
            this,
            QStringLiteral("Módulo pendiente"),
            QStringLiteral("El módulo \"%1\" aún no se encuentra registrado.").arg(moduleId));
        return false;
    }

    if (!Session::instance().hasPermission(targetEntry->id, QStringLiteral("access"))) {
        QMessageBox::warning(
            this,
            QStringLiteral("Acceso denegado"),
            QStringLiteral("No tienes permisos para acceder al módulo \"%1\".")
                .arg(targetEntry->title));
        return false;
    }

    LoadedModule loaded = m_loadedModules.value(targetEntry->id);
    if (!loaded.module) {
        auto *scrollContainer = new QScrollArea(m_stack);
        scrollContainer->setWidgetResizable(true);
        scrollContainer->setFrameShape(QFrame::NoFrame);

        ModuleBase *module = targetEntry->factory(scrollContainer);
        module->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        scrollContainer->setWidget(module);

        loaded.container = scrollContainer;
        loaded.module = module;

        m_loadedModules.insert(targetEntry->id, loaded);
        m_stack->addWidget(scrollContainer);
    }

    m_stack->setCurrentWidget(loaded.container);
    m_currentSectionLabel->setText(QStringLiteral("<b>%1</b>").arg(targetEntry->title.toHtmlEscaped()));
    loaded.module->onActivated();

    if (m_compactMode) {
        m_navPanel->hide();
        m_menuToggleBtn->setChecked(false);
    }
    return true;
}

void MainWindow::onModuleSelected(int row)
{
    if (row < 0) {
        return;
    }

    if (row == 0) {
        m_stack->setCurrentWidget(m_welcomePage);
        m_currentSectionLabel->setText(QStringLiteral("<b>Inicio</b>"));
        if (m_compactMode) {
            m_navPanel->hide();
            m_menuToggleBtn->setChecked(false);
        }
        return;
    }

    const int idx = row - 1;
    if (idx < 0 || idx >= m_sidebarModuleIds.size()) {
        return;
    }

    if (!openModuleById(m_sidebarModuleIds.at(idx))) {
        m_sidebar->setCurrentRow(0);
    }
}

void MainWindow::onSessionStarted()
{
    const UserSessionInfo &user = Session::instance().currentUser();

    m_controlPanelBtn->setVisible(Session::instance().hasPermission(QStringLiteral("panel_control"), QStringLiteral("access")));

    m_userStatusLabel->setText(
        QStringLiteral("Usuario: %1%2")
            .arg(user.email,
                 user.isAdmin ? QStringLiteral(" [Admin]") : QString()));

    QString welcomeHtml = QStringLiteral(
        "<h2 style='color:#1E3A5F;'>Bienvenido, %1</h2>"
        "<p style='color:#52606D;'>Selecciona un módulo desde el menú para comenzar a trabajar.</p>")
                              .arg(user.fullName.toHtmlEscaped());

    if (user.mustChangePassword) {
        welcomeHtml += QStringLiteral(
            "<p style='background-color:#FFF3CD; color:#664D03; padding:10px; "
            "border:1px solid #FFECB5; border-radius:6px; margin-top:12px;'>"
            "<b>Seguridad obligatoria:</b> Debes cambiar tu contraseña inicial desde el "
            "<b>Panel de Control</b> antes de operar con normalidad.</p>");
    }

    m_welcomeLabel->setText(welcomeHtml);

    rebuildSidebar();
    m_stack->setCurrentWidget(m_welcomePage);
    m_currentSectionLabel->setText(QStringLiteral("<b>Inicio</b>"));
    show();

    // Si el administrador inicia por primera vez y el Panel de Control ya está registrado, llevarlo directo
    if (user.mustChangePassword && user.isAdmin) {
        for (const ModuleEntry &entry : m_registry) {
            if (entry.id == QStringLiteral("panel_control")) {
                onControlPanelClicked();
                break;
            }
        }
    }
}

void MainWindow::onPasswordPolicyResolved()
{
    const UserSessionInfo &user = Session::instance().currentUser();
    m_welcomeLabel->setText(
        QStringLiteral("<h2 style='color:#1E3A5F;'>Bienvenido, %1</h2>"
                       "<p style='color:#52606D;'>Selecciona un módulo desde el menú para comenzar a trabajar.</p>")
            .arg(user.fullName.toHtmlEscaped()));
}

void MainWindow::onSessionEnded(bool expiredByInactivity)
{
    closeWarningDialog();
    clearLoadedModules();
    hide();

    LoginDialog login;
    if (expiredByInactivity) {
        login.setInfoBanner(
            QStringLiteral("Tu sesión caducó automáticamente por inactividad. "
                           "Vuelve a iniciar sesión para continuar."));
    } else {
        login.setInfoBanner(QStringLiteral("Has cerrado tu sesión de forma segura."));
    }

    if (login.exec() != QDialog::Accepted) {
        qApp->quit();
    }
}

void MainWindow::onInactivityWarningStarted(int secondsRemaining)
{
    closeWarningDialog();

    m_warningBox = new QMessageBox(this);
    m_warningBox->setIcon(QMessageBox::Warning);
    m_warningBox->setWindowTitle(QStringLiteral("Sesión por caducar"));
    m_warningBox->setText(
        QStringLiteral("No se ha detectado actividad en el sistema.\n"
                       "Por seguridad, tu sesión se cerrará en %1 segundos.\n\n"
                       "¿Deseas mantener la sesión activa?")
            .arg(secondsRemaining));

    QPushButton *keepBtn = m_warningBox->addButton(
        QStringLiteral("Mantener sesión"), QMessageBox::AcceptRole);
    keepBtn->setIcon(Icons::get(Icons::Name::Check, QColor(QStringLiteral("#FFFFFF")), 16));

    QPushButton *logoutBtn = m_warningBox->addButton(
        QStringLiteral("Cerrar sesión"), QMessageBox::DestructiveRole);
    logoutBtn->setIcon(Icons::get(Icons::Name::Logout, QColor(QStringLiteral("#1E3A5F")), 16));
    logoutBtn->setProperty("secondary", true);

    m_warningBox->setDefaultButton(keepBtn);

    connect(keepBtn, &QPushButton::clicked, this, [this]() {
        Session::instance().keepAlive();
        closeWarningDialog();
    });

    connect(logoutBtn, &QPushButton::clicked, this, [this]() {
        closeWarningDialog();
        Session::instance().logout(false);
    });

    m_warningBox->setModal(true);
    m_warningBox->show();
}

void MainWindow::onInactivityCountdownTick(int secondsRemaining)
{
    if (!m_warningBox) {
        return;
    }
    m_warningBox->setText(
        QStringLiteral("No se ha detectado actividad en el sistema.\n"
                       "Por seguridad, tu sesión se cerrará en %1 segundos.\n\n"
                       "¿Deseas mantener la sesión activa?")
            .arg(secondsRemaining));
}

void MainWindow::onLogoutClicked()
{
    const auto answer = QMessageBox::question(
        this,
        QStringLiteral("Cerrar sesión"),
        QStringLiteral("¿Seguro que deseas cerrar tu sesión actual?"));

    if (answer == QMessageBox::Yes) {
        Session::instance().logout(false);
    }
}

void MainWindow::clearLoadedModules()
{
    for (auto it = m_loadedModules.begin(); it != m_loadedModules.end(); ++it) {
        m_stack->removeWidget(it.value().container);
        it.value().container->deleteLater();
    }
    m_loadedModules.clear();
    m_stack->setCurrentWidget(m_welcomePage);
}

void MainWindow::closeWarningDialog()
{
    if (m_warningBox) {
        m_warningBox->hide();
        m_warningBox->deleteLater();
        m_warningBox = nullptr;
    }
}
