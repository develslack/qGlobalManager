#include "core/bootstrapConfig.h"
#include "core/db.h"
#include "core/icons.h"
#include "core/session.h"
#include "core/theme.h"
#include "ui/loginDialog.h"
#include "ui/mainWindow.h"
#include "ui/setupWizard.h"

#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QMessageBox>
#include <QPushButton>

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("qGlobalManager"));
    app.setApplicationDisplayName(QStringLiteral("Global Manager"));
    app.setOrganizationName(QStringLiteral("GlobalManager"));
    app.setApplicationVersion(QStringLiteral(APP_VERSION));

    Theme::apply(app);
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/qGlobalManager.svg")));

    // 1. Si es el primer arranque (no existe bootstrap.ini), lanzar Asistente de Configuración Inicial
    if (!BootstrapConfig::exists()) {
        SetupWizard wizard;
        if (wizard.exec() != QDialog::Accepted) {
            return 0;
        }
    }

    // 2. Protección anti-"app ciega": si en modo SQLite desapareció el archivo, NO crear una BD vacía en silencio
    BootstrapData bootCfg = BootstrapConfig::load();
    if (bootCfg.isSqlite() && !QFile::exists(bootCfg.sqlitePath)) {
        QMessageBox alert;
        alert.setIcon(QMessageBox::Critical);
        alert.setWindowTitle(QStringLiteral("Base de datos no encontrada"));
        alert.setText(
            QStringLiteral("Global Manager ya estaba inicializado en este equipo, pero el archivo de base de datos "
                           "no se encuentra en su ubicación protegida:\n\n%1\n\n"
                           "Para evitar perder tu información, la aplicación no creará una base vacía automáticamente.")
                .arg(bootCfg.sqlitePath));

        QPushButton *restoreBtn = alert.addButton(QStringLiteral("Restaurar respaldo (.sqlite)"), QMessageBox::AcceptRole);
        restoreBtn->setIcon(Icons::get(Icons::Name::Backup, QColor(QStringLiteral("#FFFFFF")), 15));

        QPushButton *reconfigBtn = alert.addButton(QStringLiteral("Reconfigurar instalación"), QMessageBox::ActionRole);
        reconfigBtn->setIcon(Icons::get(Icons::Name::ControlPanel, QColor(QStringLiteral("#1E3A5F")), 15));
        reconfigBtn->setProperty("secondary", true);

        QPushButton *exitBtn = alert.addButton(QStringLiteral("Salir"), QMessageBox::RejectRole);
        exitBtn->setIcon(Icons::get(Icons::Name::Close, QColor(QStringLiteral("#1E3A5F")), 15));
        exitBtn->setProperty("secondary", true);

        alert.exec();

        if (alert.clickedButton() == restoreBtn) {
            const QString backupFile = QFileDialog::getOpenFileName(
                nullptr,
                QStringLiteral("Seleccionar copia de seguridad a restaurar"),
                BootstrapConfig::autoBackupDirPath(),
                QStringLiteral("Base de datos SQLite (*.sqlite)"));

            if (backupFile.isEmpty()) {
                return 1;
            }
            BootstrapConfig::hideDirectory(QFileInfo(bootCfg.sqlitePath).absolutePath());
            if (!QFile::copy(backupFile, bootCfg.sqlitePath)) {
                QMessageBox::critical(nullptr, QStringLiteral("Error restaurando"),
                                      QStringLiteral("No se pudo copiar el respaldo hacia:\n%1").arg(bootCfg.sqlitePath));
                return 1;
            }
            QFile::setPermissions(bootCfg.sqlitePath, QFile::ReadOwner | QFile::WriteOwner);
        } else if (alert.clickedButton() == reconfigBtn) {
            SetupWizard wizard;
            if (wizard.exec() != QDialog::Accepted) {
                return 0;
            }
            bootCfg = BootstrapConfig::load();
        } else {
            return 1;
        }
    }

    // 3. Abrir base de datos configurada (si SetupWizard ya la dejó abierta, no reabre innecesariamente)
    if (!Database::instance().isOpen()) {
        QString dbError;
        if (!Database::instance().initialize(&dbError)) {
            QMessageBox::critical(
                nullptr,
                QStringLiteral("Error al iniciar Global Manager"),
                QStringLiteral("No se pudo conectar a la base de datos:\n\n%1").arg(dbError));
            return 1;
        }
    }

    Database::instance().registerCurrentWorkstation(bootCfg.terminalNumber, bootCfg.terminalName);

    // 4. Instalar filtro global de inactividad
    Session &session = Session::instance();
    session.reloadInactivityFromDatabase();
    app.installEventFilter(&session);

    // 5. Respaldo automático rotativo silencioso al cerrar la app (en modo SQLite)
    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        if (Database::instance().isOpen() && Database::instance().isSqlite()) {
            Database::instance().createAutomaticBackup();
        }
    });

    // 6. Login obligatorio
    LoginDialog login;
    if (login.exec() != QDialog::Accepted) {
        return 0;
    }

    // 7. Ventana principal
    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
