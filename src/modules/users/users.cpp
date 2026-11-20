#include "users.h"
#include "core/db.h"
#include "core/icons.h"
#include "core/session.h"
#include "core/userCredentials.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
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

UsuariosModule::UsuariosModule(QWidget *parent)
    : ModuleBase(parent)
{
    initBanner(
        QStringLiteral("Administración de Usuarios"),
        QStringLiteral("Alta, edición, asignación de roles, permisos personalizados, blanqueo de claves y bloqueo de cuentas."));

    setupUi();
}

void UsuariosModule::setupUi()
{
    // =========================================================================
    // 1. Formulario de Alta / Edición de Usuario
    // =========================================================================
    auto *formGroup = new QGroupBox(QStringLiteral("Datos del Usuario"), this);
    auto *formGroupLayout = new QVBoxLayout(formGroup);
    formGroupLayout->setSpacing(10);

    auto *form = new QFormLayout();
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_fullNameEdit = new QLineEdit(formGroup);
    m_fullNameEdit->setPlaceholderText(QStringLiteral("Ej: María González"));

    m_emailEdit = new QLineEdit(formGroup);
    m_emailEdit->setPlaceholderText(QStringLiteral("maria.gonzalez@empresa.com (será su usuario de acceso)"));

    m_roleCombo = new QComboBox(formGroup);

    form->addRow(QStringLiteral("Nombre y Apellido *:"), m_fullNameEdit);
    form->addRow(QStringLiteral("Email (Usuario) *:"), m_emailEdit);
    form->addRow(QStringLiteral("Rol asignado *:"), m_roleCombo);
    formGroupLayout->addLayout(form);

    auto *hintLabel = new QLabel(
        QStringLiteral("En el alta, el sistema generará una contraseña aleatoria automáticamente, creará un archivo .txt "
                       "y lo enviará al correo indicado (eliminando el archivo local tras un envío exitoso)."),
        formGroup);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet(QStringLiteral("color: #52606D; font-size: 9pt;"));
    formGroupLayout->addWidget(hintLabel);

    auto *btnRow = new QHBoxLayout();
    m_saveBtn = new QPushButton(QStringLiteral("Dar de alta usuario"), formGroup);
    m_saveBtn->setIcon(Icons::get(Icons::Name::Add, QColor(QStringLiteral("#FFFFFF")), 16));

    m_cancelBtn = new QPushButton(QStringLiteral("Limpiar"), formGroup);
    m_cancelBtn->setIcon(Icons::get(Icons::Name::Refresh, QColor(QStringLiteral("#1E3A5F")), 16));
    m_cancelBtn->setProperty("secondary", true);

    btnRow->addWidget(m_saveBtn);
    btnRow->addWidget(m_cancelBtn);
    btnRow->addStretch();
    formGroupLayout->addLayout(btnRow);

    bodyLayout()->addWidget(formGroup);

    // =========================================================================
    // 2. Listado de Usuarios con buscador y botones por registro
    // =========================================================================
    auto *listGroup = new QGroupBox(QStringLiteral("Usuarios Registrados"), this);
    auto *listLayout = new QVBoxLayout(listGroup);
    listLayout->setSpacing(10);

    auto *searchRow = new QHBoxLayout();
    m_searchEdit = new QLineEdit(listGroup);
    m_searchEdit->setPlaceholderText(QStringLiteral("Buscar por nombre, email o rol..."));
    m_searchEdit->setClearButtonEnabled(true);

    auto *refreshBtn = new QPushButton(QStringLiteral("Actualizar"), listGroup);
    refreshBtn->setIcon(Icons::get(Icons::Name::Refresh, QColor(QStringLiteral("#1E3A5F")), 15));
    refreshBtn->setProperty("secondary", true);

    searchRow->addWidget(m_searchEdit, 1);
    searchRow->addWidget(refreshBtn);
    listLayout->addLayout(searchRow);

    m_usersTable = new QTableWidget(listGroup);
    m_usersTable->setColumnCount(5);
    m_usersTable->setHorizontalHeaderLabels({
        QStringLiteral("Nombre y Apellido"),
        QStringLiteral("Email (Usuario)"),
        QStringLiteral("Rol"),
        QStringLiteral("Estado"),
        QStringLiteral("Acciones")
    });
    m_usersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_usersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_usersTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_usersTable->setAlternatingRowColors(true);
    m_usersTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_usersTable->verticalHeader()->setVisible(false);
    m_usersTable->verticalHeader()->setDefaultSectionSize(40);
    m_usersTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_usersTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_usersTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_usersTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_usersTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_usersTable->setMinimumHeight(260);
    listLayout->addWidget(m_usersTable, 1);

    bodyLayout()->addWidget(listGroup, 1);

    connect(m_saveBtn, &QPushButton::clicked, this, &UsuariosModule::onSaveUser);
    connect(m_cancelBtn, &QPushButton::clicked, this, &UsuariosModule::onResetForm);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &UsuariosModule::onSearchTextChanged);
    connect(refreshBtn, &QPushButton::clicked, this, [this]() {
        loadRolesCombo();
        loadUsersTable(m_searchEdit->text());
    });
}

