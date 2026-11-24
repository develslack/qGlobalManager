#include "cashbox.h"
#include "core/db.h"
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
#include <QHostInfo>
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
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QVariant>

namespace {

constexpr int kVerticalBreakpointPx = 880;

QString compactJson(const QJsonObject &obj)
{
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

QFrame *makeKpiCard(const QString &title, QLabel **valueLabelOut, const QString &accentHex, QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setStyleSheet(
        QStringLiteral("QFrame { background-color: #FFFFFF; border: 1px solid #D9E2EC; "
                       "border-left: 4px solid %1; border-radius: 6px; }").arg(accentHex));

    auto *lay = new QVBoxLayout(card);
    lay->setContentsMargins(10, 6, 10, 6);
    lay->setSpacing(2);

    auto *titleLbl = new QLabel(title, card);
    titleLbl->setStyleSheet(QStringLiteral("color: #52606D; font-size: 8.5pt; border: none; background: transparent;"));

    auto *valLbl = new QLabel(QStringLiteral("$ 0.00"), card);
    valLbl->setStyleSheet(QStringLiteral("color: #1F2933; font-size: 12pt; font-weight: 700; border: none; background: transparent;"));

    lay->addWidget(titleLbl);
    lay->addWidget(valLbl);

    if (valueLabelOut) {
        *valueLabelOut = valLbl;
    }
    return card;
}

} // namespace

CashboxModule::CashboxModule(QWidget *parent)
    : ModuleBase(parent)
{
    initBanner(
        QStringLiteral("Caja y Movimientos de Fondos"),
        QStringLiteral("Apertura de turno, registro de ingresos/egresos por medio de pago, arqueo y cierre auditable."));

    setupUi();
    loadSupportCombos();
    reloadAll();
}

bool CashboxModule::isClinicMode() const
{
    const QString bType = Database::instance().getSetting(
        QStringLiteral("company.business_type"),
        QStringLiteral("Comercio"));
    return bType.contains(QStringLiteral("Consultorio"), Qt::CaseInsensitive);
}

QString CashboxModule::currencySymbol() const
{
    return Database::instance().getSetting(
        QStringLiteral("regional.currency_symbol"),
        QStringLiteral("$"));
}

QString CashboxModule::formatMoney(double amount) const
{
    return QStringLiteral("%1 %2").arg(currencySymbol(), QString::number(amount, 'f', 2));
}

bool CashboxModule::isCashMethodName(const QString &methodName)
{
    const QString low = methodName.trimmed().toLower();
    return low.isEmpty()
        || low.contains(QStringLiteral("efectivo"))
        || low.contains(QStringLiteral("contado"))
        || low.contains(QStringLiteral("cash"));
}

int CashboxModule::activeOpenSessionIdForCurrentTerminal()
{
    if (!Database::instance().isOpen()) return -1;

    const int term = Database::instance().activeConfig().terminalNumber;
    QSqlQuery q(Database::instance().connection());
    q.prepare(QStringLiteral(
        "SELECT id FROM cash_sessions "
        "WHERE status = 'OPEN' AND terminal_number = :term "
        "ORDER BY id DESC LIMIT 1;"));
    q.bindValue(QStringLiteral(":term"), term);
    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }
    return -1;
}

