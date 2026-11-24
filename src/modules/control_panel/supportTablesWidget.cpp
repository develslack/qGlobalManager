#include "supportTablesWidget.h"
#include "externalSourceDialog.h"
#include "core/db.h"
#include "core/icons.h"
#include "core/session.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSet>
#include <QSplitter>
#include <QSqlError>
#include <QSqlQuery>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QVariant>

namespace {

constexpr int kVerticalBreakpointPx = 740;

QString compactJson(const QJsonObject &obj)
{
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

} // namespace

SupportTablesWidget::SupportTablesWidget(QWidget *parent)
    : QWidget(parent)
{
    registerSupportTables();
    setupUi();
    reloadAll();
}

void SupportTablesWidget::registerSupportTables()
{
    m_tables.clear();

    m_tables.append(SupportTableDef{
        QStringLiteral("roles"),
        QStringLiteral("Roles del Sistema"),
        QStringLiteral("Perfiles de usuario y matriz base de permisos por módulo."),
        QStringLiteral("Transversal"),
        {QStringLiteral("Seguridad / Sesión"), QStringLiteral("Usuarios"), QStringLiteral("Panel de Control")},
        QStringLiteral("Mixto (Roles núcleo protegidos + Roles personalizados)"),
        false, true, true, true,
        QString(),
        QStringLiteral("Ej: Cajero, Recepcionista, Odontólogo, Supervisor"),
        QStringLiteral("Descripción breve de las tareas de este rol")
    });

    m_tables.append(SupportTableDef{
        QStringLiteral("iva_conditions"),
        QStringLiteral("Condiciones frente al IVA"),
        QStringLiteral("Categorías fiscales utilizadas en empresa, clientes y comprobantes."),
        QStringLiteral("Transversal"),
        {QStringLiteral("Panel de Control (Empresa)"), QStringLiteral("Clientes"), QStringLiteral("Facturación / Ventas")},
        QStringLiteral("Mixto (Semillas fiscales iniciales + Carga manual)"),
        false, false, false, false,
        QString(),
        QStringLiteral("Ej: Responsable Inscripto, Monotributista, Exento"),
        QString()
    });

    m_tables.append(SupportTableDef{
        QStringLiteral("document_types"),
        QStringLiteral("Tipos de Documento"),
        QStringLiteral("Catálogo de identificaciones legales para personas y organizaciones."),
        QStringLiteral("Transversal"),
        {QStringLiteral("Clientes"), QStringLiteral("Pacientes"), QStringLiteral("Proveedores")},
        QStringLiteral("Mixto (Semillas estándar + Carga manual)"),
        true, false, false, false,
        QStringLiteral("Ej: DNI, CUIT, CUIL, PAS"),
        QStringLiteral("Ej: Documento Nacional de Identidad"),
        QString()
    });

    m_tables.append(SupportTableDef{
        QStringLiteral("payment_methods"),
        QStringLiteral("Medios de Pago"),
        QStringLiteral("Formas de cobro y pago habilitadas para caja, ventas y honorarios."),
        QStringLiteral("Transversal"),
        {QStringLiteral("Caja y Cobros"), QStringLiteral("Ventas"), QStringLiteral("Consultorios / Honorarios")},
        QStringLiteral("Manual / Configurable por comercio"),
        false, false, false, false,
        QString(),
        QStringLiteral("Ej: Efectivo, Transferencia Bancaria, Tarjeta Débito, QR"),
        QString()
    });
}

QString SupportTablesWidget::quoteIdent(const QString &ident) const
{
    if (Database::instance().isSqlite()) {
        return QStringLiteral("\"%1\"").arg(ident);
    }
    return QStringLiteral("`%1`").arg(ident);
}

void SupportTablesWidget::setupUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(10);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);

    // =========================================================================
    // Panel izquierdo: Listado maestro de Tablas Soporte + Fuentes Externas
    // =========================================================================
    auto *leftCard = new QGroupBox(QStringLiteral("Tablas Soporte del Sistema"), m_splitter);
    leftCard->setMinimumWidth(240);
    leftCard->setMaximumWidth(350);

    auto *leftLayout = new QVBoxLayout(leftCard);
    leftLayout->setSpacing(8);

    m_tableFilterEdit = new QLineEdit(leftCard);
    m_tableFilterEdit->setPlaceholderText(QStringLiteral("Filtrar tabla o fuente..."));
    m_tableFilterEdit->setClearButtonEnabled(true);
    leftLayout->addWidget(m_tableFilterEdit);

    m_tablesList = new QListWidget(leftCard);
    m_tablesList->setAlternatingRowColors(true);
    m_tablesList->setSpacing(2);
    m_tablesList->setMinimumHeight(200);
    leftLayout->addWidget(m_tablesList, 1);

    auto *addExtBtn = new QPushButton(QStringLiteral("Nueva fuente externa (JSON/CSV/TXT)"), leftCard);
    addExtBtn->setIcon(Icons::get(Icons::Name::Add, QColor(QStringLiteral("#FFFFFF")), 15));
    leftLayout->addWidget(addExtBtn);

    auto *leftHint = new QLabel(
        QStringLiteral("Haz clic sobre una tabla para auditar, consultar, agregar o deprecar registros."),
        leftCard);
    leftHint->setWordWrap(true);
    leftHint->setStyleSheet(QStringLiteral("color: #52606D; font-size: 8.5pt;"));
    leftLayout->addWidget(leftHint);

    m_splitter->addWidget(leftCard);

    // =========================================================================
    // Panel derecho: QStackedWidget (0: Vacío | 1: Tabla Estándar | 2: Fuente Externa)
    // =========================================================================
    m_rightStack = new QStackedWidget(m_splitter);

    // --- Página 0: Sugerencia inicial ---
    auto *emptyPage = new QWidget(m_rightStack);
    auto *emptyLayout = new QVBoxLayout(emptyPage);
    auto *emptyLbl = new QLabel(
        QStringLiteral("<h3 style='color:#1E3A5F;'>Selecciona una tabla soporte</h3>"
                       "<p style='color:#52606D;'>Elige una tabla del listado izquierdo para consultar sus registros, "
                       "inspeccionar sus relaciones entre módulos, dar de alta elementos o deprecar datos históricos "
                       "sin perder trazabilidad.</p>"
                       "<p style='color:#52606D;'>También puedes incorporar <b>fuentes externas agnósticas</b> "
                       "(URLs JSON o archivos CSV/TXT) con el botón inferior izquierdo.</p>"),
        emptyPage);
    emptyLbl->setAlignment(Qt::AlignCenter);
    emptyLbl->setWordWrap(true);
    emptyLayout->addWidget(emptyLbl, 1);
    m_rightStack->addWidget(emptyPage);

    // --- Página 1: Tabla Soporte Estándar ---
    auto *stdPage = new QWidget(m_rightStack);
    auto *stdLayout = new QVBoxLayout(stdPage);
    stdLayout->setContentsMargins(8, 0, 0, 0);
    stdLayout->setSpacing(10);

    auto *stdHeader = new QFrame(stdPage);
    stdHeader->setObjectName(QStringLiteral("CardFrame"));
    auto *stdHeaderRow = new QHBoxLayout(stdHeader);
    stdHeaderRow->setContentsMargins(14, 10, 14, 10);

    auto *stdTitles = new QVBoxLayout();
    stdTitles->setSpacing(2);
    m_stdTitleLabel = new QLabel(stdHeader);
    m_stdTitleLabel->setStyleSheet(QStringLiteral("font-size: 13pt; font-weight: 700; color: #1E3A5F;"));
    m_stdTitleLabel->setWordWrap(true);

    m_stdSubtitleLabel = new QLabel(stdHeader);
    m_stdSubtitleLabel->setStyleSheet(QStringLiteral("color: #52606D;"));
    m_stdSubtitleLabel->setWordWrap(true);

    stdTitles->addWidget(m_stdTitleLabel);
    stdTitles->addWidget(m_stdSubtitleLabel);
    stdHeaderRow->addLayout(stdTitles, 1);

    auto *stdInfoBtn = new QPushButton(QStringLiteral("Info / Relaciones"), stdHeader);
    stdInfoBtn->setIcon(Icons::get(Icons::Name::Database, QColor(QStringLiteral("#1E3A5F")), 15));
    stdInfoBtn->setProperty("secondary", true);
    stdHeaderRow->addWidget(stdInfoBtn);
    stdLayout->addWidget(stdHeader);

    // Formulario de Alta / Edición
    auto *formGroup = new QGroupBox(QStringLiteral("Alta / Edición de Registro"), stdPage);
    auto *formGroupLayout = new QVBoxLayout(formGroup);
    formGroupLayout->setSpacing(8);

    auto *form = new QFormLayout();
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_codeLabel = new QLabel(QStringLiteral("Código *:"), formGroup);
    m_codeEdit = new QLineEdit(formGroup);

    m_nameLabel = new QLabel(QStringLiteral("Nombre *:"), formGroup);
    m_nameEdit = new QLineEdit(formGroup);

    m_descLabel = new QLabel(QStringLiteral("Descripción:"), formGroup);
    m_descEdit = new QLineEdit(formGroup);

    form->addRow(m_codeLabel, m_codeEdit);
    form->addRow(m_nameLabel, m_nameEdit);
    form->addRow(m_descLabel, m_descEdit);
    formGroupLayout->addLayout(form);

    auto *btnRow = new QHBoxLayout();
    m_saveBtn = new QPushButton(QStringLiteral("Agregar registro"), formGroup);
    m_saveBtn->setIcon(Icons::get(Icons::Name::Add, QColor(QStringLiteral("#FFFFFF")), 15));

    m_cancelBtn = new QPushButton(QStringLiteral("Limpiar"), formGroup);
    m_cancelBtn->setIcon(Icons::get(Icons::Name::Refresh, QColor(QStringLiteral("#1E3A5F")), 15));
    m_cancelBtn->setProperty("secondary", true);

    btnRow->addWidget(m_saveBtn);
    btnRow->addWidget(m_cancelBtn);
    btnRow->addStretch();
    formGroupLayout->addLayout(btnRow);
    stdLayout->addWidget(formGroup);

    // Grilla de registros (con baja lógica Deprecar / Reactivar)
    auto *listGroup = new QGroupBox(QStringLiteral("Registros de la Tabla (Trazabilidad sin borrado físico)"), stdPage);
    auto *listLayout = new QVBoxLayout(listGroup);
    listLayout->setSpacing(6);

    auto *filterBar = new QHBoxLayout();
    m_showDeprecatedCheck = new QCheckBox(QStringLiteral("Mostrar registros deprecados (históricos)"), listGroup);
    m_showDeprecatedCheck->setChecked(true);
    m_recordsSummaryLabel = new QLabel(listGroup);
    m_recordsSummaryLabel->setStyleSheet(QStringLiteral("color: #52606D; font-size: 9pt;"));
    filterBar->addWidget(m_showDeprecatedCheck);
    filterBar->addStretch();
    filterBar->addWidget(m_recordsSummaryLabel);
    listLayout->addLayout(filterBar);

    m_recordsTable = new QTableWidget(listGroup);
    m_recordsTable->setColumnCount(5);
    m_recordsTable->setHorizontalHeaderLabels({
        QStringLiteral("Código"),
        QStringLiteral("Nombre"),
        QStringLiteral("Descripción"),
        QStringLiteral("Estado / Reemplazo"),
        QStringLiteral("Acciones")
    });
    m_recordsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_recordsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_recordsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_recordsTable->setAlternatingRowColors(true);
    m_recordsTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_recordsTable->verticalHeader()->setVisible(false);
    m_recordsTable->verticalHeader()->setDefaultSectionSize(42);
    m_recordsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_recordsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_recordsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_recordsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_recordsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_recordsTable->setMinimumHeight(210);

    listLayout->addWidget(m_recordsTable, 1);
    stdLayout->addWidget(listGroup, 1);
    m_rightStack->addWidget(stdPage);

    // --- Página 2: Tabla Soporte Externa Dinámica ---
    auto *extPage = new QWidget(m_rightStack);
    auto *extLayout = new QVBoxLayout(extPage);
    extLayout->setContentsMargins(8, 0, 0, 0);
    extLayout->setSpacing(10);

    auto *extHeader = new QFrame(extPage);
    extHeader->setObjectName(QStringLiteral("CardFrame"));
    auto *extHeaderRow = new QHBoxLayout(extHeader);
    extHeaderRow->setContentsMargins(14, 10, 14, 10);

    auto *extTitles = new QVBoxLayout();
    extTitles->setSpacing(3);
    m_extTitleLabel = new QLabel(extHeader);
    m_extTitleLabel->setStyleSheet(QStringLiteral("font-size: 13pt; font-weight: 700; color: #1E3A5F;"));
    m_extTitleLabel->setWordWrap(true);

    m_extMetaLabel = new QLabel(extHeader);
    m_extMetaLabel->setStyleSheet(QStringLiteral("color: #52606D;"));
    m_extMetaLabel->setWordWrap(true);

    extTitles->addWidget(m_extTitleLabel);
    extTitles->addWidget(m_extMetaLabel);
    extHeaderRow->addLayout(extTitles, 1);

    auto *extInfoBtn = new QPushButton(QStringLiteral("Info / Origen"), extHeader);
    extInfoBtn->setIcon(Icons::get(Icons::Name::Database, QColor(QStringLiteral("#1E3A5F")), 15));
    extInfoBtn->setProperty("secondary", true);
    extHeaderRow->addWidget(extInfoBtn);
    extLayout->addWidget(extHeader);

    auto *extActionRow = new QHBoxLayout();
    auto *syncBtn = new QPushButton(QStringLiteral("Sincronizar ahora"), extPage);
    syncBtn->setIcon(Icons::get(Icons::Name::Refresh, QColor(QStringLiteral("#FFFFFF")), 15));

    auto *editExtBtn = new QPushButton(QStringLiteral("Configurar fuente"), extPage);
    editExtBtn->setIcon(Icons::get(Icons::Name::Edit, QColor(QStringLiteral("#1E3A5F")), 15));
    editExtBtn->setProperty("secondary", true);

    m_extToggleActiveBtn = new QPushButton(QStringLiteral("Deprecar fuente"), extPage);
    m_extToggleActiveBtn->setIcon(Icons::get(Icons::Name::Lock, QColor(QStringLiteral("#1E3A5F")), 15));
    m_extToggleActiveBtn->setProperty("secondary", true);

    extActionRow->addWidget(syncBtn);
    extActionRow->addWidget(editExtBtn);
    extActionRow->addWidget(m_extToggleActiveBtn);
    extActionRow->addStretch();
    extLayout->addLayout(extActionRow);

    auto *extListGroup = new QGroupBox(QStringLiteral("Datos Sincronizados desde Fuente Externa"), extPage);
    auto *extListLayout = new QVBoxLayout(extListGroup);

    m_extItemsTable = new QTableWidget(extListGroup);
    m_extItemsTable->setColumnCount(5);
    m_extItemsTable->setHorizontalHeaderLabels({
        QStringLiteral("Clave"),
        QStringLiteral("Nombre / Etiqueta"),
        QStringLiteral("Valor Numérico"),
        QStringLiteral("Valor Texto"),
        QStringLiteral("Actualizado (UTC)")
    });
    m_extItemsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_extItemsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_extItemsTable->setAlternatingRowColors(true);
    m_extItemsTable->verticalHeader()->setVisible(false);
    m_extItemsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_extItemsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_extItemsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_extItemsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_extItemsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    extListLayout->addWidget(m_extItemsTable, 1);

    extLayout->addWidget(extListGroup, 1);
    m_rightStack->addWidget(extPage);

    m_splitter->addWidget(m_rightStack);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    rootLayout->addWidget(m_splitter, 1);

    // Conexiones
    connect(m_tableFilterEdit, &QLineEdit::textChanged, this, &SupportTablesWidget::onFilterTablesChanged);
    connect(m_tablesList, &QListWidget::currentRowChanged, this, &SupportTablesWidget::onListItemSelected);
    connect(addExtBtn, &QPushButton::clicked, this, &SupportTablesWidget::onAddExternalSource);

    connect(stdInfoBtn, &QPushButton::clicked, this, &SupportTablesWidget::onShowTableInfoModal);
    connect(extInfoBtn, &QPushButton::clicked, this, &SupportTablesWidget::onShowTableInfoModal);

    connect(m_saveBtn, &QPushButton::clicked, this, &SupportTablesWidget::onSaveRecord);
    connect(m_cancelBtn, &QPushButton::clicked, this, &SupportTablesWidget::onResetForm);
    connect(m_showDeprecatedCheck, &QCheckBox::toggled, this, [this](bool) {
        loadCurrentStandardTableRecords();
    });

    connect(syncBtn, &QPushButton::clicked, this, &SupportTablesWidget::onSyncCurrentExternalSource);
    connect(editExtBtn, &QPushButton::clicked, this, &SupportTablesWidget::onEditCurrentExternalSource);
    connect(m_extToggleActiveBtn, &QPushButton::clicked, this, &SupportTablesWidget::onToggleCurrentExternalSourceActive);
}

void SupportTablesWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    const Qt::Orientation target =
        (event->size().width() < kVerticalBreakpointPx) ? Qt::Vertical : Qt::Horizontal;
    if (m_splitter->orientation() != target) {
        m_splitter->setOrientation(target);
    }
}

void SupportTablesWidget::reloadAll()
{
    const bool prevExt = m_selectedIsExternal;
    const int prevId   = m_selectedIndexOrSourceId;

    m_externalSources = ExternalSourceManager::loadSources(true);
    populateTablesList(m_tableFilterEdit->text());

    for (int r = 0; r < m_visibleEntries.size(); ++r) {
        if (m_visibleEntries.at(r).isExternal == prevExt &&
            m_visibleEntries.at(r).indexOrSourceId == prevId) {
            m_tablesList->blockSignals(true);
            m_tablesList->setCurrentRow(r);
            m_tablesList->blockSignals(false);
            if (prevExt) {
                loadCurrentExternalSourceItems();
            } else {
                loadCurrentStandardTableRecords();
            }
            return;
        }
    }

    m_selectedIndexOrSourceId = -1;
    m_rightStack->setCurrentIndex(0);
}

int SupportTablesWidget::countStandardRecords(const QString &tableName, int *deprecatedCountOut) const
{
    if (deprecatedCountOut) *deprecatedCountOut = 0;
    QSqlQuery q(Database::instance().connection());
    if (q.exec(QStringLiteral(
            "SELECT COUNT(*), COALESCE(SUM(CASE WHEN is_deprecated = 1 THEN 1 ELSE 0 END), 0) "
            "FROM %1;").arg(quoteIdent(tableName))) && q.next()) {
        if (deprecatedCountOut) *deprecatedCountOut = q.value(1).toInt();
        return q.value(0).toInt();
    }
    return 0;
}

