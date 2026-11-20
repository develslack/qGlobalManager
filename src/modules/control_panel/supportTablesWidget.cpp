#include "supportTablesWidget.h"
#include "core/db.h"
#include "core/icons.h"
#include "core/session.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QVariant>

namespace {

enum CatalogKind {
    CatalogIva = 0,
    CatalogDocTypes = 1,
    CatalogPaymentMethods = 2
};

QString tableForCatalog(int idx)
{
    switch (idx) {
    case CatalogIva:            return QStringLiteral("iva_conditions");
    case CatalogDocTypes:       return QStringLiteral("document_types");
    case CatalogPaymentMethods: return QStringLiteral("payment_methods");
    default:                    return QStringLiteral("iva_conditions");
    }
}

} // namespace

SupportTablesWidget::SupportTablesWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    reloadAll();
}

void SupportTablesWidget::setupUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(14);

    // =========================================================================
    // Bloque 1: Administración de Roles y sus permisos predeterminados
    // =========================================================================
    auto *rolesGroup = new QGroupBox(QStringLiteral("Roles del Sistema"), this);
    auto *rolesLayout = new QVBoxLayout(rolesGroup);
    rolesLayout->setSpacing(10);

    auto *roleForm = new QFormLayout();
    roleForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_roleNameEdit = new QLineEdit(rolesGroup);
    m_roleNameEdit->setPlaceholderText(QStringLiteral("Ej: Cajero, Recepcionista, Odontólogo, Supervisor"));
    m_roleDescEdit = new QLineEdit(rolesGroup);
    m_roleDescEdit->setPlaceholderText(QStringLiteral("Descripción breve de las responsabilidades del rol"));

    roleForm->addRow(QStringLiteral("Nombre del rol:"), m_roleNameEdit);
    roleForm->addRow(QStringLiteral("Descripción:"), m_roleDescEdit);
    rolesLayout->addLayout(roleForm);

    auto *roleBtnRow = new QHBoxLayout();
    m_roleSaveBtn = new QPushButton(QStringLiteral("Agregar rol"), rolesGroup);
    m_roleSaveBtn->setIcon(Icons::get(Icons::Name::Add, QColor(QStringLiteral("#FFFFFF")), 16));

    m_roleCancelBtn = new QPushButton(QStringLiteral("Limpiar"), rolesGroup);
    m_roleCancelBtn->setIcon(Icons::get(Icons::Name::Refresh, QColor(QStringLiteral("#1E3A5F")), 16));
    m_roleCancelBtn->setProperty("secondary", true);

    roleBtnRow->addWidget(m_roleSaveBtn);
    roleBtnRow->addWidget(m_roleCancelBtn);
    roleBtnRow->addStretch();
    rolesLayout->addLayout(roleBtnRow);

    m_rolesTable = new QTableWidget(rolesGroup);
    m_rolesTable->setColumnCount(4);
    m_rolesTable->setHorizontalHeaderLabels({
        QStringLiteral("Rol"),
        QStringLiteral("Descripción"),
        QStringLiteral("Estado"),
        QStringLiteral("Acciones")
    });
    m_rolesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_rolesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_rolesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_rolesTable->setAlternatingRowColors(true);
    m_rolesTable->verticalHeader()->setVisible(false);
    m_rolesTable->verticalHeader()->setDefaultSectionSize(38);
    m_rolesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_rolesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_rolesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_rolesTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_rolesTable->setMinimumHeight(190);
    rolesLayout->addWidget(m_rolesTable);

    rootLayout->addWidget(rolesGroup);

    // =========================================================================
    // Bloque 2: Otras Tablas Soporte (IVA, Documentos, Medios de Pago)
    // =========================================================================
    auto *catalogGroup = new QGroupBox(QStringLiteral("Catálogos Soporte Generales"), this);
    auto *catalogLayout = new QVBoxLayout(catalogGroup);
    catalogLayout->setSpacing(10);

    auto *selectorForm = new QFormLayout();
    selectorForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_catalogSelector = new QComboBox(catalogGroup);
    m_catalogSelector->addItem(QStringLiteral("Condiciones frente al IVA"), CatalogIva);
    m_catalogSelector->addItem(QStringLiteral("Tipos de Documento"), CatalogDocTypes);
    m_catalogSelector->addItem(QStringLiteral("Medios de Pago"), CatalogPaymentMethods);

    m_catalogCodeEdit = new QLineEdit(catalogGroup);
    m_catalogCodeEdit->setPlaceholderText(QStringLiteral("Código corto (ej: DNI, CUIT, LC)"));

    m_catalogNameEdit = new QLineEdit(catalogGroup);
    m_catalogNameEdit->setPlaceholderText(QStringLiteral("Nombre / descripción del elemento"));

    selectorForm->addRow(QStringLiteral("Tabla soporte:"), m_catalogSelector);
    selectorForm->addRow(QStringLiteral("Código:"), m_catalogCodeEdit);
    selectorForm->addRow(QStringLiteral("Nombre:"), m_catalogNameEdit);
    catalogLayout->addLayout(selectorForm);

    auto *catBtnRow = new QHBoxLayout();
    m_catalogSaveBtn = new QPushButton(QStringLiteral("Agregar registro"), catalogGroup);
    m_catalogSaveBtn->setIcon(Icons::get(Icons::Name::Add, QColor(QStringLiteral("#FFFFFF")), 16));

    m_catalogCancelBtn = new QPushButton(QStringLiteral("Limpiar"), catalogGroup);
    m_catalogCancelBtn->setIcon(Icons::get(Icons::Name::Refresh, QColor(QStringLiteral("#1E3A5F")), 16));
    m_catalogCancelBtn->setProperty("secondary", true);

    catBtnRow->addWidget(m_catalogSaveBtn);
    catBtnRow->addWidget(m_catalogCancelBtn);
    catBtnRow->addStretch();
    catalogLayout->addLayout(catBtnRow);

    m_catalogTable = new QTableWidget(catalogGroup);
    m_catalogTable->setColumnCount(4);
    m_catalogTable->setHorizontalHeaderLabels({
        QStringLiteral("Código"),
        QStringLiteral("Nombre"),
        QStringLiteral("Estado"),
        QStringLiteral("Acciones")
    });
    m_catalogTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_catalogTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_catalogTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_catalogTable->setAlternatingRowColors(true);
    m_catalogTable->verticalHeader()->setVisible(false);
    m_catalogTable->verticalHeader()->setDefaultSectionSize(38);
    m_catalogTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_catalogTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_catalogTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_catalogTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_catalogTable->setMinimumHeight(180);
    catalogLayout->addWidget(m_catalogTable);

    rootLayout->addWidget(catalogGroup);
    rootLayout->addStretch();

    connect(m_roleSaveBtn, &QPushButton::clicked, this, &SupportTablesWidget::onSaveRole);
    connect(m_roleCancelBtn, &QPushButton::clicked, this, &SupportTablesWidget::onResetRoleForm);
    connect(m_catalogSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SupportTablesWidget::onCatalogChanged);
    connect(m_catalogSaveBtn, &QPushButton::clicked, this, &SupportTablesWidget::onSaveCatalogItem);
    connect(m_catalogCancelBtn, &QPushButton::clicked, this, &SupportTablesWidget::onResetCatalogForm);
}