void CashboxModule::setupUi()
{
    // =========================================================================
    // 1. Cabecera de Estado de Turno + Botones Apertura / Cierre / Informe
    // =========================================================================
    m_statusCard = new QFrame(this);
    m_statusCard->setObjectName(QStringLiteral("CardFrame"));
    auto *statusOuter = new QVBoxLayout(m_statusCard);
    statusOuter->setContentsMargins(14, 10, 14, 10);
    statusOuter->setSpacing(8);

    auto *statusRow = new QHBoxLayout();
    statusRow->setSpacing(10);

    auto *statusTextCol = new QVBoxLayout();
    statusTextCol->setSpacing(2);
    m_statusTitleLabel = new QLabel(m_statusCard);
    m_statusTitleLabel->setStyleSheet(QStringLiteral("font-size: 12pt; font-weight: 700; color: #1E3A5F;"));
    m_statusDetailLabel = new QLabel(m_statusCard);
    m_statusDetailLabel->setWordWrap(true);
    m_statusDetailLabel->setStyleSheet(QStringLiteral("color: #52606D; font-size: 9pt;"));
    statusTextCol->addWidget(m_statusTitleLabel);
    statusTextCol->addWidget(m_statusDetailLabel);
    statusRow->addLayout(statusTextCol, 1);

    m_openSessionBtn = new QPushButton(QStringLiteral("Abrir Caja"), m_statusCard);
    m_openSessionBtn->setIcon(Icons::get(Icons::Name::Unlock, QColor(QStringLiteral("#FFFFFF")), 15));

    m_closeSessionBtn = new QPushButton(QStringLiteral("Arqueo y Cierre de Caja"), m_statusCard);
    m_closeSessionBtn->setIcon(Icons::get(Icons::Name::Lock, QColor(QStringLiteral("#FFFFFF")), 15));
    m_closeSessionBtn->setProperty("danger", true);

    m_reportSessionBtn = new QPushButton(QStringLiteral("Informe de Turno"), m_statusCard);
    m_reportSessionBtn->setIcon(Icons::get(Icons::Name::Printer, QColor(QStringLiteral("#1E3A5F")), 15));
    m_reportSessionBtn->setProperty("secondary", true);

    statusRow->addWidget(m_openSessionBtn);
    statusRow->addWidget(m_closeSessionBtn);
    statusRow->addWidget(m_reportSessionBtn);
    statusOuter->addLayout(statusRow);

    // KPIs en vivo del turno
    auto *kpiRow = new QHBoxLayout();
    kpiRow->setSpacing(10);
    kpiRow->addWidget(makeKpiCard(QStringLiteral("Fondo Inicial"), &m_kpiOpeningLabel, QStringLiteral("#1E3A5F"), m_statusCard), 1);
    kpiRow->addWidget(makeKpiCard(QStringLiteral("Ingresos Totales"), &m_kpiIncomeLabel, QStringLiteral("#2E7D32"), m_statusCard), 1);
    kpiRow->addWidget(makeKpiCard(QStringLiteral("Egresos Totales"), &m_kpiExpenseLabel, QStringLiteral("#C53030"), m_statusCard), 1);
    kpiRow->addWidget(makeKpiCard(QStringLiteral("Efectivo Teórico en Cajón"), &m_kpiExpectedCashLabel, QStringLiteral("#2B7A78"), m_statusCard), 1);
    statusOuter->addLayout(kpiRow);

    m_methodsBreakdownLabel = new QLabel(m_statusCard);
    m_methodsBreakdownLabel->setWordWrap(true);
    m_methodsBreakdownLabel->setStyleSheet(QStringLiteral("color: #52606D; font-size: 9pt;"));
    statusOuter->addWidget(m_methodsBreakdownLabel);

    bodyLayout()->addWidget(m_statusCard);

    // =========================================================================
    // 2. Solapas principales: Turno Activo | Historial de Cierres y Arqueos
    // =========================================================================
    m_tabs = new QTabWidget(this);
    const QColor tabIconColor(QStringLiteral("#1E3A5F"));

    // --- Solapa 1: Turno Activo ---
    auto *activeTab = new QWidget(m_tabs);
    auto *activeTabLayout = new QVBoxLayout(activeTab);
    activeTabLayout->setContentsMargins(8, 8, 8, 8);

    m_activeSplitter = new QSplitter(Qt::Horizontal, activeTab);
    m_activeSplitter->setChildrenCollapsible(false);

    // Panel izquierdo: Carga de movimiento
    auto *formScroll = new QScrollArea(m_activeSplitter);
    formScroll->setWidgetResizable(true);
    formScroll->setFrameShape(QFrame::NoFrame);
    formScroll->setMinimumWidth(310);
    formScroll->setMaximumWidth(420);

    auto *formPage = new QWidget(formScroll);
    auto *formOuter = new QVBoxLayout(formPage);
    formOuter->setContentsMargins(0, 0, 6, 0);
    formOuter->setSpacing(8);

    m_closedNoticeLabel = new QLabel(
        QStringLiteral("<b>Caja cerrada:</b> Pulsa \"Abrir Caja\" en la barra superior para iniciar el turno y registrar movimientos."),
        formPage);
    m_closedNoticeLabel->setWordWrap(true);
    m_closedNoticeLabel->setStyleSheet(
        QStringLiteral("background-color: #FFF3CD; color: #664D03; border: 1px solid #FFECB5; "
                       "padding: 9px; border-radius: 6px;"));
    formOuter->addWidget(m_closedNoticeLabel);

    m_movFormGroup = new QGroupBox(QStringLiteral("Registrar Movimiento de Fondos"), formPage);
    auto *movGroupLayout = new QVBoxLayout(m_movFormGroup);
    movGroupLayout->setSpacing(10);

    auto *form = new QFormLayout();
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_movTypeCombo = new QComboBox(m_movFormGroup);
    m_movTypeCombo->addItem(QStringLiteral("Ingreso de fondos (+)"), QStringLiteral("INCOME"));
    m_movTypeCombo->addItem(QStringLiteral("Egreso / Gasto (-)"), QStringLiteral("EXPENSE"));

    m_categoryCombo = new QComboBox(m_movFormGroup);
    m_categoryCombo->setEditable(true);

    m_paymentMethodCombo = new QComboBox(m_movFormGroup);

    m_clientCombo = new QComboBox(m_movFormGroup);
    m_clientCombo->setEditable(false);

    m_amountSpin = new QDoubleSpinBox(m_movFormGroup);
    m_amountSpin->setRange(0.01, 999999999.99);
    m_amountSpin->setDecimals(2);
    m_amountSpin->setValue(0.0);

    m_referenceEdit = new QLineEdit(m_movFormGroup);
    m_referenceEdit->setPlaceholderText(QStringLiteral("Opcional: Nro. recibo, ticket, factura..."));

    m_descriptionEdit = new QLineEdit(m_movFormGroup);
    m_descriptionEdit->setPlaceholderText(QStringLiteral("Concepto o detalle del movimiento *"));

    form->addRow(QStringLiteral("Tipo *:"), m_movTypeCombo);
    form->addRow(QStringLiteral("Categoría:"), m_categoryCombo);
    form->addRow(QStringLiteral("Medio de Pago *:"), m_paymentMethodCombo);
    form->addRow(isClinicMode() ? QStringLiteral("Paciente (opcional):") : QStringLiteral("Cliente (opcional):"), m_clientCombo);
    form->addRow(QStringLiteral("Importe *:"), m_amountSpin);
    form->addRow(QStringLiteral("Comprobante / Ref.:"), m_referenceEdit);
    form->addRow(QStringLiteral("Concepto *:"), m_descriptionEdit);
    movGroupLayout->addLayout(form);

    auto *movBtnRow = new QHBoxLayout();
    m_registerMovBtn = new QPushButton(QStringLiteral("Registrar movimiento"), m_movFormGroup);
    m_registerMovBtn->setIcon(Icons::get(Icons::Name::Add, QColor(QStringLiteral("#FFFFFF")), 15));

    m_clearMovBtn = new QPushButton(QStringLiteral("Limpiar"), m_movFormGroup);
    m_clearMovBtn->setIcon(Icons::get(Icons::Name::Refresh, QColor(QStringLiteral("#1E3A5F")), 15));
    m_clearMovBtn->setProperty("secondary", true);

    movBtnRow->addWidget(m_registerMovBtn);
    movBtnRow->addWidget(m_clearMovBtn);
    movBtnRow->addStretch();
    movGroupLayout->addLayout(movBtnRow);

    formOuter->addWidget(m_movFormGroup);
    formOuter->addStretch();
    formScroll->setWidget(formPage);
    m_activeSplitter->addWidget(formScroll);

    // Panel derecho: Grilla de movimientos del turno actual
    auto *movListGroup = new QGroupBox(QStringLiteral("Movimientos del Turno Actual"), m_activeSplitter);
    auto *movListLayout = new QVBoxLayout(movListGroup);
    movListLayout->setSpacing(8);

    auto *filterBar = new QHBoxLayout();
    filterBar->setSpacing(8);

    m_searchEdit = new QLineEdit(movListGroup);
    m_searchEdit->setPlaceholderText(QStringLiteral("Buscar por concepto, categoría, referencia..."));
    m_searchEdit->setClearButtonEnabled(true);

    m_filterTypeCombo = new QComboBox(movListGroup);
    m_filterTypeCombo->addItem(QStringLiteral("Todos"), QStringLiteral("ALL"));
    m_filterTypeCombo->addItem(QStringLiteral("Solo Ingresos"), QStringLiteral("INCOME"));
    m_filterTypeCombo->addItem(QStringLiteral("Solo Egresos"), QStringLiteral("EXPENSE"));

    m_showVoidedCheck = new QCheckBox(QStringLiteral("Mostrar anulados"), movListGroup);
    m_showVoidedCheck->setChecked(true);

    auto *refreshMovBtn = new QPushButton(QStringLiteral("Actualizar"), movListGroup);
    refreshMovBtn->setIcon(Icons::get(Icons::Name::Refresh, QColor(QStringLiteral("#1E3A5F")), 14));
    refreshMovBtn->setProperty("secondary", true);

    filterBar->addWidget(m_searchEdit, 1);
    filterBar->addWidget(m_filterTypeCombo);
    filterBar->addWidget(m_showVoidedCheck);
    filterBar->addWidget(refreshMovBtn);
    movListLayout->addLayout(filterBar);

    m_movementsTable = new QTableWidget(movListGroup);
    m_movementsTable->setColumnCount(8);
    m_movementsTable->setHorizontalHeaderLabels(QStringList{
        QStringLiteral("Hora (UTC)"),
        QStringLiteral("Tipo"),
        QStringLiteral("Categoría"),
        QStringLiteral("Concepto / Ref."),
        QStringLiteral("Medio de Pago"),
        QStringLiteral("Importe"),
        QStringLiteral("Estado"),
        QStringLiteral("Acciones")
    });
    m_movementsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_movementsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_movementsTable->setAlternatingRowColors(true);
    m_movementsTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_movementsTable->verticalHeader()->setVisible(false);
    m_movementsTable->verticalHeader()->setDefaultSectionSize(40);
    m_movementsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_movementsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_movementsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_movementsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_movementsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_movementsTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_movementsTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    m_movementsTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    movListLayout->addWidget(m_movementsTable, 1);

    m_activeSplitter->addWidget(movListGroup);
    m_activeSplitter->setStretchFactor(0, 0);
    m_activeSplitter->setStretchFactor(1, 1);
    activeTabLayout->addWidget(m_activeSplitter, 1);

    m_tabs->addTab(activeTab, Icons::get(Icons::Name::Table, tabIconColor, 16), QStringLiteral("Turno Activo"));

    // --- Solapa 2: Historial de Cierres y Arqueos ---
    auto *histTab = new QWidget(m_tabs);
    auto *histLayout = new QVBoxLayout(histTab);
    histLayout->setContentsMargins(10, 10, 10, 10);
    histLayout->setSpacing(8);

    auto *histBar = new QHBoxLayout();
    auto *histHint = new QLabel(
        QStringLiteral("Registro histórico inmutable de turnos cerrados y arqueos de caja por puesto de trabajo."),
        histTab);
    histHint->setStyleSheet(QStringLiteral("color: #52606D;"));
    auto *refreshHistBtn = new QPushButton(QStringLiteral("Actualizar historial"), histTab);
    refreshHistBtn->setIcon(Icons::get(Icons::Name::Refresh, QColor(QStringLiteral("#1E3A5F")), 14));
    refreshHistBtn->setProperty("secondary", true);
    histBar->addWidget(histHint, 1);
    histBar->addWidget(refreshHistBtn);
    histLayout->addLayout(histBar);

    m_historyTable = new QTableWidget(histTab);
    m_historyTable->setColumnCount(9);
    m_historyTable->setHorizontalHeaderLabels(QStringList{
        QStringLiteral("Turno #"),
        QStringLiteral("Puesto"),
        QStringLiteral("Apertura"),
        QStringLiteral("Cierre"),
        QStringLiteral("Responsable"),
        QStringLiteral("Efectivo Teórico"),
        QStringLiteral("Efectivo Contado"),
        QStringLiteral("Diferencia"),
        QStringLiteral("Informe")
    });
    m_historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_historyTable->setAlternatingRowColors(true);
    m_historyTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_historyTable->verticalHeader()->setVisible(false);
    m_historyTable->verticalHeader()->setDefaultSectionSize(40);
    m_historyTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_historyTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(8, QHeaderView::ResizeToContents);
    histLayout->addWidget(m_historyTable, 1);

    m_tabs->addTab(histTab, Icons::get(Icons::Name::Shield, tabIconColor, 16), QStringLiteral("Historial de Cierres"));
    bodyLayout()->addWidget(m_tabs, 1);

    // Conexiones
    connect(m_openSessionBtn, &QPushButton::clicked, this, &CashboxModule::onOpenSessionClicked);
    connect(m_closeSessionBtn, &QPushButton::clicked, this, &CashboxModule::onCloseSessionClicked);
    connect(m_reportSessionBtn, &QPushButton::clicked, this, &CashboxModule::onGenerateActiveSessionReport);

    connect(m_movTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CashboxModule::onMovementTypeChanged);
    connect(m_registerMovBtn, &QPushButton::clicked, this, &CashboxModule::onRegisterMovementClicked);
    connect(m_clearMovBtn, &QPushButton::clicked, this, &CashboxModule::onClearMovementForm);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &CashboxModule::onFilterChanged);
    connect(m_filterTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CashboxModule::onFilterChanged);
    connect(m_showVoidedCheck, &QCheckBox::toggled, this, &CashboxModule::onFilterChanged);
    connect(refreshMovBtn, &QPushButton::clicked, this, &CashboxModule::reloadAll);
    connect(refreshHistBtn, &QPushButton::clicked, this, &CashboxModule::reloadHistoryTable);

    onMovementTypeChanged(0);
}