void SupportTablesWidget::populateTablesList(const QString &filter)
{
    const QString clean = filter.trimmed();
    m_tablesList->blockSignals(true);
    m_tablesList->clear();
    m_visibleEntries.clear();

    const QColor stdColor(QStringLiteral("#1E3A5F"));
    const QColor extColor(QStringLiteral("#2B6CB0"));
    const QColor mutedColor(QStringLiteral("#9AA5B1"));

    // 1. Tablas soporte estándar
    for (int i = 0; i < m_tables.size(); ++i) {
        const SupportTableDef &def = m_tables.at(i);
        if (!clean.isEmpty() &&
            !def.displayName.contains(clean, Qt::CaseInsensitive) &&
            !def.subtitle.contains(clean, Qt::CaseInsensitive)) {
            continue;
        }
        int depCount = 0;
        const int total = countStandardRecords(def.tableName, &depCount);
        const QString label = depCount > 0
                                  ? QStringLiteral("%1 (%2 · %3 dep.)").arg(def.displayName).arg(total).arg(depCount)
                                  : QStringLiteral("%1 (%2)").arg(def.displayName).arg(total);

        auto *item = new QListWidgetItem(
            Icons::get(def.isRolesTable ? Icons::Name::Role : Icons::Name::Table, stdColor, 16),
            label);
        item->setToolTip(QStringLiteral("%1 · Alcance: %2").arg(def.subtitle, def.scopeType));
        m_tablesList->addItem(item);
        m_visibleEntries.append(CatalogListEntry{false, i});
    }

    // 2. Fuentes externas dinámicas
    for (const ExternalSourceDef &src : m_externalSources) {
        if (!clean.isEmpty() &&
            !src.name.contains(clean, Qt::CaseInsensitive) &&
            !src.targetModule.contains(clean, Qt::CaseInsensitive)) {
            continue;
        }
        const int itemsCount = ExternalSourceManager::countItems(src.id);
        const QString stateSuffix = src.isActive ? QStringLiteral("[Externa]") : QStringLiteral("[Inactiva]");
        auto *item = new QListWidgetItem(
            Icons::get(Icons::Name::Database, src.isActive ? extColor : mutedColor, 16),
            QStringLiteral("%1 (%2) %3").arg(src.name).arg(itemsCount).arg(stateSuffix));
        if (!src.isActive) {
            item->setForeground(QBrush(mutedColor));
        }
        item->setToolTip(QStringLiteral("Origen: %1 (%2) · Módulo: %3")
                             .arg(src.sourceUri, src.format, ExternalSourceManager::moduleDisplayName(src.targetModule)));
        m_tablesList->addItem(item);
        m_visibleEntries.append(CatalogListEntry{true, src.id});
    }

    m_tablesList->blockSignals(false);
}