void SupportTablesWidget::reloadAll()
{
    loadRoles();
    onCatalogChanged(m_catalogSelector->currentIndex());
}

void SupportTablesWidget::loadRoles()
{
    m_rolesTable->setRowCount(0);

    QSqlQuery query(Database::instance().connection());
    if (!query.exec(QStringLiteral(
            "SELECT id, name, description, is_system, is_active "
            "FROM roles ORDER BY is_system DESC, name ASC;"))) {
        return;
    }

    int row = 0;
    while (query.next()) {
        const int id = query.value(0).toInt();
        const QString name = query.value(1).toString();
        const QString desc = query.value(2).toString();
        const bool isSystem = (query.value(3).toInt() == 1);
        const bool isActive = (query.value(4).toInt() == 1);

        m_rolesTable->insertRow(row);
        m_rolesTable->setItem(row, 0, new QTableWidgetItem(isSystem ? name + QStringLiteral(" (Sistema)") : name));
        m_rolesTable->setItem(row, 1, new QTableWidgetItem(desc));

        auto *statusItem = new QTableWidgetItem(isActive ? QStringLiteral("Activo") : QStringLiteral("Inactivo"));
        statusItem->setForeground(QBrush(QColor(isActive ? QStringLiteral("#2E7D32") : QStringLiteral("#C53030"))));
        m_rolesTable->setItem(row, 2, statusItem);

        auto *actionsWidget = new QWidget(m_rolesTable);
        auto *actionsLayout = new QHBoxLayout(actionsWidget);
        actionsLayout->setContentsMargins(4, 2, 4, 2);
        actionsLayout->setSpacing(6);

        auto *editBtn = new QPushButton(QStringLiteral("Editar"), actionsWidget);
        editBtn->setIcon(Icons::get(Icons::Name::Edit, QColor(QStringLiteral("#1E3A5F")), 14));
        editBtn->setProperty("secondary", true);
        editBtn->setEnabled(!isSystem);

        auto *permsBtn = new QPushButton(QStringLiteral("Permisos"), actionsWidget);
        permsBtn->setIcon(Icons::get(Icons::Name::Shield, QColor(QStringLiteral("#FFFFFF")), 14));

        auto *toggleBtn = new QPushButton(
            isActive ? QStringLiteral("Desactivar") : QStringLiteral("Activar"), actionsWidget);
        toggleBtn->setIcon(Icons::get(
            isActive ? Icons::Name::Lock : Icons::Name::Unlock,
            QColor(isActive ? QStringLiteral("#FFFFFF") : QStringLiteral("#1E3A5F")), 14));
        if (isActive) {
            toggleBtn->setProperty("danger", true);
        } else {
            toggleBtn->setProperty("secondary", true);
        }
        toggleBtn->setEnabled(!isSystem);

        connect(editBtn, &QPushButton::clicked, this, [this, id, name, desc]() {
            onEditRole(id, name, desc);
        });
        connect(permsBtn, &QPushButton::clicked, this, [this, id, name, isSystem]() {
            onConfigureRolePermissions(id, name, isSystem);
        });
        connect(toggleBtn, &QPushButton::clicked, this, [this, id, name, isActive]() {
            onToggleRoleActive(id, name, isActive);
        });

        actionsLayout->addWidget(editBtn);
        actionsLayout->addWidget(permsBtn);
        actionsLayout->addWidget(toggleBtn);
        m_rolesTable->setCellWidget(row, 3, actionsWidget);
        ++row;
    }

    m_rolesTable->resizeColumnToContents(3);
}

