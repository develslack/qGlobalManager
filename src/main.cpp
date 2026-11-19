#include "core/db.h"
#include "core/session.h"
#include "ui/loginDialog.h"
#include "ui/mainWindow.h"
#include "core/theme.h"

#include <QApplication>
#include <QMessageBox>
#include <QIcon>


int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication app(argc, argv);
    Theme::apply(app);
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/qGlobalManager.svg")));
    app.setApplicationName(QStringLiteral("qGlobalManager"));
    app.setApplicationDisplayName(QStringLiteral("Global Manager"));
    app.setOrganizationName(QStringLiteral("GlobalManager"));
    app.setApplicationVersion(QStringLiteral(APP_VERSION));

    // 1. Inicializar base de datos SQLite
    QString dbError;
    if (!Database::instance().initialize(&dbError)) {
        QMessageBox::critical(
            nullptr,
            QStringLiteral("Error al iniciar Global Manager"),
            QStringLiteral("No se pudo inicializar la base de datos local:\n\n%1").arg(dbError));
        return 1;
    }

    // 2. Instalar filtro global de inactividad (15 min inactivo, 30 seg de aviso)
    Session &session = Session::instance();
    session.reloadInactivityFromDatabase();
    app.installEventFilter(&session);

    // 3. Solicitar inicio de sesión obligatorio antes de abrir la ventana principal
    LoginDialog login;
    if (login.exec() != QDialog::Accepted) {
        return 0;
    }

    // 4. Lanzar ventana principal enrutadora
    MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