void SupportTablesWidget::onFilterTablesChanged(const QString &text)
{
    populateTablesList(text);
    for (int r = 0; r < m_visibleEntries.size(); ++r) {
        if (m_visibleEntries.at(r).isExternal == m_selectedIsExternal &&
            m_visibleEntries.at(r).indexOrSourceId == m_selectedIndexOrSourceId) {
            m_tablesList->blockSignals(true);
            m_tablesList->setCurrentRow(r);
            m_tablesList->blockSignals(false);
            break;
        }
    }
}

const SupportTablesWidget::SupportTableDef *SupportTablesWidget::currentStandardDef() const
{
    if (m_selectedIsExternal || m_selectedIndexOrSourceId < 0 || m_selectedIndexOrSourceId >= m_tables.size()) {
        return nullptr;
    }
    return &m_tables.at(m_selectedIndexOrSourceId);
}

void SupportTablesWidget::onListItemSelected(int row)
{
    if (row < 0 || row >= m_visibleEntries.size()) {
        m_selectedIndexOrSourceId = -1;
        m_rightStack->setCurrentIndex(0);
        return;
    }

    const CatalogListEntry entry = m_visibleEntries.at(row);
    m_selectedIsExternal = entry.isExternal;
    m_selectedIndexOrSourceId = entry.indexOrSourceId;

    if (!entry.isExternal) {
        const SupportTableDef *def = currentStandardDef();
        if (!def) return;

        m_stdTitleLabel->setText(def->displayName);
        m_stdSubtitleLabel->setText(QStringLiteral("%1 · Alcance: %2").arg(def->subtitle, def->scopeType));

        m_codeLabel->setVisible(def->hasCode);
        m_codeEdit->setVisible(def->hasCode);
        m_codeEdit->setPlaceholderText(def->codePlaceholder);

        m_nameEdit->setPlaceholderText(def->namePlaceholder);

        m_descLabel->setVisible(def->hasDescription);
        m_descEdit->setVisible(def->hasDescription);
        m_descEdit->setPlaceholderText(def->descPlaceholder);

        m_recordsTable->setColumnHidden(0, !def->hasCode);
        m_recordsTable->setColumnHidden(2, !def->hasDescription);

        onResetForm();
        loadCurrentStandardTableRecords();
        m_rightStack->setCurrentIndex(1);

        // Registrar consulta (VIEW) en la bitácora inmutable
        AuditEvent viewEv;
        viewEv.moduleId = QStringLiteral("panel_control");
        viewEv.targetTable = def->tableName;
        viewEv.action = QStringLiteral("VIEW");
        viewEv.details = QStringLiteral("Consulta de tabla soporte: %1").arg(def->displayName);
        Database::instance().logAuditEvent(viewEv);
    } else {
        loadCurrentExternalSourceItems();
        m_rightStack->setCurrentIndex(2);

        ExternalSourceDef src;
        if (ExternalSourceManager::loadSourceById(entry.indexOrSourceId, &src)) {
            AuditEvent viewEv;
            viewEv.moduleId = QStringLiteral("panel_control");
            viewEv.targetTable = QStringLiteral("external_source_items");
            viewEv.recordId = QString::number(src.id);
            viewEv.action = QStringLiteral("VIEW");
            viewEv.details = QStringLiteral("Consulta de fuente externa: %1").arg(src.name);
            Database::instance().logAuditEvent(viewEv);
        }
    }
}

QStringList SupportTablesWidget::introspectReferencingTables(const QString &tableName) const
{
    QStringList refs;
    const Database &db = Database::instance();
    if (!db.isOpen()) return refs;

    if (db.isSqlite()) {
        QSqlQuery tblQ(db.connection());
        if (tblQ.exec(QStringLiteral(
                "SELECT name FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%';"))) {
            while (tblQ.next()) {
                const QString childTable = tblQ.value(0).toString();
                QSqlQuery fkQ(db.connection());
                if (fkQ.exec(QStringLiteral("PRAGMA foreign_key_list(\"%1\");").arg(childTable))) {
                    while (fkQ.next()) {
                        const QString refTable = fkQ.value(2).toString();
                        const QString fromCol  = fkQ.value(3).toString();
                        const QString onDelete = fkQ.value(6).toString();
                        if (refTable.compare(tableName, Qt::CaseInsensitive) == 0) {
                            refs.append(QStringLiteral("%1.%2 (ON DELETE %3)").arg(childTable, fromCol, onDelete));
                        }
                    }
                }
            }
        }
    } else {
        QSqlQuery fkQ(db.connection());
        fkQ.prepare(QStringLiteral(
            "SELECT TABLE_NAME, COLUMN_NAME "
            "FROM information_schema.KEY_COLUMN_USAGE "
            "WHERE TABLE_SCHEMA = DATABASE() AND REFERENCED_TABLE_NAME = :tbl "
            "ORDER BY TABLE_NAME, COLUMN_NAME;"));
        fkQ.bindValue(QStringLiteral(":tbl"), tableName);
        if (fkQ.exec()) {
            while (fkQ.next()) {
                refs.append(QStringLiteral("%1.%2 (FK activa en MariaDB)")
                                .arg(fkQ.value(0).toString(), fkQ.value(1).toString()));
            }
        }
    }

    return refs;
}