void UsuariosModule::onActivated()
{
    loadRolesCombo();
    loadUsersTable(m_searchEdit->text());
}

void UsuariosModule::loadRolesCombo()
{
    const int previousRoleId = m_roleCombo->currentData().toInt();
    m_roleCombo->clear();

    QSqlQuery query(Database::instance().connection());
    if (query.exec(QStringLiteral(
            "SELECT id, name FROM roles WHERE is_active = 1 ORDER BY is_system DESC, name ASC;"))) {
        while (query.next()) {
            m_roleCombo->addItem(query.value(1).toString(), query.value(0).toInt());
        }
    }

    const int idx = m_roleCombo->findData(previousRoleId);
    if (idx >= 0) {
        m_roleCombo->setCurrentIndex(idx);
    }
}

void UsuariosModule::loadUsersTable(const QString &filter)
{
    m_usersTable->setRowCount(0);

    QSqlQuery query(Database::instance().connection());
    const QString cleanFilter = filter.trimmed();
    if (cleanFilter.isEmpty()) {
        query.prepare(QStringLiteral(
            "SELECT u.id, u.full_name, u.email, u.role_id, r.name, r.is_system, "
            "       u.is_active, u.must_change_password "
            "FROM users u "
            "INNER JOIN roles r ON r.id = u.role_id "
            "ORDER BY r.is_system DESC, u.full_name ASC;"));
    } else {
        query.prepare(QStringLiteral(
            "SELECT u.id, u.full_name, u.email, u.role_id, r.name, r.is_system, "
            "       u.is_active, u.must_change_password "
            "FROM users u "
            "INNER JOIN roles r ON r.id = u.role_id "
            "WHERE u.full_name LIKE :f OR u.email LIKE :f OR r.name LIKE :f "
            "ORDER BY r.is_system DESC, u.full_name ASC;"));
        query.bindValue(QStringLiteral(":f"), QStringLiteral("%%1%").arg(cleanFilter));
    }

    if (!query.exec()) {
        return;
    }

    int row = 0;
    while (query.next()) {
        const int userId = query.value(0).toInt();
        const QString fullName = query.value(1).toString();
        const QString email = query.value(2).toString();
        const int roleId = query.value(3).toInt();
        const QString roleName = query.value(4).toString();
        const bool isSystemRole = (query.value(5).toInt() == 1);
        const bool isActive = (query.value(6).toInt() == 1);
        const bool mustChangePass = (query.value(7).toInt() == 1);

        m_usersTable->insertRow(row);
        m_usersTable->setItem(row, 0, new QTableWidgetItem(fullName));
        m_usersTable->setItem(row, 1, new QTableWidgetItem(email));
        m_usersTable->setItem(row, 2, new QTableWidgetItem(roleName));

        QString statusText = isActive ? QStringLiteral("Activo") : QStringLiteral("Bloqueado");
        if (isActive && mustChangePass) {
            statusText += QStringLiteral(" (Clave temp.)");
        }
        auto *statusItem = new QTableWidgetItem(statusText);
        statusItem->setForeground(QBrush(QColor(
            isActive ? (mustChangePass ? QStringLiteral("#B7791F") : QStringLiteral("#2E7D32"))
                     : QStringLiteral("#C53030"))));
        m_usersTable->setItem(row, 3, statusItem);

        // Botones de acción por registro
        auto *actionsWidget = new QWidget(m_usersTable);
        auto *actionsLayout = new QHBoxLayout(actionsWidget);
        actionsLayout->setContentsMargins(4, 3, 4, 3);
        actionsLayout->setSpacing(6);

        auto *editBtn = new QPushButton(QStringLiteral("Editar"), actionsWidget);
        editBtn->setIcon(Icons::get(Icons::Name::Edit, QColor(QStringLiteral("#1E3A5F")), 14));
        editBtn->setProperty("secondary", true);

        auto *permsBtn = new QPushButton(QStringLiteral("Permisos"), actionsWidget);
        permsBtn->setIcon(Icons::get(Icons::Name::Shield, QColor(QStringLiteral("#FFFFFF")), 14));

        auto *resendBtn = new QPushButton(QStringLiteral("Reenviar Nuevo Password"), actionsWidget);
        resendBtn->setIcon(Icons::get(Icons::Name::Mail, QColor(QStringLiteral("#1E3A5F")), 14));
        resendBtn->setProperty("secondary", true);
        resendBtn->setToolTip(QStringLiteral("Genera una nueva contraseña aleatoria y envía un nuevo archivo .txt al email del usuario"));

        auto *blockBtn = new QPushButton(
            isActive ? QStringLiteral("Bloquear") : QStringLiteral("Desbloquear"), actionsWidget);
        blockBtn->setIcon(Icons::get(
            isActive ? Icons::Name::Lock : Icons::Name::Unlock,
            QColor(isActive ? QStringLiteral("#FFFFFF") : QStringLiteral("#1E3A5F")), 14));
        if (isActive) {
            blockBtn->setProperty("danger", true);
        } else {
            blockBtn->setProperty("secondary", true);
        }

        connect(editBtn, &QPushButton::clicked, this, [this, userId, fullName, email, roleId]() {
            onEditUser(userId, fullName, email, roleId);
        });
        connect(permsBtn, &QPushButton::clicked, this, [this, userId, fullName, roleId, isSystemRole]() {
            onEditUserPermissions(userId, fullName, roleId, isSystemRole);
        });
        connect(resendBtn, &QPushButton::clicked, this, [this, userId, fullName, email, isSystemRole]() {
            onResendNewPassword(userId, fullName, email, isSystemRole);
        });
        connect(blockBtn, &QPushButton::clicked, this, [this, userId, fullName, email, isActive, isSystemRole]() {
            onToggleBlockUser(userId, fullName, email, isActive, isSystemRole);
        });

        actionsLayout->addWidget(editBtn);
        actionsLayout->addWidget(permsBtn);
        actionsLayout->addWidget(resendBtn);
        actionsLayout->addWidget(blockBtn);
        m_usersTable->setCellWidget(row, 4, actionsWidget);

        ++row;
    }

    m_usersTable->resizeColumnToContents(4);
}