void CashboxModule::resizeEvent(QResizeEvent *event)
{
    ModuleBase::resizeEvent(event);
    const Qt::Orientation target =
        (event->size().width() < kVerticalBreakpointPx) ? Qt::Vertical : Qt::Horizontal;
    if (m_activeSplitter && m_activeSplitter->orientation() != target) {
        m_activeSplitter->setOrientation(target);
    }
}

void CashboxModule::onActivated()
{
    loadSupportCombos();
    reloadAll();

    AuditEvent ev;
    ev.moduleId = moduleId();
    ev.targetTable = QStringLiteral("cash_sessions");
    ev.action = QStringLiteral("VIEW");
    ev.details = QStringLiteral("Acceso al módulo Caja y Movimientos de Fondos");
    Database::instance().logAuditEvent(ev);
}

void CashboxModule::loadSupportCombos()
{
    m_amountSpin->setPrefix(currencySymbol() + QLatin1Char(' '));

    const QVariant prevMethod = m_paymentMethodCombo->currentData();
    m_paymentMethodCombo->clear();
    {
        QSqlQuery q(Database::instance().connection());
        if (q.exec(QStringLiteral(
                "SELECT id, name FROM payment_methods "
                "WHERE is_active = 1 AND is_deprecated = 0 ORDER BY id ASC;"))) {
            while (q.next()) {
                m_paymentMethodCombo->addItem(q.value(1).toString(), q.value(0).toInt());
            }
        }
    }
    if (m_paymentMethodCombo->count() == 0) {
        m_paymentMethodCombo->addItem(QStringLiteral("Efectivo"), -1);
    } else if (prevMethod.isValid()) {
        const int idx = m_paymentMethodCombo->findData(prevMethod);
        if (idx >= 0) m_paymentMethodCombo->setCurrentIndex(idx);
    }

    const QVariant prevClient = m_clientCombo->currentData();
    m_clientCombo->clear();
    m_clientCombo->addItem(QStringLiteral("(Sin asignar)"), -1);
    {
        QSqlQuery q(Database::instance().connection());
        if (q.exec(QStringLiteral(
                "SELECT id, full_name, document_number FROM clients "
                "WHERE is_active = 1 AND is_deprecated = 0 ORDER BY full_name ASC;"))) {
            while (q.next()) {
                const QString name = q.value(1).toString();
                const QString doc  = q.value(2).toString();
                const QString lbl  = doc.isEmpty() ? name : QStringLiteral("%1 (%2)").arg(name, doc);
                m_clientCombo->addItem(lbl, q.value(0).toInt());
            }
        }
    }
    if (prevClient.isValid()) {
        const int idx = m_clientCombo->findData(prevClient);
        if (idx >= 0) m_clientCombo->setCurrentIndex(idx);
    }
}

void CashboxModule::onMovementTypeChanged(int index)
{
    const bool isIncome = (m_movTypeCombo->itemData(index).toString() == QStringLiteral("INCOME"));
    m_categoryCombo->clear();
    if (isIncome) {
        m_categoryCombo->addItems(isClinicMode()
            ? QStringList{
                  QStringLiteral("Cobro de Consulta / Prestación"),
                  QStringLiteral("Cobro Coseguro / Particular"),
                  QStringLiteral("Cobro Cuenta Corriente"),
                  QStringLiteral("Aporte / Refuerzo de Caja"),
                  QStringLiteral("Otros Ingresos")
              }
            : QStringList{
                  QStringLiteral("Cobro de Venta"),
                  QStringLiteral("Cobro Cuenta Corriente"),
                  QStringLiteral("Seña / Adelanto"),
                  QStringLiteral("Aporte / Refuerzo de Caja"),
                  QStringLiteral("Otros Ingresos")
              });
    } else {
        m_categoryCombo->addItems(QStringList{
            QStringLiteral("Pago a Proveedor"),
            QStringLiteral("Gasto Operativo / Servicios"),
            QStringLiteral("Compra de Insumos"),
            QStringLiteral("Retiro de Efectivo"),
            QStringLiteral("Devolución a Cliente / Paciente"),
            QStringLiteral("Otros Egresos")
        });
    }
}

void CashboxModule::reloadAll()
{
    loadActiveSession();
    updateSessionHeaderUi();
    reloadActiveMovements();
    reloadHistoryTable();
}

bool CashboxModule::loadSessionById(int sessionId, CashSessionInfo *out) const
{
    if (sessionId <= 0 || !out) return false;

    QSqlQuery q(Database::instance().connection());
    q.prepare(QStringLiteral(
        "SELECT id, terminal_number, workstation, opened_by_email, opened_by_name, opened_at, "
        "       opening_amount, closed_by_email, closed_by_name, closed_at, "
        "       expected_cash, counted_cash, difference_amount, status, notes "
        "FROM cash_sessions WHERE id = :id LIMIT 1;"));
    q.bindValue(QStringLiteral(":id"), sessionId);
    if (!q.exec() || !q.next()) return false;

    out->id               = q.value(0).toInt();
    out->terminalNumber   = q.value(1).toInt();
    out->workstation      = q.value(2).toString();
    out->openedByEmail    = q.value(3).toString();
    out->openedByName     = q.value(4).toString();
    out->openedAt         = q.value(5).toString();
    out->openingAmount    = q.value(6).toDouble();
    out->closedByEmail    = q.value(7).toString();
    out->closedByName     = q.value(8).toString();
    out->closedAt         = q.value(9).toString();
    out->expectedCash     = q.value(10).toDouble();
    out->countedCash      = q.value(11).toDouble();
    out->differenceAmount = q.value(12).toDouble();
    out->status           = q.value(13).toString();
    out->notes            = q.value(14).toString();
    return true;
}