void SupportTablesWidget::onShowTableInfoModal()
{
    QDialog dlg(this);
    dlg.setModal(true);
    dlg.setMinimumWidth(480);

    auto *layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(12);

    QString html;

    if (!m_selectedIsExternal) {
        const SupportTableDef *def = currentStandardDef();
        if (!def) return;

        dlg.setWindowTitle(QStringLiteral("Ficha Técnica - %1").arg(def->displayName));

        const QStringList refs = introspectReferencingTables(def->tableName);
        int depCount = 0;
        const int totalCount = countStandardRecords(def->tableName, &depCount);

        const QString refsHtml = refs.isEmpty()
            ? QStringLiteral("<i>Sin claves foráneas físicas entrantes actualmente (tabla consultada por referencia lógica).</i>")
            : QStringLiteral("<ul><li>%1</li></ul>").arg(refs.join(QStringLiteral("</li><li>")));

        html = QStringLiteral(
            "<h3 style='color:#1E3A5F; margin-top:0;'>%1 (<code>%2</code>)</h3>"
            "<p>%3</p>"
            "<hr>"
            "<p><b>1. Alcance modular:</b> %4<br>"
            "<b>Módulos que la utilizan:</b> %5</p>"
            "<p><b>2. Relaciones e Integridad Referencial (en vivo):</b><br>%6"
            "<b>Protección contra borrado redundante:</b> Activa (el motor SQL y la interfaz bloquean el borrado físico).</p>"
            "<p><b>3. Origen de los datos:</b> %7</p>"
            "<p><b>4. Regla de baja y auditoría:</b> Los registros nunca se eliminan físicamente; se marcan como "
            "<b>Deprecados</b> con opción de indicar un reemplazo vigente. Siguen visibles en informes históricos, "
            "pero quedan bloqueados para nuevas cargas. Cada consulta, alta, cambio o deprecado queda sellado en SHA-256.</p>"
            "<p style='color:#52606D;'>Registros totales: <b>%8</b> (Vigentes: <b>%9</b> · Deprecados: <b>%10</b>)</p>")
                   .arg(def->displayName.toHtmlEscaped(),
                        def->tableName.toHtmlEscaped(),
                        def->subtitle.toHtmlEscaped(),
                        def->scopeType.toHtmlEscaped(),
                        def->servedModules.join(QStringLiteral(", ")).toHtmlEscaped(),
                        refsHtml,
                        def->originType.toHtmlEscaped())
                   .arg(totalCount)
                   .arg(totalCount - depCount)
                   .arg(depCount);
    } else {
        ExternalSourceDef src;
        if (!ExternalSourceManager::loadSourceById(m_selectedIndexOrSourceId, &src)) return;

        dlg.setWindowTitle(QStringLiteral("Ficha Técnica Fuente Externa - %1").arg(src.name));
        const int itemsCount = ExternalSourceManager::countItems(src.id);

        html = QStringLiteral(
            "<h3 style='color:#1E3A5F; margin-top:0;'>%1 [Fuente Externa Dinámica]</h3>"
            "<hr>"
            "<p><b>1. Alcance modular:</b> %2</p>"
            "<p><b>2. Origen externo configurado:</b><br>"
            "• Tipo: <b>%3</b> · Formato: <b>%4</b><br>"
            "• Ubicación: <code>%5</code></p>"
            "<p><b>3. Mapeo de campos:</b><br>"
            "• Clave: <code>%6</code> · Etiqueta: <code>%7</code> · Valor: <code>%8</code></p>"
            "<p><b>4. Estado y sincronización:</b><br>"
            "• Estado: <b>%9</b><br>"
            "• Última sincronización: <b>%10</b><br>"
            "• Registros almacenados localmente: <b>%11</b> (disponibles offline si cae internet)</p>")
                   .arg(src.name.toHtmlEscaped(),
                        ExternalSourceManager::moduleDisplayName(src.targetModule).toHtmlEscaped(),
                        src.sourceType.toHtmlEscaped(),
                        src.format.toHtmlEscaped(),
                        src.sourceUri.toHtmlEscaped(),
                        src.keyField.toHtmlEscaped(),
                        src.labelField.toHtmlEscaped(),
                        src.valueField.isEmpty() ? QStringLiteral("(Sin valor)") : src.valueField.toHtmlEscaped(),
                        src.isActive ? QStringLiteral("Activa") : QStringLiteral("Inactiva / Deprecada"),
                        src.lastSyncAt.isEmpty() ? QStringLiteral("Nunca sincronizada") : src.lastSyncAt.toHtmlEscaped())
                   .arg(itemsCount);
    }

    auto *infoLbl = new QLabel(html, &dlg);
    infoLbl->setWordWrap(true);
    infoLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(infoLbl);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto *closeBtn = new QPushButton(QStringLiteral("Cerrar"), &dlg);
    closeBtn->setIcon(Icons::get(Icons::Name::Close, QColor(QStringLiteral("#FFFFFF")), 15));
    btnRow->addWidget(closeBtn);
    layout->addLayout(btnRow);

    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    dlg.exec();
}

void SupportTablesWidget::loadCurrentStandardTableRecords()
{
    m_recordsTable->setRowCount(0);
    const SupportTableDef *def = currentStandardDef();
    if (!def) return;

    const QString tbl = quoteIdent(def->tableName);
    const QString codeCol = def->hasCode ? QStringLiteral("code") : QStringLiteral("''");
    const QString descCol = def->hasDescription ? QStringLiteral("description") : QStringLiteral("''");
    const QString sysCol  = def->hasIsSystem ? QStringLiteral("is_system") : QStringLiteral("0");
    const QString orderBy = def->hasIsSystem ? QStringLiteral("is_deprecated ASC, is_system DESC, name ASC")
                                             : (def->hasCode ? QStringLiteral("is_deprecated ASC, code ASC")
                                                             : QStringLiteral("is_deprecated ASC, name ASC"));

    // Mapa id -> nombre para resolver reemplazos sugeridos
    QMap<int, QString> namesById;
    {
        QSqlQuery nq(Database::instance().connection());
        if (nq.exec(QStringLiteral("SELECT id, name FROM %1;").arg(tbl))) {
            while (nq.next()) {
                namesById.insert(nq.value(0).toInt(), nq.value(1).toString());
            }
        }
    }

    const QString sql = QStringLiteral(
        "SELECT id, %1, name, %2, %3, is_deprecated, replacement_id, deprecation_note "
        "FROM %4 ORDER BY %5;")
                            .arg(codeCol, descCol, sysCol, tbl, orderBy);

    QSqlQuery query(Database::instance().connection());
    if (!query.exec(sql)) return;

    const bool showDep = m_showDeprecatedCheck->isChecked();
    int totalRows = 0;
    int depRows = 0;
    int row = 0;

    while (query.next()) {
        ++totalRows;
        const int id          = query.value(0).toInt();
        const QString code    = query.value(1).toString();
        const QString name    = query.value(2).toString();
        const QString desc    = query.value(3).toString();
        const bool isSystem   = (query.value(4).toInt() == 1);
        const bool deprecated = (query.value(5).toInt() == 1);
        const int replId      = query.value(6).isNull() ? -1 : query.value(6).toInt();
        const QString depNote = query.value(7).toString().trimmed();

        if (deprecated) {
            ++depRows;
            if (!showDep) continue;
        }

        m_recordsTable->insertRow(row);
        m_recordsTable->setItem(row, 0, new QTableWidgetItem(code));

        QString displayName = isSystem ? (name + QStringLiteral(" (Sistema)")) : name;
        if (deprecated) {
            displayName += QStringLiteral(" [Deprecado]");
        }
        m_recordsTable->setItem(row, 1, new QTableWidgetItem(displayName));
        m_recordsTable->setItem(row, 2, new QTableWidgetItem(desc));

        QString statusText = QStringLiteral("Vigente");
        if (deprecated) {
            statusText = QStringLiteral("Deprecado");
            if (replId > 0 && namesById.contains(replId)) {
                statusText += QStringLiteral(" · Usar: %1").arg(namesById.value(replId));
            }
            if (!depNote.isEmpty()) {
                statusText += QStringLiteral(" (%1)").arg(depNote);
            }
        }
        auto *statusItem = new QTableWidgetItem(statusText);
        statusItem->setForeground(QBrush(QColor(deprecated ? QStringLiteral("#B7791F") : QStringLiteral("#2E7D32"))));
        m_recordsTable->setItem(row, 3, statusItem);

        if (deprecated) {
            const QBrush depBg(QColor(QStringLiteral("#FFFBEB")));
            const QBrush depFg(QColor(QStringLiteral("#744210")));
            for (int c = 0; c < 4; ++c) {
                if (QTableWidgetItem *it = m_recordsTable->item(row, c)) {
                    it->setBackground(depBg);
                    if (c != 3) it->setForeground(depFg);
                }
            }
        }

        // Botonera incrustada por fila
        auto *actionsWidget = new QWidget(m_recordsTable);
        auto *actionsLayout = new QHBoxLayout(actionsWidget);
        actionsLayout->setContentsMargins(4, 3, 4, 3);
        actionsLayout->setSpacing(6);

        auto *editBtn = new QPushButton(QStringLiteral("Editar"), actionsWidget);
        editBtn->setIcon(Icons::get(Icons::Name::Edit, QColor(QStringLiteral("#1E3A5F")), 14));
        editBtn->setProperty("secondary", true);
        editBtn->setEnabled(!isSystem);
        actionsLayout->addWidget(editBtn);

        if (def->isRolesTable) {
            auto *permsBtn = new QPushButton(QStringLiteral("Permisos"), actionsWidget);
            permsBtn->setIcon(Icons::get(Icons::Name::Shield, QColor(QStringLiteral("#FFFFFF")), 14));
            connect(permsBtn, &QPushButton::clicked, this, [this, id, name, isSystem]() {
                onConfigureRolePermissions(id, name, isSystem);
            });
            actionsLayout->addWidget(permsBtn);
        }

        if (!deprecated) {
            auto *depBtn = new QPushButton(QStringLiteral("Deprecar"), actionsWidget);
            depBtn->setIcon(Icons::get(Icons::Name::Lock, QColor(QStringLiteral("#FFFFFF")), 14));
            depBtn->setProperty("danger", true);
            depBtn->setEnabled(!isSystem);
            connect(depBtn, &QPushButton::clicked, this, [this, id, name, isSystem]() {
                onDeprecateRecord(id, name, isSystem);
            });
            actionsLayout->addWidget(depBtn);
        } else {
            auto *reactBtn = new QPushButton(QStringLiteral("Reactivar"), actionsWidget);
            reactBtn->setIcon(Icons::get(Icons::Name::Unlock, QColor(QStringLiteral("#1E3A5F")), 14));
            reactBtn->setProperty("secondary", true);
            connect(reactBtn, &QPushButton::clicked, this, [this, id, name]() {
                onReactivateRecord(id, name);
            });
            actionsLayout->addWidget(reactBtn);
        }

        connect(editBtn, &QPushButton::clicked, this, [this, id, code, name, desc]() {
            onEditRecord(id, code, name, desc);
        });

        m_recordsTable->setCellWidget(row, 4, actionsWidget);
        ++row;
    }

    m_recordsTable->resizeColumnToContents(4);
    m_recordsSummaryLabel->setText(
        QStringLiteral("Vigentes: %1 · Deprecados: %2").arg(totalRows - depRows).arg(depRows));

    // Refrescar contador en el listado izquierdo
    for (int i = 0; i < m_visibleEntries.size(); ++i) {
        if (!m_visibleEntries.at(i).isExternal &&
            m_visibleEntries.at(i).indexOrSourceId == m_selectedIndexOrSourceId &&
            m_tablesList->item(i)) {
            const QString label = depRows > 0
                                      ? QStringLiteral("%1 (%2 · %3 dep.)").arg(def->displayName).arg(totalRows).arg(depRows)
                                      : QStringLiteral("%1 (%2)").arg(def->displayName).arg(totalRows);
            m_tablesList->item(i)->setText(label);
            break;
        }
    }
}

