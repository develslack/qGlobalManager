#ifndef CORE_ICONS_H
#define CORE_ICONS_H

#include <QColor>
#include <QIcon>

namespace Icons {

enum class Name {
    Menu,
    Home,
    ControlPanel,
    Logout,
    Login,
    Close,
    Users,
    Role,
    Shield,
    Key,
    Lock,
    Unlock,
    Company,
    Mail,
    Send,
    Printer,
    Database,
    Backup,
    Table,
    Add,
    Edit,
    Save,
    Delete,
    Search,
    Refresh,
    Check,
    Play
};

QIcon get(Name iconName, const QColor &color = QColor(QStringLiteral("#FFFFFF")), int sizePx = 18);

} // namespace Icons

#endif // CORE_ICONS_H
