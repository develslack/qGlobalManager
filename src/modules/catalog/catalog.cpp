#include "catalog.h"
#include "core/db.h"
#include "core/externalSourceManager.h"
#include "core/icons.h"
#include "core/reportpayload.h"
#include "core/reportviewerdialog.h"
#include "core/session.h"

#include <QAbstractItemView>
#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSplitter>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QVariant>

namespace {

constexpr int kVerticalBreakpointPx = 900;

QString compactJson(const QJsonObject &obj)
{
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

} // namespace

CatalogModule::CatalogModule(QWidget *parent)
    : ModuleBase(parent)
{
    initBanner(
        moduleTitle(),
        isClinicMode()
            ? QStringLiteral("Catálogo de prestaciones, honorarios, insumos con stock y precios multimoneda.")
            : QStringLiteral("Catálogo de artículos con control de stock, servicios y precios indexados a divisas."));

    setupUi();
    applyBusinessModeLabels();
    loadExternalSourceCombos();
    reloadTable();
}

bool CatalogModule::isClinicMode() const
{
    const QString bType = Database::instance().getSetting(
        QStringLiteral("company.business_type"),
        QStringLiteral("Comercio"));
    return bType.contains(QStringLiteral("Consultorio"), Qt::CaseInsensitive);
}

QString CatalogModule::moduleTitle() const
{
    return isClinicMode()
        ? QStringLiteral("Prestaciones y Servicios")
        : QStringLiteral("Artículos y Servicios");
}

QString CatalogModule::currencySymbol() const
{
    return Database::instance().getSetting(
        QStringLiteral("regional.currency_symbol"),
        QStringLiteral("$"));
}

QString CatalogModule::formatMoney(double amount) const
{
    return QStringLiteral("%1 %2").arg(currencySymbol(), QString::number(amount, 'f', 2));
}

void CatalogModule::setupUi()
{
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);

    // =========================================================================
    // Panel izquierdo: Formulario de Alta / Edición
    // =========================================================================
    auto *formScroll = new QScrollArea(m_splitter);
    formScroll->setWidgetResizable(true);
    formScroll->setFrameShape(QFrame::NoFrame);
    formScroll->setMinimumWidth(330);
    formScroll->setMaximumWidth(450);

    auto *formPage = new QWidget(formScroll);
    auto *formOuter = new QVBoxLayout(formPage);
    formOuter->setContentsMargins(0, 0, 6, 0);
    formOuter->setSpacing(10);

    m_formGroup = new QGroupBox(QStringLiteral("Alta / Edición"), formPage);
    auto *formGroupLayout = new QVBoxLayout(m_formGroup);
    formGroupLayout->setSpacing(10);

    auto *form = new QFormLayout();
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_typeCombo = new QComboBox(m_formGroup);
    m_typeCombo->addItem(QStringLiteral("Artículo / Insumo (con control de stock)"), QStringLiteral("PRODUCT"));
    m_typeCombo->addItem(QStringLiteral("Servicio / Prestación (sin stock)"), QStringLiteral("SERVICE"));

    m_skuLabel = new QLabel(QStringLiteral("Código / SKU:"), m_formGroup);
    m_skuEdit = new QLineEdit(m_formGroup);

    m_nameLabel = new QLabel(QStringLiteral("Nombre *:"), m_formGroup);
    m_nameEdit = new QLineEdit(m_formGroup);

    m_categoryEdit = new QLineEdit(m_formGroup);
    m_categoryEdit->setPlaceholderText(QStringLiteral("Ej: Alimentos, Repuestos, Consultas, Prácticas..."));

    m_unitCombo = new QComboBox(m_formGroup);
    m_unitCombo->setEditable(true);
    m_unitCombo->addItems(QStringList{
        QStringLiteral("Unid."),
        QStringLiteral("Sesión"),
        QStringLiteral("Consulta"),
        QStringLiteral("Hora"),
        QStringLiteral("Kg"),
        QStringLiteral("Lt"),
        QStringLiteral("Caja"),
        QStringLiteral("Pack")
    });

    // Esquema de precios (Local o Indexado a Fuente Externa)
    m_pricingModeCombo = new QComboBox(m_formGroup);
    m_pricingModeCombo->addItem(QStringLiteral("Precio fijo en moneda local"), QStringLiteral("LOCAL"));
    m_pricingModeCombo->addItem(QStringLiteral("Indexado a fuente externa (divisa/ref.)"), QStringLiteral("EXTERNAL"));

    m_costLabel = new QLabel(QStringLiteral("Precio de Costo:"), m_formGroup);
    m_costSpin = new QDoubleSpinBox(m_formGroup);
    m_costSpin->setRange(0.0, 999999999.99);
    m_costSpin->setDecimals(2);

    m_salePriceLabel = new QLabel(QStringLiteral("Precio de Venta *:"), m_formGroup);
    m_salePriceSpin = new QDoubleSpinBox(m_formGroup);
    m_salePriceSpin->setRange(0.0, 999999999.99);
    m_salePriceSpin->setDecimals(2);

    m_extSourceLabel = new QLabel(QStringLiteral("Fuente externa:"), m_formGroup);
    m_extSourceCombo = new QComboBox(m_formGroup);

    m_extItemLabel = new QLabel(QStringLiteral("Referencia / Divisa:"), m_formGroup);
    m_extItemCombo = new QComboBox(m_formGroup);

    m_foreignPriceLabel = new QLabel(QStringLiteral("Precio en ref. / divisa *:"), m_formGroup);
    m_foreignPriceSpin = new QDoubleSpinBox(m_formGroup);
    m_foreignPriceSpin->setRange(0.0, 999999999.99);
    m_foreignPriceSpin->setDecimals(2);

    m_computedPriceLabel = new QLabel(m_formGroup);
    m_computedPriceLabel->setWordWrap(true);
    m_computedPriceLabel->setStyleSheet(
        QStringLiteral("background-color: #EEF4FA; color: #1E3A5F; border: 1px solid #D9E2EC; "
                       "padding: 6px 8px; border-radius: 5px; font-weight: 600;"));
    m_computedPriceLabel->hide();

    m_vatSpin = new QDoubleSpinBox(m_formGroup);
    m_vatSpin->setRange(0.0, 100.0);
    m_vatSpin->setDecimals(2);
    m_vatSpin->setValue(21.0);
    m_vatSpin->setSuffix(QStringLiteral(" %"));

    // Stock (solo para PRODUCT)
    m_stockCurrentLabel = new QLabel(QStringLiteral("Stock Inicial / Actual:"), m_formGroup);
    m_stockCurrentSpin = new QDoubleSpinBox(m_formGroup);
    m_stockCurrentSpin->setRange(-999999.0, 9999999.0);
    m_stockCurrentSpin->setDecimals(2);

    m_stockMinLabel = new QLabel(QStringLiteral("Stock Mínimo (Alerta):"), m_formGroup);
    m_stockMinSpin = new QDoubleSpinBox(m_formGroup);
    m_stockMinSpin->setRange(0.0, 9999999.0);
    m_stockMinSpin->setDecimals(2);

    m_notesEdit = new QPlainTextEdit(m_formGroup);
    m_notesEdit->setMaximumHeight(68);
    m_notesEdit->setPlaceholderText(QStringLiteral("Descripción extendida, marca, proveedor o indicaciones..."));

    form->addRow(QStringLiteral("Naturaleza *:"), m_typeCombo);
    form->addRow(m_skuLabel, m_skuEdit);
    form->addRow(m_nameLabel, m_nameEdit);
    form->addRow(QStringLiteral("Categoría / Rubro:"), m_categoryEdit);
    form->addRow(QStringLiteral("Unidad:"), m_unitCombo);
    form->addRow(QStringLiteral("Esquema de Precio:"), m_pricingModeCombo);
    form->addRow(m_costLabel, m_costSpin);
    form->addRow(m_salePriceLabel, m_salePriceSpin);
    form->addRow(m_extSourceLabel, m_extSourceCombo);
    form->addRow(m_extItemLabel, m_extItemCombo);
    form->addRow(m_foreignPriceLabel, m_foreignPriceSpin);
    form->addRow(QString(), m_computedPriceLabel);
    form->addRow(QStringLiteral("Alícuota IVA:"), m_vatSpin);
    form->addRow(m_stockCurrentLabel, m_stockCurrentSpin);
    form->addRow(m_stockMinLabel, m_stockMinSpin);
    form->addRow(QStringLiteral("Observaciones:"), m_notesEdit);

