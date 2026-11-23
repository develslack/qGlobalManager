#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QHash>
#include <QMainWindow>
#include <QString>
#include <QVector>
#include <functional>

class QFrame;
class ModuleBase;
class QLabel;
class QListWidget;
class QMessageBox;
class QPushButton;
class QResizeEvent;
class QScrollArea;
class QStackedWidget;
class QWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onModuleSelected(int row);
    void onToggleSidebarClicked();
    void onControlPanelClicked();
    void onSessionStarted();
    void onSessionEnded(bool expiredByInactivity);
    void onPasswordPolicyResolved();
    void onInactivityWarningStarted(int secondsRemaining);
    void onInactivityCountdownTick(int secondsRemaining);
    void onLogoutClicked();
    void onElevationChanged(bool active);
    void onElevationTick(int remainingSeconds);
    void onElevationExpired(const QString &scopeDisplayName);

private:
    struct ModuleEntry
    {
        QString id;
        QString title;
        bool showInSidebar = true;
        std::function<ModuleBase *(QWidget *)> factory;
    };

    struct LoadedModule
    {
        QScrollArea *container = nullptr;
        ModuleBase *module = nullptr;
    };

    void setupUi();
    void applyResponsiveInitialGeometry();
    void updateResponsiveLayout(int windowWidth);
    void registerModules();
    void registerModule(const QString &id,
                        const QString &title,
                        std::function<ModuleBase *(QWidget *)> factory,
                        bool showInSidebar = true);
    bool openModuleById(const QString &moduleId);
    void rebuildSidebar();
    void clearLoadedModules();
    void closeWarningDialog();

    QWidget *m_navPanel = nullptr;
    QPushButton *m_menuToggleBtn = nullptr;
    QPushButton *m_controlPanelBtn = nullptr;
    QLabel *m_currentSectionLabel = nullptr;
    QListWidget *m_sidebar = nullptr;
    QStackedWidget *m_stack = nullptr;
    QScrollArea *m_welcomePage = nullptr;
    QLabel *m_welcomeLabel = nullptr;
    QLabel *m_userStatusLabel = nullptr;
    QMessageBox *m_warningBox = nullptr;

    bool m_compactMode = false;

    QVector<ModuleEntry> m_registry;
    QVector<QString> m_sidebarModuleIds;
    QHash<QString, LoadedModule> m_loadedModules;

    QFrame *m_elevationBanner = nullptr;
    QLabel *m_elevationLabel = nullptr;
    QPushButton *m_revokeElevationBtn = nullptr;
};

#endif // UI_MAINWINDOW_H