void UsuariosModule::onSaveUser()
{
    const bool isCreate = (m_editingUserId < 0);
    if (isCreate) {
        if (!requirePermission(QStringLiteral("create"), QStringLiteral("dar de alta nuevos usuarios"))) {
            return;
        }
    } else {
        if (!requirePermission(QStringLiteral("edit"), QStringLiteral("editar datos de usuarios"))) {
            return;
        }
    }

    const QString fullName = m_fullNameEdit->text().trimmed();
    const QString email = m_emailEdit->text().trimmed().toLower();
    const int roleId = m_roleCombo->currentData().toInt();

    if (fullName.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Dato requerido"),
                             QStringLiteral("Ingresa el Nombre y Apellido del usuario."));
        m_fullNameEdit->setFocus();
        return;
    }

    if (!UserCredentials::isValidEmail(email)) {
        QMessageBox::warning(this, QStringLiteral("Email inválido"),
                             QStringLiteral("Ingresa un correo electrónico válido (será el usuario de acceso)."));
        m_emailEdit->setFocus();
        return;
    }

    if (roleId <= 0) {
        QMessageBox::warning(this, QStringLiteral("Rol requerido"),
                             QStringLiteral("Selecciona un rol activo para el usuario. Puedes administrar roles en Panel de Control → Tablas Soporte."));
        return;
    }

    QSqlQuery query(Database::instance().connection());

    if (isCreate) {
        const QString tempPass = Database::generateRandomPassword(12);

        query.prepare(QStringLiteral(
            "INSERT INTO users (email, full_name, role_id, password_hash, is_active, must_change_password) "
            "VALUES (:email, :full_name, :role_id, :password_hash, 1, 1);"));
        query.bindValue(QStringLiteral(":email"), email);
        query.bindValue(QStringLiteral(":full_name"), fullName);
        query.bindValue(QStringLiteral(":role_id"), roleId);
        query.bindValue(QStringLiteral(":password_hash"), Database::hashPassword(tempPass));

        if (!query.exec()) {
            QMessageBox::warning(
                this,
                QStringLiteral("Error al crear usuario"),
                QStringLiteral("No se pudo dar de alta el usuario (verifica que el email no esté registrado):\n%1")
                    .arg(query.lastError().text()));
            return;
        }

        Database::instance().logAudit(
            Session::instance().currentUser().email,
            moduleId(),
            QStringLiteral("user_create"),
            QStringLiteral("Alta de usuario: %1 (%2)").arg(fullName, email));

        QApplication::setOverrideCursor(Qt::WaitCursor);
        const CredentialDeliveryResult delivery = UserCredentials::deliverTemporaryPassword(
            fullName,
            email,
            tempPass,
            QStringLiteral("Global Manager - Bienvenido, tus credenciales de acceso"),
            QStringLiteral("Se ha creado tu cuenta de usuario en Global Manager."),
            true);
        QApplication::restoreOverrideCursor();

        if (delivery.emailSent) {
            QMessageBox::information(
                this,
                QStringLiteral("Usuario creado y notificado"),
                QStringLiteral("El usuario \"%1\" (%2) fue dado de alta correctamente.\n\n"
                               "Se envió automáticamente el archivo .txt con su contraseña temporal por email "
                               "y se eliminó la copia local por seguridad.")
                    .arg(fullName, email));
        } else {
            QMessageBox::warning(
                this,
                QStringLiteral("Usuario creado - Envío de email pendiente"),
                QStringLiteral("El usuario \"%1\" (%2) fue creado correctamente, pero no se pudo enviar el correo automático.\n\n"
                               "Motivo: %3\n\n"
                               "El archivo de credenciales se conservó temporalmente en:\n%4\n\n"
                               "Cuando configures el SMTP en Panel de Control, puedes usar \"Reenviar Nuevo Password\".")
                    .arg(fullName, email, delivery.errorMessage, delivery.txtPath));
        }
    } else {
        query.prepare(QStringLiteral(
            "UPDATE users SET full_name = :full_name, email = :email, role_id = :role_id "
            "WHERE id = :id;"));
        query.bindValue(QStringLiteral(":full_name"), fullName);
        query.bindValue(QStringLiteral(":email"), email);
        query.bindValue(QStringLiteral(":role_id"), roleId);
        query.bindValue(QStringLiteral(":id"), m_editingUserId);

        if (!query.exec()) {
            QMessageBox::warning(
                this,
                QStringLiteral("Error al actualizar usuario"),
                QStringLiteral("No se pudieron guardar los cambios:\n%1").arg(query.lastError().text()));
            return;
        }

        Database::instance().logAudit(
            Session::instance().currentUser().email,
            moduleId(),
            QStringLiteral("user_edit"),
            QStringLiteral("Edición de usuario ID %1: %2 (%3)").arg(m_editingUserId).arg(fullName, email));

        QMessageBox::information(
            this,
            QStringLiteral("Usuario actualizado"),
            QStringLiteral("Los datos de \"%1\" se guardaron correctamente.").arg(fullName));
    }

    onResetForm();
    loadUsersTable(m_searchEdit->text());
}