    formGroupLayout->addLayout(form);

    auto *formBtnRow = new QHBoxLayout();
    m_saveBtn = new QPushButton(QStringLiteral("Agregar al catálogo"), m_formGroup);
    m_saveBtn->setIcon(Icons::get(Icons::Name::Add, QColor(QStringLiteral("#FFFFFF")), 15));

    m_clearBtn = new QPushButton(QStringLiteral("Limpiar"), m_formGroup);
    m_clearBtn->setIcon(Icons::get(Icons::Name::Refresh, QColor(QStringLiteral("#1E3A5F")), 15));
    m_clearBtn->setProperty("secondary", true);

    formBtnRow->addWidget(m_saveBtn);
    formBtnRow->addWidget(m_clearBtn);
    formBtnRow->addStretch();
    formGroupLayout->addLayout(formBtnRow);

    formOuter->addWidget(m_formGroup);
    formOuter->addStretch();
    formScroll->setWidget(formPage);
    m_splitter->addWidget(formScroll);

    // =========================================================================
    // Panel derecho: Buscador, Filtros, Generar Informe y Grilla
    // =========================================================================
    m_listGroup = new QGroupBox(QStringLiteral("Catálogo Vigente"), m_splitter);
    auto *listLayout = new QVBoxLayout(m_listGroup);
    listLayout->setSpacing(10);

    auto *topToolbar = new QHBoxLayout();
    topToolbar->setSpacing(8);

    m_searchEdit = new QLineEdit(m_listGroup);
    m_searchEdit->setPlaceholderText(QStringLiteral("Buscar por código, nombre o categoría..."));
    m_searchEdit->setClearButtonEnabled(true);

    m_filterTypeCombo = new QComboBox(m_listGroup);
    m_filterTypeCombo->addItem(QStringLiteral("Todos los tipos"), QStringLiteral("ALL"));
    m_filterTypeCombo->addItem(QStringLiteral("Solo Artículos (Stock)"), QStringLiteral("PRODUCT"));
    m_filterTypeCombo->addItem(QStringLiteral("Solo Servicios / Prestaciones"), QStringLiteral("SERVICE"));

    m_reportListBtn = new QPushButton(QStringLiteral("Generar Informe"), m_listGroup);
    m_reportListBtn->setIcon(Icons::get(Icons::Name::Printer, QColor(QStringLiteral("#FFFFFF")), 15));

    auto *refreshBtn = new QPushButton(QStringLiteral("Actualizar"), m_listGroup);
    refreshBtn->setIcon(Icons::get(Icons::Name::Refresh, QColor(QStringLiteral("#1E3A5F")), 15));
    refreshBtn->setProperty("secondary", true);

    topToolbar->addWidget(m_searchEdit, 1);
    topToolbar->addWidget(m_filterTypeCombo);
    topToolbar->addWidget(m_reportListBtn);
    topToolbar->addWidget(refreshBtn);
    listLayout->addLayout(topToolbar);

    auto *subToolbar = new QHBoxLayout();
    m_onlyLowStockCheck = new QCheckBox(QStringLiteral("Solo con alerta de stock bajo"), m_listGroup);
    m_showDeprecatedCheck = new QCheckBox(QStringLiteral("Mostrar deprecados"), m_listGroup);
    m_summaryLabel = new QLabel(m_listGroup);
    m_summaryLabel->setStyleSheet(QStringLiteral("color: #52606D; font-size: 9pt;"));

    subToolbar->addWidget(m_onlyLowStockCheck);
    subToolbar->addSpacing(12);
    subToolbar->addWidget(m_showDeprecatedCheck);
    subToolbar->addStretch();
    subToolbar->addWidget(m_summaryLabel);
    listLayout->addLayout(subToolbar);

    m_table = new QTableWidget(m_listGroup);
    m_table->setColumnCount(8);
    m_table->setHorizontalHeaderLabels(QStringList{
        QStringLiteral("Código"),
        QStringLiteral("Descripción"),
        QStringLiteral("Tipo / Categoría"),
        QStringLiteral("Precio Vigente"),
        QStringLiteral("Referencia"),
        QStringLiteral("Stock"),
        QStringLiteral("Estado"),
        QStringLiteral("Acciones")
    });
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(42);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    listLayout->addWidget(m_table, 1);

    m_splitter->addWidget(m_listGroup);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);

    bodyLayout()->addWidget(m_splitter, 1);

    // Conexiones
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CatalogModule::onItemTypeChanged);
    connect(m_pricingModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CatalogModule::onPricingModeChanged);
    connect(m_extSourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CatalogModule::onExternalSourceChanged);
    connect(m_extItemCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CatalogModule::onExternalItemOrPriceChanged);
    connect(m_foreignPriceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &CatalogModule::onExternalItemOrPriceChanged);

    connect(m_saveBtn, &QPushButton::clicked, this, &CatalogModule::onSaveClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &CatalogModule::onResetForm);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &CatalogModule::onFilterChanged);
    connect(m_filterTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CatalogModule::onFilterChanged);
    connect(m_onlyLowStockCheck, &QCheckBox::toggled, this, &CatalogModule::onFilterChanged);
    connect(m_showDeprecatedCheck, &QCheckBox::toggled, this, &CatalogModule::onFilterChanged);
    connect(m_reportListBtn, &QPushButton::clicked, this, &CatalogModule::onGenerateListReport);
    connect(refreshBtn, &QPushButton::clicked, this, [this]() {
        loadExternalSourceCombos();
        reloadTable();
    });

    onItemTypeChanged(m_typeCombo->currentIndex());
    onPricingModeChanged(m_pricingModeCombo->currentIndex());
}

void CatalogModule::resizeEvent(QResizeEvent *event)
{
    ModuleBase::resizeEvent(event);
    const Qt::Orientation target =
        (event->size().width() < kVerticalBreakpointPx) ? Qt::Vertical : Qt::Horizontal;
    if (m_splitter->orientation() != target) {
        m_splitter->setOrientation(target);
    }
}

void CatalogModule::applyBusinessModeLabels()
{
    const bool clinic = isClinicMode();
    const QString sym = currencySymbol();

    m_costSpin->setPrefix(sym + QLatin1Char(' '));
    m_salePriceSpin->setPrefix(sym + QLatin1Char(' '));

    m_formGroup->setTitle(clinic
                              ? QStringLiteral("Alta / Edición de Prestación o Insumo")
                              : QStringLiteral("Alta / Edición de Artículo o Servicio"));
    m_listGroup->setTitle(moduleTitle());

    m_skuLabel->setText(clinic ? QStringLiteral("Código / Nomenclador:") : QStringLiteral("Código / SKU / Barras:"));
    m_skuEdit->setPlaceholderText(clinic ? QStringLiteral("Ej: 01.01, CONS-01") : QStringLiteral("Ej: ART-001 o código de barras"));

    m_nameLabel->setText(clinic ? QStringLiteral("Prestación / Insumo *:") : QStringLiteral("Nombre del Artículo / Servicio *:"));
    m_nameEdit->setPlaceholderText(clinic ? QStringLiteral("Ej: Consulta General, Radiografía, Insumo...") : QStringLiteral("Ej: Alimento Perro 15Kg, Filtro de Aceite..."));

    m_salePriceLabel->setText(clinic ? QStringLiteral("Honorario / Arancel *:") : QStringLiteral("Precio de Venta *:"));

    if (m_editingItemId < 0) {
        const int defaultIdx = m_typeCombo->findData(clinic ? QStringLiteral("SERVICE") : QStringLiteral("PRODUCT"));
        if (defaultIdx >= 0) {
            m_typeCombo->setCurrentIndex(defaultIdx);
        }
    }
}

