#ifndef MODULES_USERS_USERS_H
#define MODULES_USERS_USERS_H

#include "core/moduleBase.h"

class QComboBox;
class QLineEdit;
class QPushButton;
class QTableWidget;

class UsuariosModule : public ModuleBase
{
    Q_OBJECT

public:
    explicit UsuariosModule(QWidget *parent = nullptr);

    QString moduleId() const override { return QStringLiteral("usuarios"); }
    QString moduleTitle() const override { return QStringLiteral("Administración de Usuarios"); }
    void onActivated() override;

private slots:
    void onSaveUser();
    void onResetForm();
    void onSearchTextChanged(const QString &text);
    void onEditUser(int userId, const QString &fullName, const QString &email, int roleId);
    void onEditUserPermissions(int userId, const QString &fullName, int roleId, bool isSystemRole);
    void onResendNewPassword(int userId, const QString &fullName, const QString &email, bool isSystemRole);
    void onToggleBlockUser(int userId, const QString &fullName, const QString &email, bool currentlyActive, bool isSystemRole);

private:
    void setupUi();
    void loadRolesCombo();
    void loadUsersTable(const QString &filter = QString());

    int m_editingUserId = -1;

    QLineEdit *m_fullNameEdit = nullptr;
    QLineEdit *m_emailEdit = nullptr;
    QComboBox *m_roleCombo = nullptr;
    QPushButton *m_saveBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;

    QLineEdit *m_searchEdit = nullptr;
    QTableWidget *m_usersTable = nullptr;
};

#endif // MODULES_USUARIOS_USUARIOS_H