void SupportTablesWidget::onSaveRole()
{
    const QString name = m_roleNameEdit->text().trimmed();
    const QString desc = m_roleDescEdit->text().trimmed();

    if (name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Dato requerido"),
                             QStringLiteral("Ingresa un nombre para el rol."));
        m_roleNameEdit->setFocus();
        return;
    }

    QSqlQuery query(Database::instance().connection());
    if (m_editingRoleId < 0) {
        query.prepare(QStringLiteral(
            "INSERT INTO roles (name, description, is_system, is_active) "
            "VALUES (:name, :desc, 0, 1);"));
    } else {
        query.prepare(QStringLiteral(
            "UPDATE roles SET name = :name, description = :desc "
            "WHERE id = :id AND is_system = 0;"));
        query.bindValue(QStringLiteral(":id"), m_editingRoleId);
    }

    query.bindValue(QStringLiteral(":name"), name);
    query.bindValue(QStringLiteral(":desc"), desc);

    if (!query.exec()) {
        QMessageBox::warning(this, QStringLiteral("Error guardando rol"),
                             QStringLiteral("No se pudo guardar el rol (verifica que el nombre no esté repetido):\n%1")
                                 .arg(query.lastError().text()));
        return;
    }

    Database::instance().logAudit(
        Session::instance().currentUser().email,
        QStringLiteral("panel_control"),
        m_editingRoleId < 0 ? QStringLiteral("role_create") : QStringLiteral("role_edit"),
        QStringLiteral("Rol: %1").arg(name));

    onResetRoleForm();
    loadRoles();
}

void SupportTablesWidget::onResetRoleForm()
{
    m_editingRoleId = -1;
    m_roleNameEdit->clear();
    m_roleDescEdit->clear();
    m_roleSaveBtn->setText(QStringLiteral("Agregar rol"));
    m_roleSaveBtn->setIcon(Icons::get(Icons::Name::Add, QColor(QStringLiteral("#FFFFFF")), 16));
}