void CatalogModule::onActivated()
{
    applyBusinessModeLabels();
    loadExternalSourceCombos();
    reloadTable();

    AuditEvent ev;
    ev.moduleId = moduleId();
    ev.targetTable = QStringLiteral("catalog_items");
    ev.action = QStringLiteral("VIEW");
    ev.details = QStringLiteral("Acceso al módulo %1").arg(moduleTitle());
    Database::instance().logAuditEvent(ev);
}

void CatalogModule::reloadExternalRatesCache()
{
    m_extSourceNames.clear();
    m_extRatesCache.clear();

    const QVector<ExternalSourceDef> sources = ExternalSourceManager::loadSources(true);
    for (const ExternalSourceDef &src : sources) {
        m_extSourceNames.insert(src.id, src.name);
        const QVector<ExternalSourceItem> items = ExternalSourceManager::loadItems(src.id);
        for (const ExternalSourceItem &it : items) {
            if (it.hasNumericValue && it.valueNum > 0.0) {
                m_extRatesCache.insert(qMakePair(src.id, it.itemKey), qMakePair(it.itemLabel, it.valueNum));
            }
        }
    }
}

void CatalogModule::loadExternalSourceCombos()
{
    reloadExternalRatesCache();

    const int prevSourceId = m_extSourceCombo->currentData().toInt();
    const QString prevKey  = m_extItemCombo->currentData().toString();

    m_extSourceCombo->blockSignals(true);
    m_extSourceCombo->clear();
    m_extSourceCombo->addItem(QStringLiteral("(Seleccionar fuente externa)"), -1);

    const QVector<ExternalSourceDef> activeSources = ExternalSourceManager::loadSources(false);
    for (const ExternalSourceDef &src : activeSources) {
        m_extSourceCombo->addItem(src.name, src.id);
    }

    const int idx = m_extSourceCombo->findData(prevSourceId);
    m_extSourceCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    m_extSourceCombo->blockSignals(false);

    reloadExternalItemsCombo(m_extSourceCombo->currentData().toInt(), prevKey);
}

void CatalogModule::reloadExternalItemsCombo(int sourceId, const QString &selectKey)
{
    m_extItemCombo->blockSignals(true);
    m_extItemCombo->clear();

    if (sourceId > 0) {
        const QVector<ExternalSourceItem> items = ExternalSourceManager::loadItems(sourceId);
        for (const ExternalSourceItem &it : items) {
            if (!it.hasNumericValue || it.valueNum <= 0.0) continue;
            const QString label = QStringLiteral("%1 (%2)")
                                      .arg(it.itemLabel, formatMoney(it.valueNum));
            m_extItemCombo->addItem(label, it.itemKey);
            m_extItemCombo->setItemData(m_extItemCombo->count() - 1, it.valueNum, Qt::UserRole + 1);
        }
    }

    if (m_extItemCombo->count() == 0) {
        m_extItemCombo->addItem(QStringLiteral("(Sin valores numéricos sincronizados)"), QString());
    } else if (!selectKey.isEmpty()) {
        const int idx = m_extItemCombo->findData(selectKey);
        if (idx >= 0) m_extItemCombo->setCurrentIndex(idx);
    }

    m_extItemCombo->blockSignals(false);
    onExternalItemOrPriceChanged();
}

void CatalogModule::onItemTypeChanged(int index)
{
    const bool isProduct = (m_typeCombo->itemData(index).toString() == QStringLiteral("PRODUCT"));
    m_stockCurrentLabel->setVisible(isProduct);
    m_stockCurrentSpin->setVisible(isProduct);
    m_stockMinLabel->setVisible(isProduct);
    m_stockMinSpin->setVisible(isProduct);
    m_stockCurrentSpin->setEnabled(m_editingItemId < 0);
    m_stockCurrentLabel->setText(
        m_editingItemId < 0
            ? QStringLiteral("Stock Inicial:")
            : QStringLiteral("Stock Actual (usar botón Stock):"));
}

void CatalogModule::onPricingModeChanged(int index)
{
    const bool isExternal = (m_pricingModeCombo->itemData(index).toString() == QStringLiteral("EXTERNAL"));
    m_salePriceLabel->setVisible(!isExternal);
    m_salePriceSpin->setVisible(!isExternal);

    m_extSourceLabel->setVisible(isExternal);
    m_extSourceCombo->setVisible(isExternal);
    m_extItemLabel->setVisible(isExternal);
    m_extItemCombo->setVisible(isExternal);
    m_foreignPriceLabel->setVisible(isExternal);
    m_foreignPriceSpin->setVisible(isExternal);
    m_computedPriceLabel->setVisible(isExternal);

    onExternalItemOrPriceChanged();
}

void CatalogModule::onExternalSourceChanged(int index)
{
    const int sourceId = m_extSourceCombo->itemData(index).toInt();
    reloadExternalItemsCombo(sourceId);
}

void CatalogModule::onExternalItemOrPriceChanged()
{
    if (m_pricingModeCombo->currentData().toString() != QStringLiteral("EXTERNAL")) {
        return;
    }

    const QVariant rateVar = m_extItemCombo->currentData(Qt::UserRole + 1);
    const double rate = rateVar.isValid() ? rateVar.toDouble() : 0.0;
    const double fPrice = m_foreignPriceSpin->value();

    if (rate > 0.0) {
        const double computed = fPrice * rate;
        m_computedPriceLabel->setText(
            QStringLiteral("Cotización actual: %1  →  Precio vigente estimado: %2")
                .arg(formatMoney(rate), formatMoney(computed)));
    } else {
        m_computedPriceLabel->setText(
            QStringLiteral("Selecciona una fuente externa y una referencia numérica sincronizada."));
    }
}

void CatalogModule::onFilterChanged()
{
    reloadTable();
}