CashboxModule::SessionTotals CashboxModule::computeSessionTotals(int sessionId, double openingAmount) const
{
    SessionTotals t;
    t.expectedCashInDrawer = openingAmount;
    if (sessionId <= 0) return t;

    QSqlQuery q(Database::instance().connection());
    q.prepare(QStringLiteral(
        "SELECT movement_type, payment_method_name, amount "
        "FROM cash_movements "
        "WHERE session_id = :sid AND is_voided = 0;"));
    q.bindValue(QStringLiteral(":sid"), sessionId);
    if (!q.exec()) return t;

    while (q.next()) {
        const QString type   = q.value(0).toString();
        const QString method = q.value(1).toString().trimmed().isEmpty()
                                   ? QStringLiteral("Efectivo")
                                   : q.value(1).toString().trimmed();
        const double amt     = q.value(2).toDouble();
        const bool isCash    = isCashMethodName(method);

        auto &pair = t.byMethod[method];
        if (type == QStringLiteral("INCOME")) {
            t.totalIncome += amt;
            pair.first += amt;
            if (isCash) t.cashIncome += amt;
        } else {
            t.totalExpense += amt;
            pair.second += amt;
            if (isCash) t.cashExpense += amt;
        }
    }

    t.expectedCashInDrawer = openingAmount + t.cashIncome - t.cashExpense;
    return t;
}

void CashboxModule::loadActiveSession()
{
    m_activeSession = CashSessionInfo{};
    m_activeTotals = SessionTotals{};

    const int openId = activeOpenSessionIdForCurrentTerminal();
    if (openId > 0 && loadSessionById(openId, &m_activeSession)) {
        m_activeTotals = computeSessionTotals(m_activeSession.id, m_activeSession.openingAmount);
    }
}

void CashboxModule::updateSessionHeaderUi()
{
    const bool isOpen = (m_activeSession.id > 0);
    const int term = Database::instance().activeConfig().terminalNumber;

    m_openSessionBtn->setVisible(!isOpen);
    m_closeSessionBtn->setVisible(isOpen);
    m_reportSessionBtn->setVisible(isOpen);

    m_closedNoticeLabel->setVisible(!isOpen);
    m_movFormGroup->setEnabled(isOpen);

    if (isOpen) {
        m_statusTitleLabel->setText(
            QStringLiteral("CAJA ABIERTA · Turno #%1 (Puesto #%2)").arg(m_activeSession.id).arg(m_activeSession.terminalNumber));
        m_statusTitleLabel->setStyleSheet(QStringLiteral("font-size: 12pt; font-weight: 700; color: #2E7D32;"));
        m_statusDetailLabel->setText(
            QStringLiteral("Apertura: %1 (UTC)  ·  Responsable: %2 (%3)  ·  Equipo: %4")
                .arg(m_activeSession.openedAt,
                     m_activeSession.openedByName,
                     m_activeSession.openedByEmail,
                     m_activeSession.workstation));

        m_kpiOpeningLabel->setText(formatMoney(m_activeSession.openingAmount));
        m_kpiIncomeLabel->setText(formatMoney(m_activeTotals.totalIncome));
        m_kpiExpenseLabel->setText(formatMoney(m_activeTotals.totalExpense));
        m_kpiExpectedCashLabel->setText(formatMoney(m_activeTotals.expectedCashInDrawer));

        if (m_activeTotals.byMethod.isEmpty()) {
            m_methodsBreakdownLabel->setText(QStringLiteral("Sin movimientos registrados en el turno actual."));
        } else {
            QStringList parts;
            for (auto it = m_activeTotals.byMethod.constBegin(); it != m_activeTotals.byMethod.constEnd(); ++it) {
                const double net = it.value().first - it.value().second;
                parts.append(QStringLiteral("<b>%1:</b> %2").arg(it.key().toHtmlEscaped(), formatMoney(net)));
            }
            m_methodsBreakdownLabel->setText(
                QStringLiteral("Neto por medio de pago → ") + parts.join(QStringLiteral(" &nbsp;|&nbsp; ")));
        }
    } else {
        m_statusTitleLabel->setText(QStringLiteral("CAJA CERRADA · Puesto #%1").arg(term));
        m_statusTitleLabel->setStyleSheet(QStringLiteral("font-size: 12pt; font-weight: 700; color: #B7791F;"));
        m_statusDetailLabel->setText(
            QStringLiteral("No hay un turno abierto en este puesto (%1). Realiza la apertura indicando el fondo inicial de efectivo.")
                .arg(QHostInfo::localHostName()));

        m_kpiOpeningLabel->setText(formatMoney(0.0));
        m_kpiIncomeLabel->setText(formatMoney(0.0));
        m_kpiExpenseLabel->setText(formatMoney(0.0));
        m_kpiExpectedCashLabel->setText(formatMoney(0.0));
        m_methodsBreakdownLabel->setText(QString());
    }
}

void CashboxModule::onOpenSessionClicked()
{
    if (!requirePermission(QStringLiteral("open_close"), QStringLiteral("abrir un turno de caja"))) {
        return;
    }

    if (activeOpenSessionIdForCurrentTerminal() > 0) {
        reloadAll();
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Apertura de Caja"));
    dlg.setModal(true);
    dlg.setMinimumWidth(420);

    auto *layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    const UserSessionInfo &user = Session::instance().currentUser();
    const int term = Database::instance().activeConfig().terminalNumber;

    auto *info = new QLabel(
        QStringLiteral("Iniciando turno en <b>Puesto #%1 (%2)</b><br>Responsable: <b>%3</b>")
            .arg(term)
            .arg(QHostInfo::localHostName().toHtmlEscaped(), user.fullName.toHtmlEscaped()),
        &dlg);
    info->setWordWrap(true);
    info->setStyleSheet(
        QStringLiteral("background-color: #EEF4FA; color: #1E3A5F; border: 1px solid #D9E2EC; "
                       "padding: 9px; border-radius: 6px;"));
    layout->addWidget(info);

    auto *form = new QFormLayout();
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);

    auto *openingSpin = new QDoubleSpinBox(&dlg);
    openingSpin->setRange(0.0, 999999999.99);
    openingSpin->setDecimals(2);
    openingSpin->setPrefix(currencySymbol() + QLatin1Char(' '));
    openingSpin->setValue(0.0);

    auto *notesEdit = new QLineEdit(&dlg);
    notesEdit->setPlaceholderText(QStringLiteral("Ej: Cambio inicial turno mañana (opcional)"));

    form->addRow(QStringLiteral("Fondo inicial en efectivo *:"), openingSpin);
    form->addRow(QStringLiteral("Observaciones de apertura:"), notesEdit);
    layout->addLayout(form);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto *cancelBtn = new QPushButton(QStringLiteral("Cancelar"), &dlg);
    cancelBtn->setIcon(Icons::get(Icons::Name::Close, QColor(QStringLiteral("#1E3A5F")), 15));
    cancelBtn->setProperty("secondary", true);

    auto *okBtn = new QPushButton(QStringLiteral("Confirmar apertura"), &dlg);
    okBtn->setIcon(Icons::get(Icons::Name::Unlock, QColor(QStringLiteral("#FFFFFF")), 15));
    okBtn->setDefault(true);

    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(okBtn);
    layout->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() != QDialog::Accepted) return;

    const double openingAmount = openingSpin->value();
    const QString notes = notesEdit->text().trimmed();

    QSqlQuery q(Database::instance().connection());
    q.prepare(QStringLiteral(
        "INSERT INTO cash_sessions ("
        "  terminal_number, workstation, opened_by_email, opened_by_name, "
        "  opening_amount, closed_by_email, closed_by_name, closed_at, "
        "  expected_cash, counted_cash, difference_amount, status, notes"
        ") VALUES ("
        "  :term, :ws, :uemail, :uname, :oamt, '', '', '', :oamt, 0, 0, 'OPEN', :notes"
        ");"));
    q.bindValue(QStringLiteral(":term"),   term);
    q.bindValue(QStringLiteral(":ws"),     QHostInfo::localHostName());
    q.bindValue(QStringLiteral(":uemail"), user.email);
    q.bindValue(QStringLiteral(":uname"),  user.fullName);
    q.bindValue(QStringLiteral(":oamt"),   openingAmount);
    q.bindValue(QStringLiteral(":notes"),  notes);

    if (!q.exec()) {
        QMessageBox::critical(this, QStringLiteral("Error al abrir caja"), q.lastError().text());
        return;
    }

    const int newSessionId = q.lastInsertId().toInt();

    AuditEvent ev;
    ev.moduleId = moduleId();
    ev.targetTable = QStringLiteral("cash_sessions");
    ev.recordId = QString::number(newSessionId);
    ev.action = QStringLiteral("CASH_OPEN");
    ev.newValuesJson = QStringLiteral("{\"opening_amount\":%1,\"terminal\":%2}").arg(openingAmount).arg(term);
    ev.details = QStringLiteral("Apertura Turno #%1 · Fondo inicial: %2").arg(newSessionId).arg(formatMoney(openingAmount));
    Database::instance().logAuditEvent(ev);

    reloadAll();
}