void SupportTablesWidget::onEditRole(int roleId, const QString &name, const QString &description)
{
    m_editingRoleId = roleId;
    m_roleNameEdit->setText(name);
    m_roleDescEdit->setText(description);
    m_roleSaveBtn->setText(QStringLiteral("Guardar cambios"));
    m_roleSaveBtn->setIcon(Icons::get(Icons::Name::Save, QColor(QStringLiteral("#FFFFFF")), 16));
    m_roleNameEdit->setFocus();
}

void SupportTablesWidget::onConfigureRolePermissions(int roleId, const QString &roleName, bool isSystem)
{
    if (isSystem) {
        QMessageBox::information(
            this,
            QStringLiteral("Rol Administrador"),
            QStringLiteral("El rol \"%1\" posee acceso completo a todos los módulos por definición del sistema.")
                .arg(roleName));
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Permisos predeterminados - %1").arg(roleName));
    dlg.setMinimumSize(420, 360);

    auto *dlgLayout = new QVBoxLayout(&dlg);
    auto *infoLbl = new QLabel(
        QStringLiteral("Marca los permisos base que heredarán los usuarios con el rol <b>%1</b>:")
            .arg(roleName.toHtmlEscaped()),
        &dlg);
    infoLbl->setWordWrap(true);
    dlgLayout->addWidget(infoLbl);

    auto *scroll = new QScrollArea(&dlg);
    scroll->setWidgetResizable(true);
    auto *content = new QWidget(scroll);
    auto *contentLayout = new QVBoxLayout(content);

    // Cargar permisos actuales del rol
    QSet<int> assignedIds;
    {
        QSqlQuery qAssigned(Database::instance().connection());
        qAssigned.prepare(QStringLiteral("SELECT permission_id FROM role_permissions WHERE role_id = :rid;"));
        qAssigned.bindValue(QStringLiteral(":rid"), roleId);
        if (qAssigned.exec()) {
            while (qAssigned.next()) {
                assignedIds.insert(qAssigned.value(0).toInt());
            }
        }
    }

    // Agrupar permisos maestros por módulo
    QMap<QString, QList<QCheckBox *>> checkboxesByModule;
    QSqlQuery qAll(Database::instance().connection());
    if (qAll.exec(QStringLiteral("SELECT id, module_id, action, description FROM permissions ORDER BY module_id, id;"))) {
        while (qAll.next()) {
            const int permId = qAll.value(0).toInt();
            const QString modId = qAll.value(1).toString();
            const QString act = qAll.value(2).toString();
            const QString desc = qAll.value(3).toString();

            auto *cb = new QCheckBox(
                QStringLiteral("%1 (%2)").arg(desc.isEmpty() ? act : desc, act), content);
            cb->setProperty("permission_id", permId);
            cb->setChecked(assignedIds.contains(permId));
            checkboxesByModule[modId].append(cb);
        }
    }

    for (auto it = checkboxesByModule.begin(); it != checkboxesByModule.end(); ++it) {
        auto *modGroup = new QGroupBox(QStringLiteral("Módulo: %1").arg(it.key()), content);
        auto *modLayout = new QVBoxLayout(modGroup);
        for (QCheckBox *cb : it.value()) {
            modLayout->addWidget(cb);
        }
        contentLayout->addWidget(modGroup);
    }
    contentLayout->addStretch();
    scroll->setWidget(content);
    dlgLayout->addWidget(scroll, 1);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto *cancelBtn = new QPushButton(QStringLiteral("Cancelar"), &dlg);
    cancelBtn->setIcon(Icons::get(Icons::Name::Close, QColor(QStringLiteral("#1E3A5F")), 15));
    cancelBtn->setProperty("secondary", true);

    auto *saveBtn = new QPushButton(QStringLiteral("Guardar permisos"), &dlg);
    saveBtn->setIcon(Icons::get(Icons::Name::Save, QColor(QStringLiteral("#FFFFFF")), 15));

    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(saveBtn);
    dlgLayout->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    QSqlDatabase db = Database::instance().connection();
    db.transaction();

    QSqlQuery delQ(db);
    delQ.prepare(QStringLiteral("DELETE FROM role_permissions WHERE role_id = :rid;"));
    delQ.bindValue(QStringLiteral(":rid"), roleId);
    bool ok = delQ.exec();

    if (ok) {
        QSqlQuery insQ(db);
        insQ.prepare(QStringLiteral(
            "INSERT INTO role_permissions (role_id, permission_id) VALUES (:rid, :pid);"));
        for (auto it = checkboxesByModule.begin(); it != checkboxesByModule.end() && ok; ++it) {
            for (QCheckBox *cb : it.value()) {
                if (cb->isChecked()) {
                    insQ.bindValue(QStringLiteral(":rid"), roleId);
                    insQ.bindValue(QStringLiteral(":pid"), cb->property("permission_id").toInt());
                    if (!insQ.exec()) {
                        ok = false;
                        break;
                    }
                }
            }
        }
    }

    if (ok) {
        db.commit();
        Database::instance().logAudit(
            Session::instance().currentUser().email,
            QStringLiteral("panel_control"),
            QStringLiteral("role_permissions_update"),
            QStringLiteral("Permisos actualizados para rol: %1").arg(roleName));
        QMessageBox::information(this, QStringLiteral("Permisos guardados"),
                                 QStringLiteral("Se actualizaron los permisos del rol \"%1\".").arg(roleName));
    } else {
        db.rollback();
        QMessageBox::warning(this, QStringLiteral("Error"),
                             QStringLiteral("No se pudieron guardar los permisos del rol."));
    }
}

void SupportTablesWidget::onToggleRoleActive(int roleId, const QString &roleName, bool currentlyActive)
{
    QSqlQuery query(Database::instance().connection());
    query.prepare(QStringLiteral("UPDATE roles SET is_active = :active WHERE id = :id AND is_system = 0;"));
    query.bindValue(QStringLiteral(":active"), currentlyActive ? 0 : 1);
    query.bindValue(QStringLiteral(":id"), roleId);

    if (query.exec()) {
        Database::instance().logAudit(
            Session::instance().currentUser().email,
            QStringLiteral("panel_control"),
            QStringLiteral("role_toggle"),
            QStringLiteral("Rol %1 -> %2").arg(roleName, currentlyActive ? QStringLiteral("Inactivo") : QStringLiteral("Activo")));
        loadRoles();
    }
}

void SupportTablesWidget::onCatalogChanged(int index)
{
    const bool hasCode = (index == CatalogDocTypes);
    m_catalogCodeEdit->setEnabled(hasCode);
    m_catalogCodeEdit->setVisible(hasCode);
    m_catalogTable->setColumnHidden(0, !hasCode);
    onResetCatalogForm();
    loadCatalogItems();
}

void SupportTablesWidget::loadCatalogItems()
{
    m_catalogTable->setRowCount(0);
    const int idx = m_catalogSelector->currentIndex();
    const bool hasCode = (idx == CatalogDocTypes);
    const QString table = tableForCatalog(idx);

    QSqlQuery query(Database::instance().connection());
    const QString sql = hasCode
                            ? QStringLiteral("SELECT id, code, name, is_active FROM document_types ORDER BY code;")
                            : QStringLiteral("SELECT id, '', name, is_active FROM %1 ORDER BY name;").arg(table);

    if (!query.exec(sql)) {
        return;
    }

    int row = 0;
    while (query.next()) {
        const int id = query.value(0).toInt();
        const QString code = query.value(1).toString();
        const QString name = query.value(2).toString();
        const bool isActive = (query.value(3).toInt() == 1);

        m_catalogTable->insertRow(row);
        m_catalogTable->setItem(row, 0, new QTableWidgetItem(code));
        m_catalogTable->setItem(row, 1, new QTableWidgetItem(name));

        auto *statusItem = new QTableWidgetItem(isActive ? QStringLiteral("Activo") : QStringLiteral("Inactivo"));
        statusItem->setForeground(QBrush(QColor(isActive ? QStringLiteral("#2E7D32") : QStringLiteral("#C53030"))));
        m_catalogTable->setItem(row, 2, statusItem);

        auto *actionsWidget = new QWidget(m_catalogTable);
        auto *actionsLayout = new QHBoxLayout(actionsWidget);
        actionsLayout->setContentsMargins(4, 2, 4, 2);
        actionsLayout->setSpacing(6);

        auto *editBtn = new QPushButton(QStringLiteral("Editar"), actionsWidget);
        editBtn->setIcon(Icons::get(Icons::Name::Edit, QColor(QStringLiteral("#1E3A5F")), 14));
        editBtn->setProperty("secondary", true);

        auto *toggleBtn = new QPushButton(
            isActive ? QStringLiteral("Desactivar") : QStringLiteral("Activar"), actionsWidget);
        toggleBtn->setIcon(Icons::get(
            isActive ? Icons::Name::Lock : Icons::Name::Unlock,
            QColor(isActive ? QStringLiteral("#FFFFFF") : QStringLiteral("#1E3A5F")), 14));
        if (isActive) {
            toggleBtn->setProperty("danger", true);
        } else {
            toggleBtn->setProperty("secondary", true);
        }

        connect(editBtn, &QPushButton::clicked, this, [this, id, code, name]() {
            onEditCatalogItem(id, code, name);
        });
        connect(toggleBtn, &QPushButton::clicked, this, [this, id, isActive]() {
            onToggleCatalogItemActive(id, isActive);
        });

        actionsLayout->addWidget(editBtn);
        actionsLayout->addWidget(toggleBtn);
        m_catalogTable->setCellWidget(row, 3, actionsWidget);
        ++row;
    }

    m_catalogTable->resizeColumnToContents(3);
}

void SupportTablesWidget::onSaveCatalogItem()
{
    const int idx = m_catalogSelector->currentIndex();
    const bool hasCode = (idx == CatalogDocTypes);
    const QString table = tableForCatalog(idx);
    const QString code = m_catalogCodeEdit->text().trimmed().toUpper();
    const QString name = m_catalogNameEdit->text().trimmed();

    if (hasCode && code.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Dato requerido"),
                             QStringLiteral("Ingresa el código corto del tipo de documento."));
        return;
    }
    if (name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Dato requerido"),
                             QStringLiteral("Ingresa el nombre del registro."));
        return;
    }

    QSqlQuery query(Database::instance().connection());
    if (hasCode) {
        if (m_editingCatalogId < 0) {
            query.prepare(QStringLiteral("INSERT INTO document_types (code, name, is_active) VALUES (:code, :name, 1);"));
        } else {
            query.prepare(QStringLiteral("UPDATE document_types SET code = :code, name = :name WHERE id = :id;"));
            query.bindValue(QStringLiteral(":id"), m_editingCatalogId);
        }
        query.bindValue(QStringLiteral(":code"), code);
        query.bindValue(QStringLiteral(":name"), name);
    } else {
        if (m_editingCatalogId < 0) {
            query.prepare(QStringLiteral("INSERT INTO %1 (name, is_active) VALUES (:name, 1);").arg(table));
        } else {
            query.prepare(QStringLiteral("UPDATE %1 SET name = :name WHERE id = :id;").arg(table));
            query.bindValue(QStringLiteral(":id"), m_editingCatalogId);
        }
        query.bindValue(QStringLiteral(":name"), name);
    }

    if (!query.exec()) {
        QMessageBox::warning(this, QStringLiteral("Error guardando registro"),
                             QStringLiteral("No se pudo guardar (verifica que no exista duplicado):\n%1")
                                 .arg(query.lastError().text()));
        return;
    }

    onResetCatalogForm();
    loadCatalogItems();
}