void UsuariosModule::onResetForm()
{
    m_editingUserId = -1;
    m_fullNameEdit->clear();
    m_emailEdit->clear();
    if (m_roleCombo->count() > 0) {
        m_roleCombo->setCurrentIndex(0);
    }
    m_saveBtn->setText(QStringLiteral("Dar de alta usuario"));
    m_saveBtn->setIcon(Icons::get(Icons::Name::Add, QColor(QStringLiteral("#FFFFFF")), 16));
}

void UsuariosModule::onSearchTextChanged(const QString &text)
{
    loadUsersTable(text);
}

void UsuariosModule::onEditUser(int userId, const QString &fullName, const QString &email, int roleId)
{
    if (!requirePermission(QStringLiteral("edit"), QStringLiteral("editar datos de usuarios"))) {
        return;
    }

    m_editingUserId = userId;
    m_fullNameEdit->setText(fullName);
    m_emailEdit->setText(email);

    const int roleIdx = m_roleCombo->findData(roleId);
    if (roleIdx >= 0) {
        m_roleCombo->setCurrentIndex(roleIdx);
    }

    m_saveBtn->setText(QStringLiteral("Guardar cambios"));
    m_saveBtn->setIcon(Icons::get(Icons::Name::Save, QColor(QStringLiteral("#FFFFFF")), 16));
    m_fullNameEdit->setFocus();
}

