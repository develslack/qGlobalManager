#ifndef MODULES_PANEL_CONTROL_SUPPORTTABLESWIDGET_H
#define MODULES_PANEL_CONTROL_SUPPORTTABLESWIDGET_H

#include <QWidget>

class QComboBox;
class QLineEdit;
class QPushButton;
class QTableWidget;

class SupportTablesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SupportTablesWidget(QWidget *parent = nullptr);
    void reloadAll();

private slots:
    // Roles
    void onSaveRole();
    void onResetRoleForm();
    void onEditRole(int roleId, const QString &name, const QString &description);
    void onConfigureRolePermissions(int roleId, const QString &roleName, bool isSystem);
    void onToggleRoleActive(int roleId, const QString &roleName, bool currentlyActive);

    // Catálogos generales (IVA, Documentos, Medios de pago)
    void onCatalogChanged(int index);
    void onSaveCatalogItem();
    void onResetCatalogForm();
    void onEditCatalogItem(int itemId, const QString &code, const QString &name);
    void onToggleCatalogItemActive(int itemId, bool currentlyActive);

private:
    void setupUi();
    void loadRoles();
    void loadCatalogItems();

    // Controles de Roles
    int m_editingRoleId = -1;
    QLineEdit *m_roleNameEdit = nullptr;
    QLineEdit *m_roleDescEdit = nullptr;
    QPushButton *m_roleSaveBtn = nullptr;
    QPushButton *m_roleCancelBtn = nullptr;
    QTableWidget *m_rolesTable = nullptr;

    // Controles de Catálogos soporte
    int m_editingCatalogId = -1;
    QComboBox *m_catalogSelector = nullptr;
    QLineEdit *m_catalogCodeEdit = nullptr;
    QLineEdit *m_catalogNameEdit = nullptr;
    QPushButton *m_catalogSaveBtn = nullptr;
    QPushButton *m_catalogCancelBtn = nullptr;
    QTableWidget *m_catalogTable = nullptr;
};

#endif // MODULES_PANEL_CONTROL_SUPPORTTABLESWIDGET_H