void CashboxModule::onCloseSessionClicked()
{
    if (!requirePermission(QStringLiteral("open_close"), QStringLiteral("realizar el cierre y arqueo de caja"))) {
        return;
    }

    loadActiveSession();
    if (m_activeSession.id <= 0) return;

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Arqueo y Cierre de Caja - Turno #%1").arg(m_activeSession.id));
    dlg.setModal(true);
    dlg.setMinimumWidth(480);

    auto *layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    QString methodLines;
    for (auto it = m_activeTotals.byMethod.constBegin(); it != m_activeTotals.byMethod.constEnd(); ++it) {
        methodLines += QStringLiteral("• %1: Ingresos %2 | Egresos %3<br>")
                           .arg(it.key().toHtmlEscaped(), formatMoney(it.value().first), formatMoney(it.value().second));
    }
    if (methodLines.isEmpty()) {
        methodLines = QStringLiteral("<i>Sin movimientos registrados durante el turno.</i><br>");
    }

    auto *summaryLbl = new QLabel(
        QStringLiteral("<b>Resumen Teórico del Turno #%1</b><br><br>"
                       "• Fondo inicial: <b>%2</b><br>"
                       "• Ingresos en efectivo: <b>%3</b> · Egresos en efectivo: <b>%4</b><br>"
                       "• <b>Efectivo teórico esperado en cajón: %5</b><br><br>"
                       "<b>Detalle por medio de pago:</b><br>%6")
            .arg(m_activeSession.id)
            .arg(formatMoney(m_activeSession.openingAmount),
                 formatMoney(m_activeTotals.cashIncome),
                 formatMoney(m_activeTotals.cashExpense),
                 formatMoney(m_activeTotals.expectedCashInDrawer),
                 methodLines),
        &dlg);
    summaryLbl->setWordWrap(true);
    summaryLbl->setStyleSheet(
        QStringLiteral("background-color: #EEF4FA; color: #1E3A5F; border: 1px solid #D9E2EC; "
                       "padding: 10px; border-radius: 6px;"));
    layout->addWidget(summaryLbl);

    auto *form = new QFormLayout();
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);

    auto *countedSpin = new QDoubleSpinBox(&dlg);
    countedSpin->setRange(0.0, 999999999.99);
    countedSpin->setDecimals(2);
    countedSpin->setPrefix(currencySymbol() + QLatin1Char(' '));
    countedSpin->setValue(qMax(0.0, m_activeTotals.expectedCashInDrawer));

    auto *diffLabel = new QLabel(&dlg);
    diffLabel->setStyleSheet(QStringLiteral("font-weight: 700; color: #2E7D32;"));

    auto updateDiff = [&]() {
        const double diff = countedSpin->value() - m_activeTotals.expectedCashInDrawer;
        if (qAbs(diff) < 0.005) {
            diffLabel->setText(QStringLiteral("Sin diferencia (Arqueo exacto)"));
            diffLabel->setStyleSheet(QStringLiteral("font-weight: 700; color: #2E7D32;"));
        } else if (diff > 0.0) {
            diffLabel->setText(QStringLiteral("Sobrante de +%1").arg(formatMoney(diff)));
            diffLabel->setStyleSheet(QStringLiteral("font-weight: 700; color: #2B6CB0;"));
        } else {
            diffLabel->setText(QStringLiteral("Faltante de -%1").arg(formatMoney(qAbs(diff))));
            diffLabel->setStyleSheet(QStringLiteral("font-weight: 700; color: #C53030;"));
        }
    };
    updateDiff();
    connect(countedSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg, [&](double) {
        updateDiff();
    });

    auto *notesEdit = new QPlainTextEdit(&dlg);
    notesEdit->setMaximumHeight(70);
    notesEdit->setPlaceholderText(QStringLiteral("Observaciones de cierre / justificación de diferencia (opcional)"));

    auto *showReportCheck = new QCheckBox(QStringLiteral("Abrir Informe de Arqueo y Cierre al finalizar"), &dlg);
    showReportCheck->setChecked(true);

    form->addRow(QStringLiteral("Efectivo físico contado *:"), countedSpin);
    form->addRow(QStringLiteral("Resultado del arqueo:"), diffLabel);
    form->addRow(QStringLiteral("Observaciones:"), notesEdit);
    form->addRow(QString(), showReportCheck);
    layout->addLayout(form);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto *cancelBtn = new QPushButton(QStringLiteral("Cancelar"), &dlg);
    cancelBtn->setIcon(Icons::get(Icons::Name::Close, QColor(QStringLiteral("#1E3A5F")), 15));
    cancelBtn->setProperty("secondary", true);

    auto *confirmBtn = new QPushButton(QStringLiteral("Confirmar cierre de caja"), &dlg);
    confirmBtn->setIcon(Icons::get(Icons::Name::Lock, QColor(QStringLiteral("#FFFFFF")), 15));
    confirmBtn->setProperty("danger", true);

    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(confirmBtn);
    layout->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(confirmBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() != QDialog::Accepted) return;

    const int closedSessionId = m_activeSession.id;
    const double expected = m_activeTotals.expectedCashInDrawer;
    const double counted  = countedSpin->value();
    const double diff     = counted - expected;
    const UserSessionInfo &user = Session::instance().currentUser();
    const QString closedAt = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));

    QString combinedNotes = m_activeSession.notes;
    const QString closeNotes = notesEdit->toPlainText().trimmed();
    if (!closeNotes.isEmpty()) {
        if (!combinedNotes.isEmpty()) combinedNotes += QStringLiteral(" | ");
        combinedNotes += QStringLiteral("Cierre: ") + closeNotes;
    }

    QSqlQuery q(Database::instance().connection());
    q.prepare(QStringLiteral(
        "UPDATE cash_sessions SET "
        "  closed_by_email = :uemail, closed_by_name = :uname, closed_at = :cat, "
        "  expected_cash = :exp, counted_cash = :cnt, difference_amount = :diff, "
        "  status = 'CLOSED', notes = :notes "
        "WHERE id = :id;"));
    q.bindValue(QStringLiteral(":uemail"), user.email);
    q.bindValue(QStringLiteral(":uname"),  user.fullName);
    q.bindValue(QStringLiteral(":cat"),    closedAt);
    q.bindValue(QStringLiteral(":exp"),    expected);
    q.bindValue(QStringLiteral(":cnt"),    counted);
    q.bindValue(QStringLiteral(":diff"),   diff);
    q.bindValue(QStringLiteral(":notes"),  combinedNotes);
    q.bindValue(QStringLiteral(":id"),     closedSessionId);

    if (!q.exec()) {
        QMessageBox::critical(this, QStringLiteral("Error al cerrar caja"), q.lastError().text());
        return;
    }

    QJsonObject newJson;
    newJson.insert(QStringLiteral("expected_cash"), expected);
    newJson.insert(QStringLiteral("counted_cash"), counted);
    newJson.insert(QStringLiteral("difference"), diff);

    AuditEvent ev;
    ev.moduleId = moduleId();
    ev.targetTable = QStringLiteral("cash_sessions");
    ev.recordId = QString::number(closedSessionId);
    ev.action = QStringLiteral("CASH_CLOSE");
    ev.newValuesJson = compactJson(newJson);
    ev.details = QStringLiteral("Cierre Turno #%1 · Esperado: %2 · Contado: %3 · Dif: %4")
                     .arg(closedSessionId)
                     .arg(formatMoney(expected), formatMoney(counted), formatMoney(diff));
    Database::instance().logAuditEvent(ev);

    reloadAll();

    if (showReportCheck->isChecked()) {
        showSessionReport(closedSessionId);
    }
}