void SupportTablesWidget::onResetCatalogForm()
{
    m_editingCatalogId = -1;
    m_catalogCodeEdit->clear();
    m_catalogNameEdit->clear();
    m_catalogSaveBtn->setText(QStringLiteral("Agregar registro"));
    m_catalogSaveBtn->setIcon(Icons::get(Icons::Name::Add, QColor(QStringLiteral("#FFFFFF")), 16));
}

void SupportTablesWidget::onEditCatalogItem(int itemId, const QString &code, const QString &name)
{
    m_editingCatalogId = itemId;
    m_catalogCodeEdit->setText(code);
    m_catalogNameEdit->setText(name);
    m_catalogSaveBtn->setText(QStringLiteral("Guardar cambios"));
    m_catalogSaveBtn->setIcon(Icons::get(Icons::Name::Save, QColor(QStringLiteral("#FFFFFF")), 16));
    m_catalogNameEdit->setFocus();
}

void SupportTablesWidget::onToggleCatalogItemActive(int itemId, bool currentlyActive)
{
    const QString table = tableForCatalog(m_catalogSelector->currentIndex());
    QSqlQuery query(Database::instance().connection());
    query.prepare(QStringLiteral("UPDATE %1 SET is_active = :active WHERE id = :id;").arg(table));
    query.bindValue(QStringLiteral(":active"), currentlyActive ? 0 : 1);
    query.bindValue(QStringLiteral(":id"), itemId);
    if (query.exec()) {
        loadCatalogItems();
    }
}