void CatalogModule::reloadTable()
{
    reloadExternalRatesCache();
    m_table->setRowCount(0);
    m_visibleRows.clear();

    const QString search     = m_searchEdit->text().trimmed();
    const QString typeFilter = m_filterTypeCombo->currentData().toString();
    const bool onlyLowStock  = m_onlyLowStockCheck->isChecked();
    const bool showDep       = m_showDeprecatedCheck->isChecked();

    QString sql = QStringLiteral(
        "SELECT id, sku, name, item_type, category, unit, "
        "       cost_price, sale_price, pricing_mode, ext_source_id, ext_item_key, "
        "       foreign_price, vat_rate, stock_current, stock_min, notes, "
        "       is_deprecated, deprecation_note, created_at "
        "FROM catalog_items WHERE 1 = 1 ");

    if (!showDep) {
        sql += QStringLiteral("AND is_deprecated = 0 ");
    }
    if (typeFilter != QStringLiteral("ALL")) {
        sql += QStringLiteral("AND item_type = :itype ");
    }
    if (onlyLowStock) {
        sql += QStringLiteral("AND item_type = 'PRODUCT' AND stock_current <= stock_min ");
    }
    if (!search.isEmpty()) {
        sql += QStringLiteral(
            "AND (LOWER(sku) LIKE :q OR LOWER(name) LIKE :q OR LOWER(category) LIKE :q) ");
    }

    sql += QStringLiteral("ORDER BY is_deprecated ASC, name ASC;");

    QSqlQuery q(Database::instance().connection());
    q.prepare(sql);
    if (typeFilter != QStringLiteral("ALL")) {
        q.bindValue(QStringLiteral(":itype"), typeFilter);
    }
    if (!search.isEmpty()) {
        q.bindValue(QStringLiteral(":q"), QStringLiteral("%%1%").arg(search.toLower()));
    }

    if (!q.exec()) return;

    int activeCount = 0;
    int lowStockCount = 0;
    int rowIdx = 0;

    while (q.next()) {
        CatalogRow r;
        r.id              = q.value(0).toInt();
        r.sku             = q.value(1).toString();
        r.name            = q.value(2).toString();
        r.itemType        = q.value(3).toString();
        r.category        = q.value(4).toString();
        r.unit            = q.value(5).toString();
        r.costPrice       = q.value(6).toDouble();
        r.salePrice       = q.value(7).toDouble();
        r.pricingMode     = q.value(8).toString();
        r.extSourceId     = q.value(9).isNull() ? -1 : q.value(9).toInt();
        r.extItemKey      = q.value(10).toString();
        r.foreignPrice    = q.value(11).toDouble();
        r.vatRate         = q.value(12).toDouble();
        r.stockCurrent    = q.value(13).toDouble();
        r.stockMin        = q.value(14).toDouble();
        r.notes           = q.value(15).toString();
        r.isDeprecated    = (q.value(16).toInt() == 1);
        r.deprecationNote = q.value(17).toString();
        r.createdAt       = q.value(18).toString();

        if (r.pricingMode == QStringLiteral("EXTERNAL") && r.extSourceId > 0) {
            r.extSourceName = m_extSourceNames.value(r.extSourceId);
            const auto keyPair = qMakePair(r.extSourceId, r.extItemKey);
            if (m_extRatesCache.contains(keyPair)) {
                r.extItemLabel = m_extRatesCache.value(keyPair).first;
                r.extRate      = m_extRatesCache.value(keyPair).second;
                r.hasExtRate   = true;
            }
        }

        const bool isProduct = (r.itemType == QStringLiteral("PRODUCT"));
        const bool isLowStock = isProduct && !r.isDeprecated && (r.stockCurrent <= r.stockMin);

        if (!r.isDeprecated) ++activeCount;
        if (isLowStock) ++lowStockCount;
        m_visibleRows.append(r);

        const QString typeTag = isProduct ? QStringLiteral("Artículo") : QStringLiteral("Servicio");
        const QString typeCat = r.category.isEmpty()
            ? typeTag
            : QStringLiteral("%1 · %2").arg(typeTag, r.category);

        QString refText = QStringLiteral("Moneda local");
        if (r.pricingMode == QStringLiteral("EXTERNAL")) {
            const QString refLabel = r.extItemLabel.isEmpty() ? r.extItemKey : r.extItemLabel;
            refText = QStringLiteral("%1 × %2 (%3)")
                          .arg(QString::number(r.foreignPrice, 'f', 2),
                               refLabel,
                               r.hasExtRate ? formatMoney(r.extRate) : QStringLiteral("Sin cotiz."));
        }

        QString stockText = QStringLiteral("N/A (Servicio)");
        if (isProduct) {
            stockText = QStringLiteral("%1 %2 (mín: %3)")
                            .arg(QString::number(r.stockCurrent, 'g', 6),
                                 r.unit.isEmpty() ? QStringLiteral("Unid.") : r.unit,
                                 QString::number(r.stockMin, 'g', 6));
        }

        m_table->insertRow(rowIdx);
        m_table->setItem(rowIdx, 0, new QTableWidgetItem(r.sku.isEmpty() ? QStringLiteral("-") : r.sku));
        m_table->setItem(rowIdx, 1, new QTableWidgetItem(r.isDeprecated ? r.name + QStringLiteral(" [Deprecado]") : r.name));
        m_table->setItem(rowIdx, 2, new QTableWidgetItem(typeCat));

        auto *priceItem = new QTableWidgetItem(formatMoney(r.effectiveLocalPrice()));
        priceItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(rowIdx, 3, priceItem);

        m_table->setItem(rowIdx, 4, new QTableWidgetItem(refText));

        auto *stockItem = new QTableWidgetItem(stockText);
        if (isLowStock) {
            stockItem->setForeground(QBrush(QColor(QStringLiteral("#C53030"))));
            stockItem->setText(stockText + QStringLiteral(" !"));
        } else if (isProduct) {
            stockItem->setForeground(QBrush(QColor(QStringLiteral("#2E7D32"))));
        } else {
            stockItem->setForeground(QBrush(QColor(QStringLiteral("#52606D"))));
        }
        m_table->setItem(rowIdx, 5, stockItem);

        auto *statusItem = new QTableWidgetItem(r.isDeprecated ? QStringLiteral("Deprecado") : QStringLiteral("Activo"));
        statusItem->setForeground(QBrush(QColor(r.isDeprecated ? QStringLiteral("#B7791F") : QStringLiteral("#2E7D32"))));
        m_table->setItem(rowIdx, 6, statusItem);

        if (r.isDeprecated) {
            const QBrush depBg(QColor(QStringLiteral("#FFFBEB")));
            const QBrush depFg(QColor(QStringLiteral("#744210")));
            for (int c = 0; c < 7; ++c) {
                if (QTableWidgetItem *it = m_table->item(rowIdx, c)) {
                    it->setBackground(depBg);
                    if (c != 6) it->setForeground(depFg);
                }
            }
        } else if (isLowStock) {
            const QBrush alertBg(QColor(QStringLiteral("#FFF5F5")));
            for (int c = 0; c < 7; ++c) {
                if (QTableWidgetItem *it = m_table->item(rowIdx, c)) {
                    it->setBackground(alertBg);
                }
            }
        }

        // Botonera por fila
        auto *actWidget = new QWidget(m_table);
        auto *actLayout = new QHBoxLayout(actWidget);
        actLayout->setContentsMargins(4, 3, 4, 3);
        actLayout->setSpacing(5);

        auto *editBtn = new QPushButton(QStringLiteral("Editar"), actWidget);
        editBtn->setIcon(Icons::get(Icons::Name::Edit, QColor(QStringLiteral("#1E3A5F")), 14));
        editBtn->setProperty("secondary", true);
        actLayout->addWidget(editBtn);

        const int idCopy = r.id;
        const QString nameCopy = r.name;

        if (isProduct && !r.isDeprecated) {
            auto *stockBtn = new QPushButton(QStringLiteral("Stock"), actWidget);
            stockBtn->setIcon(Icons::get(Icons::Name::Table, QColor(QStringLiteral("#1E3A5F")), 14));
            stockBtn->setProperty("secondary", true);
            connect(stockBtn, &QPushButton::clicked, this, [this, idCopy]() {
                onAdjustStock(idCopy);
            });
            actLayout->addWidget(stockBtn);
        }

        auto *repBtn = new QPushButton(QStringLiteral("Informe"), actWidget);
        repBtn->setIcon(Icons::get(Icons::Name::Printer, QColor(QStringLiteral("#FFFFFF")), 14));
        actLayout->addWidget(repBtn);

        if (!r.isDeprecated) {
            auto *depBtn = new QPushButton(QStringLiteral("Deprecar"), actWidget);
            depBtn->setIcon(Icons::get(Icons::Name::Lock, QColor(QStringLiteral("#FFFFFF")), 14));
            depBtn->setProperty("danger", true);
            connect(depBtn, &QPushButton::clicked, this, [this, idCopy, nameCopy]() {
                onDeprecateItem(idCopy, nameCopy);
            });
            actLayout->addWidget(depBtn);
        } else {
            auto *reactBtn = new QPushButton(QStringLiteral("Reactivar"), actWidget);
            reactBtn->setIcon(Icons::get(Icons::Name::Unlock, QColor(QStringLiteral("#1E3A5F")), 14));
            reactBtn->setProperty("secondary", true);
            connect(reactBtn, &QPushButton::clicked, this, [this, idCopy, nameCopy]() {
                onReactivateItem(idCopy, nameCopy);
            });
            actLayout->addWidget(reactBtn);
        }

        connect(editBtn, &QPushButton::clicked, this, [this, idCopy]() {
            onEditItem(idCopy);
        });
        connect(repBtn, &QPushButton::clicked, this, [this, idCopy]() {
            onGenerateSingleReport(idCopy);
        });

        m_table->setCellWidget(rowIdx, 7, actWidget);
        ++rowIdx;
    }

    m_table->resizeColumnToContents(7);
    m_summaryLabel->setText(
        QStringLiteral("Mostrando %1 ítem(s) · Activos: %2 · Alertas de stock bajo: %3")
            .arg(m_visibleRows.size())
            .arg(activeCount)
            .arg(lowStockCount));
}