void CashboxModule::onRegisterMovementClicked()
{
    if (!requirePermission(QStringLiteral("movement"), QStringLiteral("registrar movimientos de caja"))) {
        return;
    }

    if (m_activeSession.id <= 0) {
        QMessageBox::warning(this, QStringLiteral("Caja cerrada"),
                             QStringLiteral("Debes abrir un turno de caja antes de registrar movimientos."));
        return;
    }

    const QString movType    = m_movTypeCombo->currentData().toString();
    const QString category   = m_categoryCombo->currentText().trimmed();
    const int payMethodId    = m_paymentMethodCombo->currentData().toInt();
    const QString payMethod  = m_paymentMethodCombo->currentText().trimmed();
    const int clientId       = m_clientCombo->currentData().toInt();
    const double amount      = m_amountSpin->value();
    const QString refCode    = m_referenceEdit->text().trimmed();
    const QString desc       = m_descriptionEdit->text().trimmed();

    if (amount <= 0.0) {
        QMessageBox::warning(this, QStringLiteral("Importe inválido"),
                             QStringLiteral("El importe del movimiento debe ser mayor a cero."));
        m_amountSpin->setFocus();
        return;
    }

    if (desc.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Concepto requerido"),
                             QStringLiteral("Ingresa una breve descripción o concepto del movimiento."));
        m_descriptionEdit->setFocus();
        return;
    }

    QSqlQuery q(Database::instance().connection());
    q.prepare(QStringLiteral(
        "INSERT INTO cash_movements ("
        "  session_id, movement_type, category, payment_method_id, payment_method_name, "
        "  amount, description, reference_code, client_id, user_email, is_voided"
        ") VALUES ("
        "  :sid, :mtype, :cat, :pmid, :pmname, :amt, :desc, :ref, :cid, :uemail, 0"
        ");"));
    q.bindValue(QStringLiteral(":sid"),    m_activeSession.id);
    q.bindValue(QStringLiteral(":mtype"),  movType);
    q.bindValue(QStringLiteral(":cat"),    category);
    q.bindValue(QStringLiteral(":pmid"),   payMethodId > 0 ? QVariant(payMethodId) : QVariant());
    q.bindValue(QStringLiteral(":pmname"), payMethod);
    q.bindValue(QStringLiteral(":amt"),    amount);
    q.bindValue(QStringLiteral(":desc"),   desc);
    q.bindValue(QStringLiteral(":ref"),    refCode);
    q.bindValue(QStringLiteral(":cid"),    clientId > 0 ? QVariant(clientId) : QVariant());
    q.bindValue(QStringLiteral(":uemail"), Session::instance().currentUser().email);

    if (!q.exec()) {
        QMessageBox::critical(this, QStringLiteral("Error al registrar"), q.lastError().text());
        return;
    }

    const int movId = q.lastInsertId().toInt();

    QJsonObject newJson;
    newJson.insert(QStringLiteral("session_id"), m_activeSession.id);
    newJson.insert(QStringLiteral("type"), movType);
    newJson.insert(QStringLiteral("method"), payMethod);
    newJson.insert(QStringLiteral("amount"), amount);
    newJson.insert(QStringLiteral("description"), desc);

    AuditEvent ev;
    ev.moduleId = moduleId();
    ev.targetTable = QStringLiteral("cash_movements");
    ev.recordId = QString::number(movId);
    ev.action = (movType == QStringLiteral("INCOME")) ? QStringLiteral("CASH_INCOME") : QStringLiteral("CASH_EXPENSE");
    ev.newValuesJson = compactJson(newJson);
    ev.details = QStringLiteral("Turno #%1 · %2 (%3): %4 - %5")
                     .arg(m_activeSession.id)
                     .arg(movType == QStringLiteral("INCOME") ? QStringLiteral("Ingreso") : QStringLiteral("Egreso"),
                          payMethod,
                          formatMoney(amount),
                          desc);
    Database::instance().logAuditEvent(ev);

    onClearMovementForm();
    reloadAll();
}

void CashboxModule::onClearMovementForm()
{
    m_amountSpin->setValue(0.0);
    m_referenceEdit->clear();
    m_descriptionEdit->clear();
    m_clientCombo->setCurrentIndex(0);
    m_amountSpin->setFocus();
}

void CashboxModule::onFilterChanged()
{
    reloadActiveMovements();
}