void SupportTablesWidget::loadCurrentExternalSourceItems()
{
    m_extItemsTable->setRowCount(0);
    if (!m_selectedIsExternal || m_selectedIndexOrSourceId <= 0) return;

    ExternalSourceDef src;
    if (!ExternalSourceManager::loadSourceById(m_selectedIndexOrSourceId, &src)) {
        m_rightStack->setCurrentIndex(0);
        return;
    }

    m_extTitleLabel->setText(
        QStringLiteral("%1 %2").arg(src.name, src.isActive ? QStringLiteral("[Activa]") : QStringLiteral("[Inactiva]")));
    m_extMetaLabel->setText(
        QStringLiteral("Origen (%1/%2): %3  ·  Módulo: %4  ·  Última sync: %5")
            .arg(src.sourceType, src.format, src.sourceUri,
                 ExternalSourceManager::moduleDisplayName(src.targetModule),
                 src.lastSyncAt.isEmpty() ? QStringLiteral("Pendiente") : src.lastSyncAt));

    m_extToggleActiveBtn->setText(src.isActive ? QStringLiteral("Deprecar fuente") : QStringLiteral("Reactivar fuente"));

    const QVector<ExternalSourceItem> items = ExternalSourceManager::loadItems(src.id);
    m_extItemsTable->setRowCount(items.size());

    for (int r = 0; r < items.size(); ++r) {
        const ExternalSourceItem &it = items.at(r);
        m_extItemsTable->setItem(r, 0, new QTableWidgetItem(it.itemKey));
        m_extItemsTable->setItem(r, 1, new QTableWidgetItem(it.itemLabel));
        m_extItemsTable->setItem(r, 2, new QTableWidgetItem(it.hasNumericValue ? QString::number(it.valueNum, 'f', 2) : QStringLiteral("-")));
        m_extItemsTable->setItem(r, 3, new QTableWidgetItem(it.valueText));
        m_extItemsTable->setItem(r, 4, new QTableWidgetItem(it.updatedAt));
    }

    // Actualizar contador en listado izquierdo
    for (int i = 0; i < m_visibleEntries.size(); ++i) {
        if (m_visibleEntries.at(i).isExternal &&
            m_visibleEntries.at(i).indexOrSourceId == src.id &&
            m_tablesList->item(i)) {
            m_tablesList->item(i)->setText(
                QStringLiteral("%1 (%2) %3")
                    .arg(src.name)
                    .arg(items.size())
                    .arg(src.isActive ? QStringLiteral("[Externa]") : QStringLiteral("[Inactiva]")));
            break;
        }
    }
}