bool CatalogModule::loadItemById(int itemId, CatalogRow *out) const
{
    if (itemId <= 0 || !out) return false;

    QSqlQuery q(Database::instance().connection());
    q.prepare(QStringLiteral(
        "SELECT id, sku, name, item_type, category, unit, "
        "       cost_price, sale_price, pricing_mode, ext_source_id, ext_item_key, "
        "       foreign_price, vat_rate, stock_current, stock_min, notes, "
        "       is_deprecated, deprecation_note, created_at "
        "FROM catalog_items WHERE id = :id LIMIT 1;"));
    q.bindValue(QStringLiteral(":id"), itemId);

    if (!q.exec() || !q.next()) return false;

    out->id              = q.value(0).toInt();
    out->sku             = q.value(1).toString();
    out->name            = q.value(2).toString();
    out->itemType        = q.value(3).toString();
    out->category        = q.value(4).toString();
    out->unit            = q.value(5).toString();
    out->costPrice       = q.value(6).toDouble();
    out->salePrice       = q.value(7).toDouble();
    out->pricingMode     = q.value(8).toString();
    out->extSourceId     = q.value(9).isNull() ? -1 : q.value(9).toInt();
    out->extItemKey      = q.value(10).toString();
    out->foreignPrice    = q.value(11).toDouble();
    out->vatRate         = q.value(12).toDouble();
    out->stockCurrent    = q.value(13).toDouble();
    out->stockMin        = q.value(14).toDouble();
    out->notes           = q.value(15).toString();
    out->isDeprecated    = (q.value(16).toInt() == 1);
    out->deprecationNote = q.value(17).toString();
    out->createdAt       = q.value(18).toString();

    if (out->pricingMode == QStringLiteral("EXTERNAL") && out->extSourceId > 0) {
        out->extSourceName = m_extSourceNames.value(out->extSourceId);
        const auto keyPair = qMakePair(out->extSourceId, out->extItemKey);
        if (m_extRatesCache.contains(keyPair)) {
            out->extItemLabel = m_extRatesCache.value(keyPair).first;
            out->extRate      = m_extRatesCache.value(keyPair).second;
            out->hasExtRate   = true;
        }
    }
    return true;
}

void CatalogModule::onSaveClicked()
{
    const bool isCreate = (m_editingItemId < 0);
    if (!requirePermission(
            isCreate ? QStringLiteral("create") : QStringLiteral("edit"),
            isCreate ? QStringLiteral("dar de alta ítems en el catálogo")
                     : QStringLiteral("editar ítems del catálogo"))) {
        return;
    }

    const QString itemType    = m_typeCombo->currentData().toString();
    const QString sku         = m_skuEdit->text().trimmed().toUpper();
    const QString name        = m_nameEdit->text().trimmed();
    const QString category    = m_categoryEdit->text().trimmed();
    const QString unit        = m_unitCombo->currentText().trimmed().isEmpty()
                                    ? QStringLiteral("Unid.")
                                    : m_unitCombo->currentText().trimmed();
    const QString pricingMode = m_pricingModeCombo->currentData().toString();
    const double costPrice    = m_costSpin->value();
    double salePrice          = m_salePriceSpin->value();
    int extSourceId           = -1;
    QString extItemKey;
    double foreignPrice       = 0.0;
    const double vatRate      = m_vatSpin->value();
    const double stockCurrent = (itemType == QStringLiteral("PRODUCT")) ? m_stockCurrentSpin->value() : 0.0;
    const double stockMin     = (itemType == QStringLiteral("PRODUCT")) ? m_stockMinSpin->value() : 0.0;
    const QString notes       = m_notesEdit->toPlainText().trimmed();

    if (name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Dato requerido"),
                             QStringLiteral("Ingresa el nombre del artículo o prestación."));
        m_nameEdit->setFocus();
        return;
    }

    if (pricingMode == QStringLiteral("EXTERNAL")) {
        extSourceId  = m_extSourceCombo->currentData().toInt();
        extItemKey   = m_extItemCombo->currentData().toString();
        foreignPrice = m_foreignPriceSpin->value();

        if (extSourceId <= 0 || extItemKey.isEmpty()) {
            QMessageBox::warning(
                this,
                QStringLiteral("Fuente externa requerida"),
                QStringLiteral("Selecciona una fuente externa y una cotización/referencia numérica sincronizada."));
            return;
        }

        const double rate = m_extItemCombo->currentData(Qt::UserRole + 1).toDouble();
        if (rate > 0.0) {
            salePrice = foreignPrice * rate;
        }
    }

    // Validar código SKU duplicado si se especificó
    if (!sku.isEmpty()) {
        QSqlQuery dupQ(Database::instance().connection());
        dupQ.prepare(QStringLiteral(
            "SELECT id, name FROM catalog_items WHERE sku = :sku AND id <> :id LIMIT 1;"));
        dupQ.bindValue(QStringLiteral(":sku"), sku);
        dupQ.bindValue(QStringLiteral(":id"), m_editingItemId);
        if (dupQ.exec() && dupQ.next()) {
            QMessageBox::warning(
                this,
                QStringLiteral("Código duplicado"),
                QStringLiteral("El código \"%1\" ya pertenece a:\n%2").arg(sku, dupQ.value(1).toString()));
            m_skuEdit->setFocus();
            return;
        }
    }

    QJsonObject oldJson;
    if (!isCreate) {
        CatalogRow prev;
        if (loadItemById(m_editingItemId, &prev)) {
            oldJson.insert(QStringLiteral("sku"), prev.sku);
            oldJson.insert(QStringLiteral("name"), prev.name);
            oldJson.insert(QStringLiteral("sale_price"), prev.salePrice);
            oldJson.insert(QStringLiteral("pricing_mode"), prev.pricingMode);
        }
    }

    QSqlQuery q(Database::instance().connection());
    if (isCreate) {
        q.prepare(QStringLiteral(
            "INSERT INTO catalog_items ("
            "  sku, name, item_type, category, unit, cost_price, sale_price, "
            "  pricing_mode, ext_source_id, ext_item_key, foreign_price, "
            "  vat_rate, stock_current, stock_min, notes, is_active, is_deprecated"
            ") VALUES ("
            "  :sku, :name, :itype, :cat, :unit, :cost, :sale, "
            "  :pmode, :esid, :ekey, :fprice, :vat, :scur, :smin, :notes, 1, 0"
            ");"));
        q.bindValue(QStringLiteral(":scur"), stockCurrent);
    } else {
        q.prepare(QStringLiteral(
            "UPDATE catalog_items SET "
            "  sku = :sku, name = :name, item_type = :itype, category = :cat, unit = :unit, "
            "  cost_price = :cost, sale_price = :sale, pricing_mode = :pmode, "
            "  ext_source_id = :esid, ext_item_key = :ekey, foreign_price = :fprice, "
            "  vat_rate = :vat, stock_min = :smin, notes = :notes, updated_at = CURRENT_TIMESTAMP "
            "WHERE id = :id;"));
        q.bindValue(QStringLiteral(":id"), m_editingItemId);
    }

    q.bindValue(QStringLiteral(":sku"),    sku);
    q.bindValue(QStringLiteral(":name"),   name);
    q.bindValue(QStringLiteral(":itype"),  itemType);
    q.bindValue(QStringLiteral(":cat"),    category);
    q.bindValue(QStringLiteral(":unit"),   unit);
    q.bindValue(QStringLiteral(":cost"),   costPrice);
    q.bindValue(QStringLiteral(":sale"),   salePrice);
    q.bindValue(QStringLiteral(":pmode"),  pricingMode);
    q.bindValue(QStringLiteral(":esid"),   extSourceId > 0 ? QVariant(extSourceId) : QVariant());
    q.bindValue(QStringLiteral(":ekey"),   extItemKey);
    q.bindValue(QStringLiteral(":fprice"), foreignPrice);
    q.bindValue(QStringLiteral(":vat"),    vatRate);
    q.bindValue(QStringLiteral(":smin"),   stockMin);
    q.bindValue(QStringLiteral(":notes"),  notes);

    if (!q.exec()) {
        QMessageBox::warning(this, QStringLiteral("Error al guardar"),
                             QStringLiteral("No se pudo guardar el ítem:\n%1").arg(q.lastError().text()));
        return;
    }

    const int finalId = isCreate ? q.lastInsertId().toInt() : m_editingItemId;

    // Registrar movimiento de stock inicial si es alta de artículo con stock > 0
    if (isCreate && itemType == QStringLiteral("PRODUCT") && stockCurrent != 0.0) {
        QSqlQuery mvQ(Database::instance().connection());
        mvQ.prepare(QStringLiteral(
            "INSERT INTO stock_movements (item_id, movement_type, quantity, previous_stock, new_stock, reason, user_email) "
            "VALUES (:iid, 'ADJUST', :qty, 0, :nstock, :reason, :uemail);"));
        mvQ.bindValue(QStringLiteral(":iid"),    finalId);
        mvQ.bindValue(QStringLiteral(":qty"),    stockCurrent);
        mvQ.bindValue(QStringLiteral(":nstock"), stockCurrent);
        mvQ.bindValue(QStringLiteral(":reason"), QStringLiteral("Stock inicial de alta"));
        mvQ.bindValue(QStringLiteral(":uemail"), Session::instance().currentUser().email);
        mvQ.exec();
    }

    QJsonObject newJson;
    newJson.insert(QStringLiteral("sku"), sku);
    newJson.insert(QStringLiteral("name"), name);
    newJson.insert(QStringLiteral("item_type"), itemType);
    newJson.insert(QStringLiteral("sale_price"), salePrice);
    newJson.insert(QStringLiteral("pricing_mode"), pricingMode);

    AuditEvent ev;
    ev.moduleId = moduleId();
    ev.targetTable = QStringLiteral("catalog_items");
    ev.recordId = QString::number(finalId);
    ev.action = isCreate ? QStringLiteral("CREATE") : QStringLiteral("UPDATE");
    ev.oldValuesJson = isCreate ? QString() : compactJson(oldJson);
    ev.newValuesJson = compactJson(newJson);
    ev.details = QStringLiteral("%1: %2 (%3)").arg(itemType, name, formatMoney(salePrice));
    Database::instance().logAuditEvent(ev);

    onResetForm();
    reloadTable();
}