void CashboxModule::reloadActiveMovements()
{
    m_movementsTable->setRowCount(0);
    if (m_activeSession.id <= 0) return;

    const QString search     = m_searchEdit->text().trimmed();
    const QString typeFilter = m_filterTypeCombo->currentData().toString();
    const bool showVoided    = m_showVoidedCheck->isChecked();

    QString sql = QStringLiteral(
        "SELECT m.id, m.created_at, m.movement_type, m.category, m.description, "
        "       m.reference_code, m.payment_method_name, m.amount, m.is_voided, "
        "       m.void_reason, COALESCE(c.full_name, '') "
        "FROM cash_movements m "
        "LEFT JOIN clients c ON c.id = m.client_id "
        "WHERE m.session_id = :sid ");

    if (!showVoided) {
        sql += QStringLiteral("AND m.is_voided = 0 ");
    }
    if (typeFilter != QStringLiteral("ALL")) {
        sql += QStringLiteral("AND m.movement_type = :mtype ");
    }
    if (!search.isEmpty()) {
        sql += QStringLiteral(
            "AND (LOWER(m.description) LIKE :q OR LOWER(m.category) LIKE :q "
            "  OR LOWER(m.reference_code) LIKE :q OR LOWER(m.payment_method_name) LIKE :q) ");
    }
    sql += QStringLiteral("ORDER BY m.id DESC;");

    QSqlQuery q(Database::instance().connection());
    q.prepare(sql);
    q.bindValue(QStringLiteral(":sid"), m_activeSession.id);
    if (typeFilter != QStringLiteral("ALL")) {
        q.bindValue(QStringLiteral(":mtype"), typeFilter);
    }
    if (!search.isEmpty()) {
        q.bindValue(QStringLiteral(":q"), QStringLiteral("%%1%").arg(search.toLower()));
    }

    if (!q.exec()) return;

    int row = 0;
    while (q.next()) {
        const int id          = q.value(0).toInt();
        const QString created = q.value(1).toString();
        const QString mtype   = q.value(2).toString();
        const QString cat     = q.value(3).toString();
        const QString desc    = q.value(4).toString();
        const QString ref     = q.value(5).toString();
        const QString method  = q.value(6).toString();
        const double amount   = q.value(7).toDouble();
        const bool voided     = (q.value(8).toInt() == 1);
        const QString vReason = q.value(9).toString();
        const QString cName   = q.value(10).toString();

        const bool isIncome = (mtype == QStringLiteral("INCOME"));
        QString detailText = desc;
        if (!ref.isEmpty()) detailText += QStringLiteral(" [Ref: %1]").arg(ref);
        if (!cName.isEmpty()) detailText += QStringLiteral(" · %1").arg(cName);
        if (voided && !vReason.isEmpty()) detailText += QStringLiteral(" (Anulado: %1)").arg(vReason);

        m_movementsTable->insertRow(row);
        m_movementsTable->setItem(row, 0, new QTableWidgetItem(created));

        auto *typeItem = new QTableWidgetItem(isIncome ? QStringLiteral("Ingreso (+)") : QStringLiteral("Egreso (-)"));
        typeItem->setForeground(QBrush(QColor(isIncome ? QStringLiteral("#2E7D32") : QStringLiteral("#C53030"))));
        m_movementsTable->setItem(row, 1, typeItem);

        m_movementsTable->setItem(row, 2, new QTableWidgetItem(cat));
        m_movementsTable->setItem(row, 3, new QTableWidgetItem(detailText));
        m_movementsTable->setItem(row, 4, new QTableWidgetItem(method));

        auto *amtItem = new QTableWidgetItem((isIncome ? QStringLiteral("+ ") : QStringLiteral("- ")) + formatMoney(amount));
        amtItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        amtItem->setForeground(QBrush(QColor(isIncome ? QStringLiteral("#2E7D32") : QStringLiteral("#C53030"))));
        m_movementsTable->setItem(row, 5, amtItem);

        auto *stItem = new QTableWidgetItem(voided ? QStringLiteral("Anulado") : QStringLiteral("Vigente"));
        stItem->setForeground(QBrush(QColor(voided ? QStringLiteral("#B7791F") : QStringLiteral("#2E7D32"))));
        m_movementsTable->setItem(row, 6, stItem);

        if (voided) {
            const QBrush vBg(QColor(QStringLiteral("#FFFBEB")));
            const QBrush vFg(QColor(QStringLiteral("#744210")));
            for (int c = 0; c < 7; ++c) {
                if (QTableWidgetItem *it = m_movementsTable->item(row, c)) {
                    it->setBackground(vBg);
                    it->setForeground(vFg);
                }
            }
        } else {
            auto *actWidget = new QWidget(m_movementsTable);
            auto *actLay = new QHBoxLayout(actWidget);
            actLay->setContentsMargins(4, 2, 4, 2);

            auto *voidBtn = new QPushButton(QStringLiteral("Anular"), actWidget);
            voidBtn->setIcon(Icons::get(Icons::Name::Close, QColor(QStringLiteral("#FFFFFF")), 13));
            voidBtn->setProperty("danger", true);

            const QString summary = QStringLiteral("%1 de %2 (%3)").arg(
                isIncome ? QStringLiteral("Ingreso") : QStringLiteral("Egreso"),
                formatMoney(amount),
                desc);

            connect(voidBtn, &QPushButton::clicked, this, [this, id, summary]() {
                onVoidMovement(id, summary);
            });

            actLay->addWidget(voidBtn);
            m_movementsTable->setCellWidget(row, 7, actWidget);
        }

        ++row;
    }

    m_movementsTable->resizeColumnToContents(7);
}