void SupportTablesWidget::onSaveRecord()
{
    const SupportTableDef *def = currentStandardDef();
    if (!def) return;

    const QString code = m_codeEdit->text().trimmed().toUpper();
    const QString name = m_nameEdit->text().trimmed();
    const QString desc = m_descEdit->text().trimmed();

    if (def->hasCode && code.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Dato requerido"), QStringLiteral("Ingresa el código corto."));
        m_codeEdit->setFocus();
        return;
    }
    if (name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Dato requerido"), QStringLiteral("Ingresa el nombre del registro."));
        m_nameEdit->setFocus();
        return;
    }

    const QString tbl = quoteIdent(def->tableName);
    const bool isCreate = (m_editingRecordId < 0);

    // Capturar estado previo para auditoría JSON en caso de UPDATE
    QJsonObject oldJson;
    if (!isCreate) {
        const QString codeSel = def->hasCode ? QStringLiteral("code") : QStringLiteral("''");
        const QString descSel = def->hasDescription ? QStringLiteral("description") : QStringLiteral("''");
        QSqlQuery prevQ(Database::instance().connection());
        prevQ.prepare(QStringLiteral("SELECT %1, name, %2 FROM %3 WHERE id = :id LIMIT 1;").arg(codeSel, descSel, tbl));
        prevQ.bindValue(QStringLiteral(":id"), m_editingRecordId);
        if (prevQ.exec() && prevQ.next()) {
            if (def->hasCode) oldJson.insert(QStringLiteral("code"), prevQ.value(0).toString());
            oldJson.insert(QStringLiteral("name"), prevQ.value(1).toString());
            if (def->hasDescription) oldJson.insert(QStringLiteral("description"), prevQ.value(2).toString());
        }
    }

    QSqlQuery query(Database::instance().connection());
    if (def->isRolesTable) {
        if (isCreate) {
            query.prepare(QStringLiteral("INSERT INTO roles (name, description, is_system, is_active) VALUES (:name, :desc, 0, 1);"));
        } else {
            query.prepare(QStringLiteral("UPDATE roles SET name = :name, description = :desc WHERE id = :id AND is_system = 0;"));
            query.bindValue(QStringLiteral(":id"), m_editingRecordId);
        }
        query.bindValue(QStringLiteral(":name"), name);
        query.bindValue(QStringLiteral(":desc"), desc);
    } else if (def->hasCode) {
        if (isCreate) {
            query.prepare(QStringLiteral("INSERT INTO %1 (code, name, is_active) VALUES (:code, :name, 1);").arg(tbl));
        } else {
            query.prepare(QStringLiteral("UPDATE %1 SET code = :code, name = :name WHERE id = :id;").arg(tbl));
            query.bindValue(QStringLiteral(":id"), m_editingRecordId);
        }
        query.bindValue(QStringLiteral(":code"), code);
        query.bindValue(QStringLiteral(":name"), name);
    } else {
        if (isCreate) {
            query.prepare(QStringLiteral("INSERT INTO %1 (name, is_active) VALUES (:name, 1);").arg(tbl));
        } else {
            query.prepare(QStringLiteral("UPDATE %1 SET name = :name WHERE id = :id;").arg(tbl));
            query.bindValue(QStringLiteral(":id"), m_editingRecordId);
        }
        query.bindValue(QStringLiteral(":name"), name);
    }

    if (!query.exec()) {
        QMessageBox::warning(
            this, QStringLiteral("Error al guardar"),
            QStringLiteral("No se pudo guardar en \"%1\" (verifica duplicados):\n%2")
                .arg(def->displayName, query.lastError().text()));
        return;
    }

    const int finalId = isCreate ? query.lastInsertId().toInt() : m_editingRecordId;

    QJsonObject newJson;
    if (def->hasCode) newJson.insert(QStringLiteral("code"), code);
    newJson.insert(QStringLiteral("name"), name);
    if (def->hasDescription) newJson.insert(QStringLiteral("description"), desc);

    AuditEvent ev;
    ev.moduleId = QStringLiteral("panel_control");
    ev.targetTable = def->tableName;
    ev.recordId = QString::number(finalId);
    ev.action = isCreate ? QStringLiteral("CREATE") : QStringLiteral("UPDATE");
    ev.oldValuesJson = isCreate ? QString() : compactJson(oldJson);
    ev.newValuesJson = compactJson(newJson);
    ev.details = QStringLiteral("Tabla %1 · Registro: %2").arg(def->displayName, name);
    Database::instance().logAuditEvent(ev);

    onResetForm();
    loadCurrentStandardTableRecords();
}

void SupportTablesWidget::onResetForm()
{
    m_editingRecordId = -1;
    m_codeEdit->clear();
    m_nameEdit->clear();
    m_descEdit->clear();
    m_saveBtn->setText(QStringLiteral("Agregar registro"));
    m_saveBtn->setIcon(Icons::get(Icons::Name::Add, QColor(QStringLiteral("#FFFFFF")), 15));
}

void SupportTablesWidget::onEditRecord(int recordId, const QString &code, const QString &name, const QString &description)
{
    m_editingRecordId = recordId;
    m_codeEdit->setText(code);
    m_nameEdit->setText(name);
    m_descEdit->setText(description);
    m_saveBtn->setText(QStringLiteral("Guardar cambios"));
    m_saveBtn->setIcon(Icons::get(Icons::Name::Save, QColor(QStringLiteral("#FFFFFF")), 15));
    m_nameEdit->setFocus();
}

void SupportTablesWidget::onDeprecateRecord(int recordId, const QString &displayName, bool isSystem)
{
    const SupportTableDef *def = currentStandardDef();
    if (!def) return;

    if (isSystem) {
        QMessageBox::warning(this, QStringLiteral("Registro protegido"),
                             QStringLiteral("Este registro pertenece al núcleo del sistema y no puede deprecarse."));
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Deprecar registro - %1").arg(displayName));
    dlg.setModal(true);
    dlg.setMinimumWidth(440);

    auto *layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    auto *warn = new QLabel(
        QStringLiteral("<b>Baja lógica (Deprecado):</b> El registro <b>\"%1\"</b> dejará de ofrecerse en altas nuevas, "
                       "pero se conservará intacto en todos los informes y auditorías históricas.")
            .arg(displayName.toHtmlEscaped()),
        &dlg);
    warn->setWordWrap(true);
    warn->setStyleSheet(
        QStringLiteral("background-color: #FFF3CD; color: #664D03; border: 1px solid #FFECB5; "
                       "padding: 9px; border-radius: 6px;"));
    layout->addWidget(warn);

    auto *form = new QFormLayout();
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);

    auto *replCombo = new QComboBox(&dlg);
    replCombo->addItem(QStringLiteral("(Sin reemplazo sugerido)"), -1);

    QSqlQuery candQ(Database::instance().connection());
    candQ.prepare(QStringLiteral(
        "SELECT id, name FROM %1 WHERE is_deprecated = 0 AND id <> :id ORDER BY name ASC;")
                      .arg(quoteIdent(def->tableName)));
    candQ.bindValue(QStringLiteral(":id"), recordId);
    if (candQ.exec()) {
        while (candQ.next()) {
            replCombo->addItem(candQ.value(1).toString(), candQ.value(0).toInt());
        }
    }

    auto *noteEdit = new QLineEdit(&dlg);
    noteEdit->setPlaceholderText(QStringLiteral("Ej: Reemplazado por nueva normativa fiscal / medio vigente"));

    form->addRow(QStringLiteral("Aconsejar reemplazo por:"), replCombo);
    form->addRow(QStringLiteral("Motivo / Nota:"), noteEdit);
    layout->addLayout(form);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto *cancelBtn = new QPushButton(QStringLiteral("Cancelar"), &dlg);
    cancelBtn->setIcon(Icons::get(Icons::Name::Close, QColor(QStringLiteral("#1E3A5F")), 15));
    cancelBtn->setProperty("secondary", true);

    auto *confirmBtn = new QPushButton(QStringLiteral("Confirmar deprecado"), &dlg);
    confirmBtn->setIcon(Icons::get(Icons::Name::Lock, QColor(QStringLiteral("#FFFFFF")), 15));
    confirmBtn->setProperty("danger", true);

    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(confirmBtn);
    layout->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(confirmBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() != QDialog::Accepted) return;

    const int replId = replCombo->currentData().toInt();
    const QString note = noteEdit->text().trimmed();
    const QString who  = Session::instance().currentUser().email;
    const QString when = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));

    QSqlQuery updQ(Database::instance().connection());
    updQ.prepare(QStringLiteral(
        "UPDATE %1 SET is_deprecated = 1, is_active = 0, deprecated_at = :at, "
        "  deprecated_by = :by, replacement_id = :rid, deprecation_note = :note "
        "WHERE id = :id;").arg(quoteIdent(def->tableName)));
    updQ.bindValue(QStringLiteral(":at"),   when);
    updQ.bindValue(QStringLiteral(":by"),   who);
    updQ.bindValue(QStringLiteral(":rid"),  replId > 0 ? QVariant(replId) : QVariant());
    updQ.bindValue(QStringLiteral(":note"), note);
    updQ.bindValue(QStringLiteral(":id"),   recordId);

    if (!updQ.exec()) {
        QMessageBox::warning(this, QStringLiteral("Error"), updQ.lastError().text());
        return;
    }

    QJsonObject newJson;
    newJson.insert(QStringLiteral("is_deprecated"), true);
    newJson.insert(QStringLiteral("replacement_id"), replId);
    newJson.insert(QStringLiteral("deprecation_note"), note);

    AuditEvent ev;
    ev.moduleId = QStringLiteral("panel_control");
    ev.targetTable = def->tableName;
    ev.recordId = QString::number(recordId);
    ev.action = QStringLiteral("DEPRECATE");
    ev.oldValuesJson = QStringLiteral("{\"is_deprecated\":false}");
    ev.newValuesJson = compactJson(newJson);
    ev.details = QStringLiteral("Deprecado '%1' en %2%3")
                     .arg(displayName, def->displayName,
                          replId > 0 ? QStringLiteral(" · Reemplazo: %1").arg(replCombo->currentText()) : QString());
    Database::instance().logAuditEvent(ev);

    if (m_editingRecordId == recordId) onResetForm();
    loadCurrentStandardTableRecords();
}