void CatalogModule::onResetForm()
{
    m_editingItemId = -1;
    m_skuEdit->clear();
    m_nameEdit->clear();
    m_categoryEdit->clear();
    m_unitCombo->setCurrentIndex(0);
    m_pricingModeCombo->setCurrentIndex(0);
    m_costSpin->setValue(0.0);
    m_salePriceSpin->setValue(0.0);
    m_foreignPriceSpin->setValue(0.0);
    m_vatSpin->setValue(21.0);
    m_stockCurrentSpin->setValue(0.0);
    m_stockMinSpin->setValue(0.0);
    m_notesEdit->clear();

    const int defaultTypeIdx = m_typeCombo->findData(isClinicMode() ? QStringLiteral("SERVICE") : QStringLiteral("PRODUCT"));
    if (defaultTypeIdx >= 0) m_typeCombo->setCurrentIndex(defaultTypeIdx);

    m_saveBtn->setText(QStringLiteral("Agregar al catálogo"));
    m_saveBtn->setIcon(Icons::get(Icons::Name::Add, QColor(QStringLiteral("#FFFFFF")), 15));
    onItemTypeChanged(m_typeCombo->currentIndex());
    onPricingModeChanged(m_pricingModeCombo->currentIndex());
}

void CatalogModule::onEditItem(int itemId)
{
    if (!requirePermission(QStringLiteral("edit"), QStringLiteral("editar ítems del catálogo"))) {
        return;
    }

    CatalogRow r;
    if (!loadItemById(itemId, &r)) return;

    loadExternalSourceCombos();

    m_editingItemId = r.id;

    const int tIdx = m_typeCombo->findData(r.itemType);
    if (tIdx >= 0) m_typeCombo->setCurrentIndex(tIdx);

    m_skuEdit->setText(r.sku);
    m_nameEdit->setText(r.name);
    m_categoryEdit->setText(r.category);
    m_unitCombo->setCurrentText(r.unit);
    m_costSpin->setValue(r.costPrice);
    m_salePriceSpin->setValue(r.salePrice);

    const int pIdx = m_pricingModeCombo->findData(r.pricingMode);
    m_pricingModeCombo->setCurrentIndex(pIdx >= 0 ? pIdx : 0);

    if (r.pricingMode == QStringLiteral("EXTERNAL")) {
        const int sIdx = m_extSourceCombo->findData(r.extSourceId);
        if (sIdx >= 0) {
            m_extSourceCombo->setCurrentIndex(sIdx);
        }
        reloadExternalItemsCombo(r.extSourceId, r.extItemKey);
        m_foreignPriceSpin->setValue(r.foreignPrice);
    }

    m_vatSpin->setValue(r.vatRate);
    m_stockCurrentSpin->setValue(r.stockCurrent);
    m_stockMinSpin->setValue(r.stockMin);
    m_notesEdit->setPlainText(r.notes);

    m_saveBtn->setText(QStringLiteral("Guardar cambios"));
    m_saveBtn->setIcon(Icons::get(Icons::Name::Save, QColor(QStringLiteral("#FFFFFF")), 15));
    onItemTypeChanged(m_typeCombo->currentIndex());
    onPricingModeChanged(m_pricingModeCombo->currentIndex());
    m_nameEdit->setFocus();
}