void CashboxModule::onVoidMovement(int movementId, const QString &summaryText)
{
    if (!requirePermission(QStringLiteral("void"), QStringLiteral("anular movimientos de caja"))) {
        return;
    }

    bool okInput = false;
    const QString reason = QInputDialog::getText(
        this,
        QStringLiteral("Anular movimiento de caja"),
        QStringLiteral("Vas a anular: %1\n\nIndica el motivo obligatorio para la bitácora de auditoría:").arg(summaryText),
        QLineEdit::Normal,
        QString(),
        &okInput).trimmed();

    if (!okInput) return;

    if (reason.size() < 3) {
        QMessageBox::warning(this, QStringLiteral("Motivo requerido"),
                             QStringLiteral("Debes ingresar un motivo breve (mínimo 3 caracteres) para anular un movimiento."));
        return;
    }

    QSqlQuery q(Database::instance().connection());
    q.prepare(QStringLiteral(
        "UPDATE cash_movements SET is_voided = 1, voided_by = :by, voided_at = :at, void_reason = :reason "
        "WHERE id = :id AND is_voided = 0;"));
    q.bindValue(QStringLiteral(":by"),     Session::instance().currentUser().email);
    q.bindValue(QStringLiteral(":at"),     QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    q.bindValue(QStringLiteral(":reason"), reason);
    q.bindValue(QStringLiteral(":id"),     movementId);

    if (!q.exec()) {
        QMessageBox::warning(this, QStringLiteral("Error"), q.lastError().text());
        return;
    }

    AuditEvent ev;
    ev.moduleId = moduleId();
    ev.targetTable = QStringLiteral("cash_movements");
    ev.recordId = QString::number(movementId);
    ev.action = QStringLiteral("VOID");
    ev.oldValuesJson = QStringLiteral("{\"is_voided\":false}");
    ev.newValuesJson = QStringLiteral("{\"is_voided\":true}");
    ev.details = QStringLiteral("Anulado %1 · Motivo: %2").arg(summaryText, reason);
    Database::instance().logAuditEvent(ev);

    reloadAll();
}

void CashboxModule::reloadHistoryTable()
{
    m_historyTable->setRowCount(0);

    QSqlQuery q(Database::instance().connection());
    if (!q.exec(QStringLiteral(
            "SELECT id, terminal_number, workstation, opened_at, closed_at, "
            "       opened_by_name, closed_by_name, expected_cash, counted_cash, difference_amount "
            "FROM cash_sessions "
            "WHERE status = 'CLOSED' "
            "ORDER BY id DESC LIMIT 100;"))) {
        return;
    }

    int row = 0;
    while (q.next()) {
        const int sid        = q.value(0).toInt();
        const int term       = q.value(1).toInt();
        const QString ws     = q.value(2).toString();
        const QString opened = q.value(3).toString();
        const QString closed = q.value(4).toString();
        const QString oName  = q.value(5).toString();
        const QString cName  = q.value(6).toString();
        const double expCash = q.value(7).toDouble();
        const double cntCash = q.value(8).toDouble();
        const double diff    = q.value(9).toDouble();

        const QString resp = (oName == cName || cName.isEmpty())
            ? oName
            : QStringLiteral("%1 / %2").arg(oName, cName);

        m_historyTable->insertRow(row);
        m_historyTable->setItem(row, 0, new QTableWidgetItem(QStringLiteral("#%1").arg(sid)));
        m_historyTable->setItem(row, 1, new QTableWidgetItem(QStringLiteral("#%1 (%2)").arg(term).arg(ws)));
        m_historyTable->setItem(row, 2, new QTableWidgetItem(opened));
        m_historyTable->setItem(row, 3, new QTableWidgetItem(closed));
        m_historyTable->setItem(row, 4, new QTableWidgetItem(resp));

        auto *expItem = new QTableWidgetItem(formatMoney(expCash));
        expItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_historyTable->setItem(row, 5, expItem);

        auto *cntItem = new QTableWidgetItem(formatMoney(cntCash));
        cntItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_historyTable->setItem(row, 6, cntItem);

        QString diffTxt = formatMoney(0.0);
        QColor diffCol(QStringLiteral("#2E7D32"));
        if (diff > 0.005) {
            diffTxt = QStringLiteral("+") + formatMoney(diff);
            diffCol = QColor(QStringLiteral("#2B6CB0"));
        } else if (diff < -0.005) {
            diffTxt = QStringLiteral("-") + formatMoney(qAbs(diff));
            diffCol = QColor(QStringLiteral("#C53030"));
        }
        auto *diffItem = new QTableWidgetItem(diffTxt);
        diffItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        diffItem->setForeground(QBrush(diffCol));
        m_historyTable->setItem(row, 7, diffItem);

        auto *btnWidget = new QWidget(m_historyTable);
        auto *btnLay = new QHBoxLayout(btnWidget);
        btnLay->setContentsMargins(4, 2, 4, 2);

        auto *repBtn = new QPushButton(QStringLiteral("Ver Informe"), btnWidget);
        repBtn->setIcon(Icons::get(Icons::Name::Printer, QColor(QStringLiteral("#FFFFFF")), 13));
        connect(repBtn, &QPushButton::clicked, this, [this, sid]() {
            onGenerateHistoricalSessionReport(sid);
        });

        btnLay->addWidget(repBtn);
        m_historyTable->setCellWidget(row, 8, btnWidget);
        ++row;
    }

    m_historyTable->resizeColumnToContents(8);
}

void CashboxModule::onGenerateActiveSessionReport()
{
    if (!requirePermission(QStringLiteral("report"), QStringLiteral("generar informes de caja"))) {
        return;
    }
    if (m_activeSession.id <= 0) return;
    showSessionReport(m_activeSession.id);
}

void CashboxModule::onGenerateHistoricalSessionReport(int sessionId)
{
    if (!requirePermission(QStringLiteral("report"), QStringLiteral("generar informes de cierre de caja"))) {
        return;
    }
    showSessionReport(sessionId);
}

void CashboxModule::showSessionReport(int sessionId)
{
    CashSessionInfo s;
    if (!loadSessionById(sessionId, &s)) return;

    const SessionTotals totals = computeSessionTotals(s.id, s.openingAmount);
    const bool isClosed = (s.status == QStringLiteral("CLOSED"));

    Reporting::ReportPayload payload;
    payload.reportTitle = isClosed
        ? QStringLiteral("Arqueo y Cierre de Caja - Turno #%1").arg(s.id)
        : QStringLiteral("Estado Parcial de Caja - Turno #%1 (Abierto)").arg(s.id);
    payload.reportSubtitle = QStringLiteral("Puesto #%1 (%2) · Apertura: %3%4")
        .arg(s.terminalNumber)
        .arg(s.workstation,
             s.openedAt,
             isClosed ? QStringLiteral(" · Cierre: %1").arg(s.closedAt) : QString());
    payload.generatedBy = Session::instance().currentUser().fullName;

    payload.metadata.append({QStringLiteral("Estado"), isClosed ? QStringLiteral("Cerrada") : QStringLiteral("Abierta")});
    payload.metadata.append({QStringLiteral("Apertura por"), s.openedByName});
    if (isClosed && !s.closedByName.isEmpty()) {
        payload.metadata.append({QStringLiteral("Cierre por"), s.closedByName});
    }

    // Tabla 1: Resumen por Medio de Pago
    Reporting::TableSection methodSec;
    methodSec.title = QStringLiteral("Resumen por Medio de Pago");
    methodSec.columns = {
        {QStringLiteral("Medio de Pago"), QStringLiteral("L"), 0},
        {QStringLiteral("Ingresos"), QStringLiteral("R"), 0},
        {QStringLiteral("Egresos"), QStringLiteral("R"), 0},
        {QStringLiteral("Saldo Neto"), QStringLiteral("R"), 0}
    };
    for (auto it = totals.byMethod.constBegin(); it != totals.byMethod.constEnd(); ++it) {
        methodSec.rows.append(QStringList{
            it.key().toHtmlEscaped(),
            formatMoney(it.value().first),
            formatMoney(it.value().second),
            formatMoney(it.value().first - it.value().second)
        });
    }
    if (!methodSec.rows.isEmpty()) {
        payload.tables.append(methodSec);
    }

    // Tabla 2: Detalle de Movimientos
    Reporting::TableSection movSec;
    movSec.title = QStringLiteral("Detalle de Movimientos del Turno");
    movSec.columns = {
        {QStringLiteral("Fecha / Hora"), QStringLiteral("C"), 0},
        {QStringLiteral("Tipo"), QStringLiteral("C"), 0},
        {QStringLiteral("Medio"), QStringLiteral("L"), 0},
        {QStringLiteral("Concepto / Categoría"), QStringLiteral("L"), 0},
        {QStringLiteral("Importe"), QStringLiteral("R"), 0},
        {QStringLiteral("Estado"), QStringLiteral("C"), 0}
    };

    QSqlQuery q(Database::instance().connection());
    q.prepare(QStringLiteral(
        "SELECT created_at, movement_type, payment_method_name, category, description, reference_code, amount, is_voided "
        "FROM cash_movements WHERE session_id = :sid ORDER BY id ASC;"));
    q.bindValue(QStringLiteral(":sid"), s.id);
    if (q.exec()) {
        while (q.next()) {
            const bool inc    = (q.value(1).toString() == QStringLiteral("INCOME"));
            const bool voided = (q.value(7).toInt() == 1);
            const QString ref = q.value(5).toString();
            QString concepto  = QStringLiteral("%1 · %2").arg(q.value(3).toString(), q.value(4).toString());
            if (!ref.isEmpty()) concepto += QStringLiteral(" [Ref: %1]").arg(ref);

            movSec.rows.append(QStringList{
                q.value(0).toString().toHtmlEscaped(),
                inc ? QStringLiteral("Ingreso") : QStringLiteral("Egreso"),
                q.value(2).toString().toHtmlEscaped(),
                concepto.toHtmlEscaped(),
                (inc ? QStringLiteral("+ ") : QStringLiteral("- ")) + formatMoney(q.value(6).toDouble()),
                voided ? QStringLiteral("Anulado") : QStringLiteral("Vigente")
            });
        }
    }
    if (!movSec.rows.isEmpty()) {
        payload.tables.append(movSec);
    }

    payload.totals.append({QStringLiteral("Fondo Inicial"), formatMoney(s.openingAmount)});
    payload.totals.append({QStringLiteral("Total Ingresos (todos los medios)"), formatMoney(totals.totalIncome)});
    payload.totals.append({QStringLiteral("Total Egresos (todos los medios)"), formatMoney(totals.totalExpense)});
    payload.totals.append({QStringLiteral("Efectivo Teórico en Cajón"), formatMoney(totals.expectedCashInDrawer)});
    if (isClosed) {
        payload.totals.append({QStringLiteral("Efectivo Físico Contado"), formatMoney(s.countedCash)});
        payload.totals.append({QStringLiteral("Diferencia de Arqueo"), formatMoney(s.differenceAmount)});
    }
    payload.footerNotes = s.notes.isEmpty()
        ? QStringLiteral("Los movimientos marcados como Anulados están excluidos de los totales.")
        : s.notes;

    AuditEvent ev;
    ev.moduleId = moduleId();
    ev.targetTable = QStringLiteral("cash_sessions");
    ev.recordId = QString::number(s.id);
    ev.action = QStringLiteral("REPORT");
    ev.details = QStringLiteral("Informe de caja generado para Turno #%1").arg(s.id);
    Database::instance().logAuditEvent(ev);

    ReportViewerDialog::showReport(payload, this);
}