void SupportTablesWidget::onReactivateRecord(int recordId, const QString &displayName)
{
    const SupportTableDef *def = currentStandardDef();
    if (!def) return;

    const auto reply = QMessageBox::question(
        this, QStringLiteral("Reactivar registro"),
        QStringLiteral("¿Deseas reactivar \"%1\" en \"%2\" para que vuelva a estar disponible en nuevas cargas?")
            .arg(displayName, def->displayName),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (reply != QMessageBox::Yes) return;

    QSqlQuery updQ(Database::instance().connection());
    updQ.prepare(QStringLiteral(
        "UPDATE %1 SET is_deprecated = 0, is_active = 1, deprecated_at = '', "
        "  deprecated_by = '', replacement_id = NULL, deprecation_note = '' "
        "WHERE id = :id;").arg(quoteIdent(def->tableName)));
    updQ.bindValue(QStringLiteral(":id"), recordId);

    if (!updQ.exec()) {
        QMessageBox::warning(this, QStringLiteral("Error"), updQ.lastError().text());
        return;
    }

    AuditEvent ev;
    ev.moduleId = QStringLiteral("panel_control");
    ev.targetTable = def->tableName;
    ev.recordId = QString::number(recordId);
    ev.action = QStringLiteral("REACTIVATE");
    ev.oldValuesJson = QStringLiteral("{\"is_deprecated\":true}");
    ev.newValuesJson = QStringLiteral("{\"is_deprecated\":false}");
    ev.details = QStringLiteral("Reactivado '%1' en %2").arg(displayName, def->displayName);
    Database::instance().logAuditEvent(ev);

    loadCurrentStandardTableRecords();
}

void SupportTablesWidget::onAddExternalSource()
{
    ExternalSourceDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        m_selectedIsExternal = true;
        m_selectedIndexOrSourceId = dlg.savedSourceId();
        reloadAll();
        m_rightStack->setCurrentIndex(2);
    }
}

void SupportTablesWidget::onEditCurrentExternalSource()
{
    if (!m_selectedIsExternal || m_selectedIndexOrSourceId <= 0) return;

    ExternalSourceDialog dlg(this, m_selectedIndexOrSourceId);
    if (dlg.exec() == QDialog::Accepted) {
        reloadAll();
    }
}

void SupportTablesWidget::onSyncCurrentExternalSource()
{
    if (!m_selectedIsExternal || m_selectedIndexOrSourceId <= 0) return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    int synced = 0;
    QString err;
    const bool ok = ExternalSourceManager::syncSource(m_selectedIndexOrSourceId, &synced, &err);
    QApplication::restoreOverrideCursor();

    if (ok) {
        loadCurrentExternalSourceItems();
        QMessageBox::information(
            this, QStringLiteral("Sincronización completada"),
            QStringLiteral("Se actualizaron %1 registro(s) desde la fuente externa.").arg(synced));
    } else {
        QMessageBox::warning(this, QStringLiteral("Error de sincronización"), err);
    }
}

void SupportTablesWidget::onToggleCurrentExternalSourceActive()
{
    if (!m_selectedIsExternal || m_selectedIndexOrSourceId <= 0) return;

    ExternalSourceDef src;
    if (!ExternalSourceManager::loadSourceById(m_selectedIndexOrSourceId, &src)) return;

    QString err;
    if (!ExternalSourceManager::setSourceActive(src.id, !src.isActive, &err)) {
        QMessageBox::warning(this, QStringLiteral("Error"), err);
        return;
    }
    reloadAll();
}

void SupportTablesWidget::onConfigureRolePermissions(int roleId, const QString &roleName, bool isSystem)
{
    if (isSystem) {
        QMessageBox::information(
            this, QStringLiteral("Rol Administrador"),
            QStringLiteral("El rol \"%1\" posee acceso completo a todos los módulos y secciones del Panel de Control.")
                .arg(roleName));
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Permisos predeterminados - %1").arg(roleName));
    dlg.setMinimumSize(460, 400);

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

    if (dlg.exec() != QDialog::Accepted) return;

    QSqlDatabase db = Database::instance().connection();
    db.transaction();

    QSqlQuery delQ(db);
    delQ.prepare(QStringLiteral("DELETE FROM role_permissions WHERE role_id = :rid;"));
    delQ.bindValue(QStringLiteral(":rid"), roleId);
    bool ok = delQ.exec();

    if (ok) {
        QSqlQuery insQ(db);
        insQ.prepare(QStringLiteral("INSERT INTO role_permissions (role_id, permission_id) VALUES (:rid, :pid);"));
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
        AuditEvent ev;
        ev.moduleId = QStringLiteral("panel_control");
        ev.targetTable = QStringLiteral("role_permissions");
        ev.recordId = QString::number(roleId);
        ev.action = QStringLiteral("UPDATE");
        ev.details = QStringLiteral("Permisos actualizados para rol: %1").arg(roleName);
        Database::instance().logAuditEvent(ev);

        QMessageBox::information(this, QStringLiteral("Permisos guardados"),
                                 QStringLiteral("Se actualizaron los permisos del rol \"%1\".").arg(roleName));
    } else {
        db.rollback();
        QMessageBox::warning(this, QStringLiteral("Error"),
                             QStringLiteral("No se pudieron guardar los permisos del rol."));
    }
}