void UsuariosModule::onEditUserPermissions(int userId, const QString &fullName, int roleId, bool isSystemRole)
{
    if (!requirePermission(QStringLiteral("permissions"), QStringLiteral("modificar permisos de usuarios"))) {
        return;
    }

    if (isSystemRole) {
        QMessageBox::information(
            this,
            QStringLiteral("Usuario Administrador"),
            QStringLiteral("\"%1\" posee el rol Administrador del sistema y cuenta con acceso completo a todos los módulos.")
                .arg(fullName));
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Permisos de usuario - %1").arg(fullName));
    dlg.setMinimumSize(440, 380);

    auto *dlgLayout = new QVBoxLayout(&dlg);
    auto *infoLbl = new QLabel(
        QStringLiteral("Configura los permisos efectivos para <b>%1</b>. "
                       "Los permisos marcados inicialmente provienen de su rol y puedes ajustarlos individualmente:")
            .arg(fullName.toHtmlEscaped()),
        &dlg);
    infoLbl->setWordWrap(true);
    dlgLayout->addWidget(infoLbl);

    // 1. Permisos heredados del rol
    QSet<int> rolePermIds;
    {
        QSqlQuery qRole(Database::instance().connection());
        qRole.prepare(QStringLiteral("SELECT permission_id FROM role_permissions WHERE role_id = :rid;"));
        qRole.bindValue(QStringLiteral(":rid"), roleId);
        if (qRole.exec()) {
            while (qRole.next()) {
                rolePermIds.insert(qRole.value(0).toInt());
            }
        }
    }

    // 2. Excepciones actuales del usuario
    QMap<int, bool> userOverrides;
    {
        QSqlQuery qUser(Database::instance().connection());
        qUser.prepare(QStringLiteral("SELECT permission_id, granted FROM user_permissions WHERE user_id = :uid;"));
        qUser.bindValue(QStringLiteral(":uid"), userId);
        if (qUser.exec()) {
            while (qUser.next()) {
                userOverrides.insert(qUser.value(0).toInt(), qUser.value(1).toInt() == 1);
            }
        }
    }

    auto *scroll = new QScrollArea(&dlg);
    scroll->setWidgetResizable(true);
    auto *content = new QWidget(scroll);
    auto *contentLayout = new QVBoxLayout(content);

    QMap<QString, QList<QCheckBox *>> checkboxesByModule;
    QSqlQuery qAll(Database::instance().connection());
    if (qAll.exec(QStringLiteral("SELECT id, module_id, action, description FROM permissions ORDER BY module_id, id;"))) {
        while (qAll.next()) {
            const int permId = qAll.value(0).toInt();
            const QString modId = qAll.value(1).toString();
            const QString act = qAll.value(2).toString();
            const QString desc = qAll.value(3).toString();

            const bool fromRole = rolePermIds.contains(permId);
            const bool effective = userOverrides.contains(permId) ? userOverrides.value(permId) : fromRole;

            QString label = QStringLiteral("%1 (%2)").arg(desc.isEmpty() ? act : desc, act);
            if (fromRole) {
                label += QStringLiteral("  [del rol]");
            }

            auto *cb = new QCheckBox(label, content);
            cb->setProperty("permission_id", permId);
            cb->setProperty("from_role", fromRole);
            cb->setChecked(effective);
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
    delQ.prepare(QStringLiteral("DELETE FROM user_permissions WHERE user_id = :uid;"));
    delQ.bindValue(QStringLiteral(":uid"), userId);
    bool ok = delQ.exec();

    if (ok) {
        QSqlQuery insQ(db);
        insQ.prepare(QStringLiteral(
            "INSERT INTO user_permissions (user_id, permission_id, granted) "
            "VALUES (:uid, :pid, :granted);"));

        for (auto it = checkboxesByModule.begin(); it != checkboxesByModule.end() && ok; ++it) {
            for (QCheckBox *cb : it.value()) {
                const bool fromRole = cb->property("from_role").toBool();
                const bool checked = cb->isChecked();
                // Solo almacenamos las diferencias respecto al rol para mantener la tabla limpia
                if (checked != fromRole) {
                    insQ.bindValue(QStringLiteral(":uid"), userId);
                    insQ.bindValue(QStringLiteral(":pid"), cb->property("permission_id").toInt());
                    insQ.bindValue(QStringLiteral(":granted"), checked ? 1 : 0);
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
            moduleId(),
            QStringLiteral("user_permissions_update"),
            QStringLiteral("Permisos personalizados guardados para %1 (ID %2)").arg(fullName).arg(userId));
        QMessageBox::information(this, QStringLiteral("Permisos guardados"),
                                 QStringLiteral("Se actualizaron los permisos de \"%1\".").arg(fullName));
    } else {
        db.rollback();
        QMessageBox::warning(this, QStringLiteral("Error"),
                             QStringLiteral("No se pudieron guardar los permisos del usuario."));
    }
}

void UsuariosModule::onResendNewPassword(int userId, const QString &fullName, const QString &email, bool isSystemRole)
{
    if (!requirePermission(QStringLiteral("reset_password"), QStringLiteral("reenviar contraseñas de usuarios"))) {
        return;
    }

    if (isSystemRole) {
        QMessageBox::information(
            this,
            QStringLiteral("Seguridad del Administrador"),
            QStringLiteral("La contraseña del usuario Administrador solo puede modificarse desde "
                           "Panel de Control → Seguridad y Licencia."));
        return;
    }

    const auto confirm = QMessageBox::question(
        this,
        QStringLiteral("Reenviar Nuevo Password"),
        QStringLiteral("Se generará una nueva contraseña aleatoria para \"%1\" (%2), se creará un nuevo archivo .txt "
                       "y se enviará automáticamente por email.\n\n"
                       "El usuario deberá cambiarla en su próximo inicio de sesión.\n\n¿Deseas continuar?")
            .arg(fullName, email),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);

    if (confirm != QMessageBox::Yes) {
        return;
    }

    const QString tempPass = Database::generateRandomPassword(12);

    QSqlQuery updQ(Database::instance().connection());
    updQ.prepare(QStringLiteral(
        "UPDATE users SET password_hash = :hash, must_change_password = 1 WHERE id = :id;"));
    updQ.bindValue(QStringLiteral(":hash"), Database::hashPassword(tempPass));
    updQ.bindValue(QStringLiteral(":id"), userId);

    if (!updQ.exec()) {
        QMessageBox::critical(this, QStringLiteral("Error"),
                              QStringLiteral("No se pudo actualizar la contraseña:\n%1").arg(updQ.lastError().text()));
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const CredentialDeliveryResult delivery = UserCredentials::deliverTemporaryPassword(
        fullName,
        email,
        tempPass,
        QStringLiteral("Global Manager - Nueva contraseña de acceso"),
        QStringLiteral("El administrador ha generado una nueva contraseña temporal para tu cuenta en Global Manager."),
        true);
    QApplication::restoreOverrideCursor();

    Database::instance().logAudit(
        Session::instance().currentUser().email,
        moduleId(),
        QStringLiteral("user_resend_password"),
        QStringLiteral("Nuevo password generado para %1 (%2) | Email enviado: %3")
            .arg(fullName, email, delivery.emailSent ? QStringLiteral("Sí") : QStringLiteral("No")));

    if (delivery.emailSent) {
        QMessageBox::information(
            this,
            QStringLiteral("Nuevo password enviado"),
            QStringLiteral("Se envió el nuevo archivo .txt de acceso a \"%1\" y se eliminó la copia local por seguridad.")
                .arg(email));
    } else {
        QMessageBox::warning(
            this,
            QStringLiteral("Password actualizado - Error enviando email"),
            QStringLiteral("La contraseña de \"%1\" fue restablecida, pero falló el envío por correo.\n\n"
                           "Motivo: %2\n\n"
                           "Se conservó temporalmente el archivo en:\n%3")
                .arg(fullName, delivery.errorMessage, delivery.txtPath));
    }

    loadUsersTable(m_searchEdit->text());
}

void UsuariosModule::onToggleBlockUser(
    int userId,
    const QString &fullName,
    const QString &email,
    bool currentlyActive,
    bool isSystemRole)
{
    if (!requirePermission(QStringLiteral("block"), QStringLiteral("bloquear o desbloquear usuarios"))) {
        return;
    }

    if (userId == Session::instance().currentUser().id && currentlyActive) {
        QMessageBox::warning(
            this,
            QStringLiteral("Acción no permitida"),
            QStringLiteral("No puedes bloquear tu propia cuenta mientras tienes la sesión activa."));
        return;
    }

    if (isSystemRole && currentlyActive) {
        QSqlQuery countAdmins(Database::instance().connection());
        countAdmins.exec(QStringLiteral(
            "SELECT COUNT(*) FROM users u INNER JOIN roles r ON r.id = u.role_id "
            "WHERE r.is_system = 1 AND u.is_active = 1;"));
        if (countAdmins.next() && countAdmins.value(0).toInt() <= 1) {
            QMessageBox::warning(
                this,
                QStringLiteral("Acción no permitida"),
                QStringLiteral("No se puede bloquear al único Administrador activo del sistema."));
            return;
        }
    }

    const QString actionWord = currentlyActive ? QStringLiteral("bloquear") : QStringLiteral("desbloquear");
    const auto confirm = QMessageBox::question(
        this,
        currentlyActive ? QStringLiteral("Bloquear usuario") : QStringLiteral("Desbloquear usuario"),
        QStringLiteral("¿Seguro que deseas %1 al usuario \"%2\" (%3)?").arg(actionWord, fullName, email));

    if (confirm != QMessageBox::Yes) {
        return;
    }

    QSqlQuery query(Database::instance().connection());
    query.prepare(QStringLiteral("UPDATE users SET is_active = :active WHERE id = :id;"));
    query.bindValue(QStringLiteral(":active"), currentlyActive ? 0 : 1);
    query.bindValue(QStringLiteral(":id"), userId);

    if (query.exec()) {
        Database::instance().logAudit(
            Session::instance().currentUser().email,
            moduleId(),
            currentlyActive ? QStringLiteral("user_block") : QStringLiteral("user_unblock"),
            QStringLiteral("Usuario: %1 (%2)").arg(fullName, email));
        loadUsersTable(m_searchEdit->text());
    }
}