void CatalogModule::onAdjustStock(int itemId)
{
    if (!requirePermission(QStringLiteral("adjust_stock"), QStringLiteral("ajustar stock de inventario"))) {
        return;
    }

    CatalogRow r;
    if (!loadItemById(itemId, &r) || r.itemType != QStringLiteral("PRODUCT")) {
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Movimiento de Stock - %1").arg(r.name));
    dlg.setModal(true);
    dlg.setMinimumWidth(420);

    auto *layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    auto *info = new QLabel(
        QStringLiteral("<b>%1</b> (%2)<br>Stock actual: <b>%3 %4</b> · Stock mínimo: <b>%5</b>")
            .arg(r.name.toHtmlEscaped(),
                 r.sku.isEmpty() ? QStringLiteral("Sin SKU") : r.sku.toHtmlEscaped(),
                 QString::number(r.stockCurrent, 'g', 6),
                 r.unit.toHtmlEscaped(),
                 QString::number(r.stockMin, 'g', 6)),
        &dlg);
    info->setWordWrap(true);
    info->setStyleSheet(
        QStringLiteral("background-color: #EEF4FA; color: #1E3A5F; border: 1px solid #D9E2EC; "
                       "padding: 9px; border-radius: 6px;"));
    layout->addWidget(info);

    auto *form = new QFormLayout();
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);

    auto *opCombo = new QComboBox(&dlg);
    opCombo->addItem(QStringLiteral("Ingreso de mercadería (+)"), QStringLiteral("IN"));
    opCombo->addItem(QStringLiteral("Egreso / Baja de stock (-)"), QStringLiteral("OUT"));
    opCombo->addItem(QStringLiteral("Fijar stock físico exacto (=)"), QStringLiteral("ADJUST"));

    auto *qtySpin = new QDoubleSpinBox(&dlg);
    qtySpin->setRange(0.0, 9999999.0);
    qtySpin->setDecimals(2);
    qtySpin->setValue(1.0);

    auto *reasonEdit = new QLineEdit(&dlg);
    reasonEdit->setPlaceholderText(QStringLiteral("Ej: Compra a proveedor, conteo inventario, rotura..."));

    form->addRow(QStringLiteral("Tipo de movimiento:"), opCombo);
    form->addRow(QStringLiteral("Cantidad:"), qtySpin);
    form->addRow(QStringLiteral("Motivo *:"), reasonEdit);
    layout->addLayout(form);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto *cancelBtn = new QPushButton(QStringLiteral("Cancelar"), &dlg);
    cancelBtn->setIcon(Icons::get(Icons::Name::Close, QColor(QStringLiteral("#1E3A5F")), 15));
    cancelBtn->setProperty("secondary", true);

    auto *okBtn = new QPushButton(QStringLiteral("Aplicar movimiento"), &dlg);
    okBtn->setIcon(Icons::get(Icons::Name::Save, QColor(QStringLiteral("#FFFFFF")), 15));
    okBtn->setDefault(true);

    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    layout->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, &dlg, [&]() {
        if (reasonEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dlg, QStringLiteral("Motivo requerido"),
                                 QStringLiteral("Indica brevemente el motivo del movimiento de stock."));
            reasonEdit->setFocus();
            return;
        }
        dlg.accept();
    });

    if (dlg.exec() != QDialog::Accepted) return;

    const QString mvType = opCombo->currentData().toString();
    const double qty     = qtySpin->value();
    const QString reason = reasonEdit->text().trimmed();
    const double prev    = r.stockCurrent;
    double next          = prev;

    if (mvType == QStringLiteral("IN")) {
        next = prev + qty;
    } else if (mvType == QStringLiteral("OUT")) {
        next = prev - qty;
    } else {
        next = qty;
    }

    QSqlDatabase db = Database::instance().connection();
    db.transaction();

    QSqlQuery updQ(db);
    updQ.prepare(QStringLiteral(
        "UPDATE catalog_items SET stock_current = :nstock, updated_at = CURRENT_TIMESTAMP WHERE id = :id;"));
    updQ.bindValue(QStringLiteral(":nstock"), next);
    updQ.bindValue(QStringLiteral(":id"), r.id);

    QSqlQuery insQ(db);
    insQ.prepare(QStringLiteral(
        "INSERT INTO stock_movements (item_id, movement_type, quantity, previous_stock, new_stock, reason, user_email) "
        "VALUES (:iid, :mtype, :qty, :pstock, :nstock, :reason, :uemail);"));
    insQ.bindValue(QStringLiteral(":iid"),    r.id);
    insQ.bindValue(QStringLiteral(":mtype"),  mvType);
    insQ.bindValue(QStringLiteral(":qty"),    qty);
    insQ.bindValue(QStringLiteral(":pstock"), prev);
    insQ.bindValue(QStringLiteral(":nstock"), next);
    insQ.bindValue(QStringLiteral(":reason"), reason);
    insQ.bindValue(QStringLiteral(":uemail"), Session::instance().currentUser().email);

    if (!updQ.exec() || !insQ.exec() || !db.commit()) {
        db.rollback();
        QMessageBox::critical(this, QStringLiteral("Error de stock"),
                              QStringLiteral("No se pudo registrar el movimiento de inventario."));
        return;
    }

    AuditEvent ev;
    ev.moduleId = moduleId();
    ev.targetTable = QStringLiteral("stock_movements");
    ev.recordId = QString::number(r.id);
    ev.action = QStringLiteral("STOCK_%1").arg(mvType);
    ev.oldValuesJson = QStringLiteral("{\"stock\":%1}").arg(prev);
    ev.newValuesJson = QStringLiteral("{\"stock\":%1}").arg(next);
    ev.details = QStringLiteral("%1 | %2 -> %3 | Motivo: %4").arg(r.name).arg(prev).arg(next).arg(reason);
    Database::instance().logAuditEvent(ev);

    reloadTable();
}

void CatalogModule::onDeprecateItem(int itemId, const QString &displayName)
{
    if (!requirePermission(QStringLiteral("deprecate"), QStringLiteral("deprecar ítems del catálogo"))) {
        return;
    }

    bool okInput = false;
    const QString note = QInputDialog::getText(
        this,
        QStringLiteral("Deprecar ítem"),
        QStringLiteral("\"%1\" se marcará como Deprecado (se conservará en ventas e informes históricos).\n"
                       "Motivo breve (opcional):").arg(displayName),
        QLineEdit::Normal,
        QString(),
        &okInput).trimmed();

    if (!okInput) return;

    QSqlQuery q(Database::instance().connection());
    q.prepare(QStringLiteral(
        "UPDATE catalog_items SET is_deprecated = 1, is_active = 0, "
        "  deprecated_at = :at, deprecated_by = :by, deprecation_note = :note, "
        "  updated_at = CURRENT_TIMESTAMP "
        "WHERE id = :id;"));
    q.bindValue(QStringLiteral(":at"),   QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    q.bindValue(QStringLiteral(":by"),   Session::instance().currentUser().email);
    q.bindValue(QStringLiteral(":note"), note);
    q.bindValue(QStringLiteral(":id"),   itemId);

    if (!q.exec()) {
        QMessageBox::warning(this, QStringLiteral("Error"), q.lastError().text());
        return;
    }

    AuditEvent ev;
    ev.moduleId = moduleId();
    ev.targetTable = QStringLiteral("catalog_items");
    ev.recordId = QString::number(itemId);
    ev.action = QStringLiteral("DEPRECATE");
    ev.details = QStringLiteral("Deprecado '%1'%2").arg(displayName, note.isEmpty() ? QString() : QStringLiteral(" · %1").arg(note));
    Database::instance().logAuditEvent(ev);

    if (m_editingItemId == itemId) onResetForm();
    reloadTable();
}

void CatalogModule::onReactivateItem(int itemId, const QString &displayName)
{
    if (!requirePermission(QStringLiteral("deprecate"), QStringLiteral("reactivar ítems del catálogo"))) {
        return;
    }

    const auto ans = QMessageBox::question(
        this,
        QStringLiteral("Reactivar ítem"),
        QStringLiteral("¿Deseas reactivar \"%1\" para volver a ofrecerlo normalmente?").arg(displayName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);

    if (ans != QMessageBox::Yes) return;

    QSqlQuery q(Database::instance().connection());
    q.prepare(QStringLiteral(
        "UPDATE catalog_items SET is_deprecated = 0, is_active = 1, "
        "  deprecated_at = '', deprecated_by = '', deprecation_note = '', "
        "  updated_at = CURRENT_TIMESTAMP "
        "WHERE id = :id;"));
    q.bindValue(QStringLiteral(":id"), itemId);

    if (!q.exec()) {
        QMessageBox::warning(this, QStringLiteral("Error"), q.lastError().text());
        return;
    }

    AuditEvent ev;
    ev.moduleId = moduleId();
    ev.targetTable = QStringLiteral("catalog_items");
    ev.recordId = QString::number(itemId);
    ev.action = QStringLiteral("REACTIVATE");
    ev.details = QStringLiteral("Reactivado '%1'").arg(displayName);
    Database::instance().logAuditEvent(ev);

    reloadTable();
}

void CatalogModule::onGenerateListReport()
{
    if (!requirePermission(QStringLiteral("report"), QStringLiteral("generar informes del catálogo"))) {
        return;
    }

    if (m_visibleRows.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Sin registros"),
                                 QStringLiteral("No hay ítems visibles con el filtro actual."));
        return;
    }

    const bool onlyLow = m_onlyLowStockCheck->isChecked();

    Reporting::ReportPayload payload;
    payload.reportTitle = onlyLow
        ? QStringLiteral("Alerta de Reposición de Stock")
        : QStringLiteral("Lista de Precios y %1").arg(moduleTitle());
    payload.reportSubtitle = QStringLiteral("Emitido el %1")
                                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yyyy HH:mm")));
    payload.generatedBy = Session::instance().currentUser().fullName;

    payload.metadata.append({QStringLiteral("Filtro Tipo"), m_filterTypeCombo->currentText()});
    payload.metadata.append({QStringLiteral("Ítems incluidos"), QString::number(m_visibleRows.size())});

    Reporting::TableSection sec;
    sec.title = QStringLiteral("Detalle del Catálogo");
    sec.columns = {
        {QStringLiteral("Código"), QStringLiteral("C"), 0},
        {QStringLiteral("Descripción"), QStringLiteral("L"), 0},
        {QStringLiteral("Tipo / Categoría"), QStringLiteral("L"), 0},
        {QStringLiteral("Precio Vigente"), QStringLiteral("R"), 0},
        {QStringLiteral("Referencia"), QStringLiteral("L"), 0},
        {QStringLiteral("Stock"), QStringLiteral("C"), 0}
    };

    double stockValuation = 0.0;
    int lowStockCount = 0;

    for (const CatalogRow &r : m_visibleRows) {
        const bool isProd = (r.itemType == QStringLiteral("PRODUCT"));
        const double price = r.effectiveLocalPrice();
        if (isProd && !r.isDeprecated) {
            stockValuation += (r.stockCurrent * price);
            if (r.stockCurrent <= r.stockMin) ++lowStockCount;
        }

        const QString refStr = (r.pricingMode == QStringLiteral("EXTERNAL"))
            ? QStringLiteral("%1 × %2").arg(QString::number(r.foreignPrice, 'f', 2),
                                            r.extItemLabel.isEmpty() ? r.extItemKey : r.extItemLabel)
            : QStringLiteral("Fijo local");

        const QString stockStr = isProd
            ? QStringLiteral("%1 %2").arg(QString::number(r.stockCurrent, 'g', 6), r.unit)
            : QStringLiteral("Servicio");

        sec.rows.append(QStringList{
            r.sku.isEmpty() ? QStringLiteral("-") : r.sku.toHtmlEscaped(),
            r.name.toHtmlEscaped(),
            (isProd ? QStringLiteral("Artículo") : QStringLiteral("Servicio"))
                + (r.category.isEmpty() ? QString() : QStringLiteral(" · %1").arg(r.category.toHtmlEscaped())),
            formatMoney(price),
            refStr.toHtmlEscaped(),
            stockStr.toHtmlEscaped()
        });
    }

    payload.tables.append(sec);
    payload.totals.append({QStringLiteral("Total de ítems listados"), QString::number(m_visibleRows.size())});
    payload.totals.append({QStringLiteral("Ítems con alerta de stock bajo"), QString::number(lowStockCount)});
    if (stockValuation > 0.0) {
        payload.totals.append({QStringLiteral("Valuación estimada del stock (precio venta)"), formatMoney(stockValuation)});
    }
    payload.footerNotes = QStringLiteral("Los precios indexados a fuentes externas reflejan la última cotización sincronizada.");

    AuditEvent ev;
    ev.moduleId = moduleId();
    ev.targetTable = QStringLiteral("catalog_items");
    ev.action = QStringLiteral("REPORT");
    ev.details = QStringLiteral("Informe de catálogo generado (%1 ítems)").arg(m_visibleRows.size());
    Database::instance().logAuditEvent(ev);

    ReportViewerDialog::showReport(payload, this);
}

void CatalogModule::onGenerateSingleReport(int itemId)
{
    if (!requirePermission(QStringLiteral("report"), QStringLiteral("generar ficha del ítem"))) {
        return;
    }

    CatalogRow r;
    if (!loadItemById(itemId, &r)) return;

    const bool isProd = (r.itemType == QStringLiteral("PRODUCT"));

    Reporting::ReportPayload payload;
    payload.reportTitle = QStringLiteral("Ficha de %1").arg(isProd ? QStringLiteral("Artículo") : QStringLiteral("Servicio / Prestación"));
    payload.reportSubtitle = QStringLiteral("%1 · Código: %2")
                                 .arg(r.name, r.sku.isEmpty() ? QStringLiteral("Sin SKU") : r.sku);
    payload.generatedBy = Session::instance().currentUser().fullName;

    Reporting::TableSection infoSec;
    infoSec.title = QStringLiteral("Especificaciones y Precio");
    infoSec.columns = {
        {QStringLiteral("Campo"), QStringLiteral("L"), 35},
        {QStringLiteral("Valor"), QStringLiteral("L"), 65}
    };
    infoSec.rows.append(QStringList{QStringLiteral("Nombre"), r.name.toHtmlEscaped()});
    infoSec.rows.append(QStringList{QStringLiteral("Código / SKU"), r.sku.isEmpty() ? QStringLiteral("-") : r.sku.toHtmlEscaped()});
    infoSec.rows.append(QStringList{QStringLiteral("Naturaleza"), isProd ? QStringLiteral("Artículo con stock") : QStringLiteral("Servicio / Prestación")});
    infoSec.rows.append(QStringList{QStringLiteral("Categoría"), r.category.isEmpty() ? QStringLiteral("-") : r.category.toHtmlEscaped()});
    infoSec.rows.append(QStringList{QStringLiteral("Unidad"), r.unit.toHtmlEscaped()});
    infoSec.rows.append(QStringList{QStringLiteral("Precio de Costo"), formatMoney(r.costPrice)});
    infoSec.rows.append(QStringList{QStringLiteral("Precio de Venta Vigente"), formatMoney(r.effectiveLocalPrice())});
    if (r.pricingMode == QStringLiteral("EXTERNAL")) {
        infoSec.rows.append(QStringList{
            QStringLiteral("Referencia Externa"),
            QStringLiteral("%1 (%2) × %3").arg(r.extSourceName, r.extItemLabel.isEmpty() ? r.extItemKey : r.extItemLabel)
                .arg(QString::number(r.foreignPrice, 'f', 2)).toHtmlEscaped()
        });
    }
    infoSec.rows.append(QStringList{QStringLiteral("Alícuota IVA"), QStringLiteral("%1 %").arg(QString::number(r.vatRate, 'f', 2))});
    if (isProd) {
        infoSec.rows.append(QStringList{QStringLiteral("Stock Actual"), QStringLiteral("%1 %2").arg(QString::number(r.stockCurrent, 'g', 6), r.unit).toHtmlEscaped()});
        infoSec.rows.append(QStringList{QStringLiteral("Stock Mínimo"), QStringLiteral("%1 %2").arg(QString::number(r.stockMin, 'g', 6), r.unit).toHtmlEscaped()});
    }
    if (!r.notes.isEmpty()) {
        infoSec.rows.append(QStringList{QStringLiteral("Observaciones"), r.notes.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br/>"))});
    }
    payload.tables.append(infoSec);

    // Si es producto, anexar los últimos 30 movimientos de stock
    if (isProd) {
        Reporting::TableSection mvSec;
        mvSec.title = QStringLiteral("Últimos Movimientos de Stock");
        mvSec.columns = {
            {QStringLiteral("Fecha (UTC)"), QStringLiteral("C"), 0},
            {QStringLiteral("Movimiento"), QStringLiteral("C"), 0},
            {QStringLiteral("Cantidad"), QStringLiteral("R"), 0},
            {QStringLiteral("Anterior → Nuevo"), QStringLiteral("C"), 0},
            {QStringLiteral("Motivo / Operador"), QStringLiteral("L"), 0}
        };

        QSqlQuery mvQ(Database::instance().connection());
        mvQ.prepare(QStringLiteral(
            "SELECT created_at, movement_type, quantity, previous_stock, new_stock, reason, user_email "
            "FROM stock_movements WHERE item_id = :iid ORDER BY id DESC LIMIT 30;"));
        mvQ.bindValue(QStringLiteral(":iid"), r.id);
        if (mvQ.exec()) {
            while (mvQ.next()) {
                mvSec.rows.append(QStringList{
                    mvQ.value(0).toString().toHtmlEscaped(),
                    mvQ.value(1).toString().toHtmlEscaped(),
                    QString::number(mvQ.value(2).toDouble(), 'g', 6),
                    QStringLiteral("%1 → %2")
                        .arg(QString::number(mvQ.value(3).toDouble(), 'g', 6),
                             QString::number(mvQ.value(4).toDouble(), 'g', 6)),
                    QStringLiteral("%1 (%2)").arg(mvQ.value(5).toString(), mvQ.value(6).toString()).toHtmlEscaped()
                });
            }
        }
        if (!mvSec.rows.isEmpty()) {
            payload.tables.append(mvSec);
        }
    }

    payload.totals.append({QStringLiteral("Precio Vigente"), formatMoney(r.effectiveLocalPrice())});
    ReportViewerDialog::showReport(payload, this);
}
