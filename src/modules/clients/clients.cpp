#include "clients.h"
#include "core/db.h"
#include "core/icons.h"
#include "core/reportpayload.h"
#include "core/reportviewerdialog.h"
#include "core/session.h"
#include "core/userCredentials.h"

#include <QAbstractItemView>
#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDateTime>
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

constexpr int kVerticalBreakpointPx = 860;

QString compactJson(const QJsonObject &obj)
{
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

QString formatMoney(double amount)
{
    const QString sym = Database::instance().getSetting(
        QStringLiteral("regional.currency_symbol"),
        QStringLiteral("$"));
    return QStringLiteral("%1 %2").arg(sym, QString::number(amount, 'f', 2));
}

} // namespace

ClientesModule::ClientesModule(QWidget *parent)
    : ModuleBase(parent)
{
    initBanner(
        entityPlural(),
        isClinicMode()
            ? QStringLiteral("Padrón de pacientes, cobertura médica, datos de contacto e informes individuales.")
            : QStringLiteral("Padrón de clientes, datos fiscales, cuenta corriente e informes corporativos."));

    setupUi();
    applyBusinessModeLabels();
    loadSupportCombos();
    reloadTable();
}

bool ClientesModule::isClinicMode() const
{
    const QString bType = Database::instance().getSetting(
        QStringLiteral("company.business_type"),
        QStringLiteral("Comercio"));
    return bType.contains(QStringLiteral("Consultorio"), Qt::CaseInsensitive);
}

QString ClientesModule::entitySingular() const
{
    return isClinicMode() ? QStringLiteral("Paciente") : QStringLiteral("Cliente");
}

QString ClientesModule::entityPlural() const
{
    return isClinicMode() ? QStringLiteral("Pacientes") : QStringLiteral("Clientes");
}

QString ClientesModule::moduleTitle() const
{
    return entityPlural();
}

void ClientesModule::setupUi()
{
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);

    // =========================================================================
    // Panel izquierdo: Formulario de Alta / Edición (dentro de QScrollArea)
    // =========================================================================
    auto *formScroll = new QScrollArea(m_splitter);
    formScroll->setWidgetResizable(true);
    formScroll->setFrameShape(QFrame::NoFrame);
    formScroll->setMinimumWidth(310);
    formScroll->setMaximumWidth(440);

    auto *formPage = new QWidget(formScroll);
    auto *formOuterLayout = new QVBoxLayout(formPage);
    formOuterLayout->setContentsMargins(0, 0, 6, 0);
    formOuterLayout->setSpacing(10);

    m_formGroup = new QGroupBox(QStringLiteral("Alta / Edición"), formPage);
    auto *formGroupLayout = new QVBoxLayout(m_formGroup);
    formGroupLayout->setSpacing(10);

    auto *form = new QFormLayout();
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_fullNameLabel = new QLabel(m_formGroup);
    m_fullNameEdit = new QLineEdit(m_formGroup);

    m_tradeNameLabel = new QLabel(m_formGroup);
    m_tradeNameEdit = new QLineEdit(m_formGroup);

    m_docTypeCombo = new QComboBox(m_formGroup);
    m_docNumberEdit = new QLineEdit(m_formGroup);
    m_docNumberEdit->setPlaceholderText(QStringLiteral("Ej: 30123456 o 20-30123456-7"));

    m_ivaCombo = new QComboBox(m_formGroup);

    m_phoneEdit = new QLineEdit(m_formGroup);
    m_phoneEdit->setPlaceholderText(QStringLiteral("Teléfono / WhatsApp"));

    m_emailEdit = new QLineEdit(m_formGroup);
    m_emailEdit->setPlaceholderText(QStringLiteral("correo@ejemplo.com (para envío de informes)"));

    m_addressEdit = new QLineEdit(m_formGroup);
    m_addressEdit->setPlaceholderText(QStringLiteral("Calle, altura, localidad"));

    // Campos de Consultorio (Pacientes)
    m_birthDateLabel = new QLabel(QStringLiteral("Fecha de nacimiento:"), m_formGroup);
    m_birthDateContainer = new QWidget(m_formGroup);
    auto *birthRow = new QHBoxLayout(m_birthDateContainer);
    birthRow->setContentsMargins(0, 0, 0, 0);
    birthRow->setSpacing(6);

    m_hasBirthDateCheck = new QCheckBox(QStringLiteral("Indicar"), m_birthDateContainer);
    m_birthDateEdit = new QDateEdit(QDate(1990, 1, 1), m_birthDateContainer);
    m_birthDateEdit->setCalendarPopup(true);
    m_birthDateEdit->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));
    m_birthDateEdit->setEnabled(false);
    birthRow->addWidget(m_hasBirthDateCheck);
    birthRow->addWidget(m_birthDateEdit, 1);

    m_insuranceLabel = new QLabel(QStringLiteral("Obra Social / Prepaga:"), m_formGroup);
    m_insuranceEdit = new QLineEdit(m_formGroup);
    m_insuranceEdit->setPlaceholderText(QStringLiteral("Ej: OSDE, Swiss Medical, Particular"));

    m_affiliateLabel = new QLabel(QStringLiteral("Nro. de Afiliado:"), m_formGroup);
    m_affiliateEdit = new QLineEdit(m_formGroup);
    m_affiliateEdit->setPlaceholderText(QStringLiteral("Credencial / Número de socio"));

    // Campos de Comercio (Cuenta Corriente)
    m_creditLimitLabel = new QLabel(QStringLiteral("Límite Cta. Cte.:"), m_formGroup);
    m_creditLimitSpin = new QDoubleSpinBox(m_formGroup);
    m_creditLimitSpin->setRange(0.0, 999999999.99);
    m_creditLimitSpin->setDecimals(2);
    m_creditLimitSpin->setPrefix(QStringLiteral("$ "));

    m_notesEdit = new QPlainTextEdit(m_formGroup);
    m_notesEdit->setMaximumHeight(75);
    m_notesEdit->setPlaceholderText(QStringLiteral("Observaciones internas, alergias, referencias comerciales..."));

    form->addRow(m_fullNameLabel, m_fullNameEdit);
    form->addRow(m_tradeNameLabel, m_tradeNameEdit);
    form->addRow(QStringLiteral("Tipo de Documento:"), m_docTypeCombo);
    form->addRow(QStringLiteral("Nro. de Documento:"), m_docNumberEdit);
    form->addRow(QStringLiteral("Condición IVA:"), m_ivaCombo);
    form->addRow(QStringLiteral("Teléfono / WhatsApp:"), m_phoneEdit);
    form->addRow(QStringLiteral("Email:"), m_emailEdit);
    form->addRow(QStringLiteral("Domicilio:"), m_addressEdit);
    form->addRow(m_birthDateLabel, m_birthDateContainer);
    form->addRow(m_insuranceLabel, m_insuranceEdit);
    form->addRow(m_affiliateLabel, m_affiliateEdit);
    form->addRow(m_creditLimitLabel, m_creditLimitSpin);
    form->addRow(QStringLiteral("Observaciones:"), m_notesEdit);

    formGroupLayout->addLayout(form);

    auto *formBtnRow = new QHBoxLayout();
    m_saveBtn = new QPushButton(QStringLiteral("Guardar"), m_formGroup);
    m_saveBtn->setIcon(Icons::get(Icons::Name::Add, QColor(QStringLiteral("#FFFFFF")), 15));

    m_clearBtn = new QPushButton(QStringLiteral("Limpiar"), m_formGroup);
    m_clearBtn->setIcon(Icons::get(Icons::Name::Refresh, QColor(QStringLiteral("#1E3A5F")), 15));
    m_clearBtn->setProperty("secondary", true);

    formBtnRow->addWidget(m_saveBtn);
    formBtnRow->addWidget(m_clearBtn);
    formBtnRow->addStretch();
    formGroupLayout->addLayout(formBtnRow);

    formOuterLayout->addWidget(m_formGroup);
    formOuterLayout->addStretch();
    formScroll->setWidget(formPage);
    m_splitter->addWidget(formScroll);

    // =========================================================================
    // Panel derecho: Buscador, botón Generar Informe y Grilla de registros
    // =========================================================================
    m_listGroup = new QGroupBox(QStringLiteral("Listado"), m_splitter);
    auto *listLayout = new QVBoxLayout(m_listGroup);
    listLayout->setSpacing(10);

    auto *toolbar = new QHBoxLayout();
    toolbar->setSpacing(8);

    m_searchEdit = new QLineEdit(m_listGroup);
    m_searchEdit->setPlaceholderText(QStringLiteral("Buscar por nombre, documento, email, teléfono..."));
    m_searchEdit->setClearButtonEnabled(true);

    m_showDeprecatedCheck = new QCheckBox(QStringLiteral("Mostrar deprecados"), m_listGroup);
    m_showDeprecatedCheck->setChecked(false);

    m_reportListBtn = new QPushButton(QStringLiteral("Generar Informe"), m_listGroup);
    m_reportListBtn->setIcon(Icons::get(Icons::Name::Printer, QColor(QStringLiteral("#FFFFFF")), 15));

    auto *refreshBtn = new QPushButton(QStringLiteral("Actualizar"), m_listGroup);
    refreshBtn->setIcon(Icons::get(Icons::Name::Refresh, QColor(QStringLiteral("#1E3A5F")), 15));
    refreshBtn->setProperty("secondary", true);

    toolbar->addWidget(m_searchEdit, 1);
    toolbar->addWidget(m_showDeprecatedCheck);
    toolbar->addWidget(m_reportListBtn);
    toolbar->addWidget(refreshBtn);
    listLayout->addLayout(toolbar);

    m_summaryLabel = new QLabel(m_listGroup);
    m_summaryLabel->setStyleSheet(QStringLiteral("color: #52606D; font-size: 9pt;"));
    listLayout->addWidget(m_summaryLabel);

    m_table = new QTableWidget(m_listGroup);
    m_table->setColumnCount(7);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(42);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    listLayout->addWidget(m_table, 1);

    m_splitter->addWidget(m_listGroup);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);

    bodyLayout()->addWidget(m_splitter, 1);

    // Conexiones
    connect(m_hasBirthDateCheck, &QCheckBox::toggled, m_birthDateEdit, &QDateEdit::setEnabled);
    connect(m_saveBtn, &QPushButton::clicked, this, &ClientesModule::onSaveClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &ClientesModule::onResetForm);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ClientesModule::onFilterChanged);
    connect(m_showDeprecatedCheck, &QCheckBox::toggled, this, &ClientesModule::onFilterChanged);
    connect(m_reportListBtn, &QPushButton::clicked, this, &ClientesModule::onGenerateListReport);
    connect(refreshBtn, &QPushButton::clicked, this, [this]() {
        loadSupportCombos();
        reloadTable();
    });
}

void ClientesModule::resizeEvent(QResizeEvent *event)
{
    ModuleBase::resizeEvent(event);
    const Qt::Orientation target =
        (event->size().width() < kVerticalBreakpointPx) ? Qt::Vertical : Qt::Horizontal;
    if (m_splitter->orientation() != target) {
        m_splitter->setOrientation(target);
    }
}

void ClientesModule::applyBusinessModeLabels()
{
    const bool clinic = isClinicMode();
    const QString singular = entitySingular();
    const QString plural = entityPlural();
    const QString sym = Database::instance().getSetting(
        QStringLiteral("regional.currency_symbol"),
        QStringLiteral("$"));

    m_formGroup->setTitle(QStringLiteral("Alta / Edición de %1").arg(singular));
    m_listGroup->setTitle(QStringLiteral("Padrón de %1").arg(plural));

    m_fullNameLabel->setText(clinic ? QStringLiteral("Apellido y Nombre *:") : QStringLiteral("Razón Social / Nombre *:"));
    m_fullNameEdit->setPlaceholderText(clinic ? QStringLiteral("Ej: Pérez, Juan Carlos") : QStringLiteral("Nombre completo o Razón Social"));

    m_tradeNameLabel->setText(QStringLiteral("Nombre de Fantasía:"));
    m_tradeNameEdit->setPlaceholderText(QStringLiteral("Opcional (comercio / empresa)"));
    m_tradeNameLabel->setVisible(!clinic);
    m_tradeNameEdit->setVisible(!clinic);

    m_birthDateLabel->setVisible(clinic);
    m_birthDateContainer->setVisible(clinic);
    m_insuranceLabel->setVisible(clinic);
    m_insuranceEdit->setVisible(clinic);
    m_affiliateLabel->setVisible(clinic);
    m_affiliateEdit->setVisible(clinic);

    m_creditLimitLabel->setVisible(!clinic);
    m_creditLimitSpin->setVisible(!clinic);
    m_creditLimitSpin->setPrefix(sym + QLatin1Char(' '));

    m_table->setHorizontalHeaderLabels(QStringList{
        clinic ? QStringLiteral("Paciente") : QStringLiteral("Cliente / Razón Social"),
        QStringLiteral("Documento"),
        QStringLiteral("Teléfono"),
        QStringLiteral("Email"),
        clinic ? QStringLiteral("Cobertura / Afiliado") : QStringLiteral("IVA / Saldo Cta. Cte."),
        QStringLiteral("Estado"),
        QStringLiteral("Acciones")
    });

    if (m_editingClientId < 0) {
        m_saveBtn->setText(QStringLiteral("Agregar %1").arg(singular.toLower()));
    }
}

void ClientesModule::onActivated()
{
    applyBusinessModeLabels();
    loadSupportCombos();
    reloadTable();

    AuditEvent ev;
    ev.moduleId = moduleId();
    ev.targetTable = QStringLiteral("clients");
    ev.action = QStringLiteral("VIEW");
    ev.details = QStringLiteral("Acceso al padrón de %1").arg(entityPlural());
    Database::instance().logAuditEvent(ev);
}

void ClientesModule::loadSupportCombos()
{
    const QVariant prevDocId = m_docTypeCombo->currentData();
    const QVariant prevIvaId = m_ivaCombo->currentData();

    m_docTypeCombo->clear();
    m_docTypeCombo->addItem(QStringLiteral("(Sin especificar)"), -1);
    {
        QSqlQuery q(Database::instance().connection());
        if (q.exec(QStringLiteral(
                "SELECT id, code, name FROM document_types "
                "WHERE is_active = 1 AND is_deprecated = 0 ORDER BY code ASC;"))) {
            while (q.next()) {
                const int id = q.value(0).toInt();
                const QString code = q.value(1).toString();
                const QString name = q.value(2).toString();
                m_docTypeCombo->addItem(QStringLiteral("%1 - %2").arg(code, name), id);
                m_docTypeCombo->setItemData(m_docTypeCombo->count() - 1, code, Qt::UserRole + 1);
            }
        }
    }

    m_ivaCombo->clear();
    m_ivaCombo->addItem(QStringLiteral("(Sin especificar)"), -1);
    {
        QSqlQuery q(Database::instance().connection());
        if (q.exec(QStringLiteral(
                "SELECT id, name FROM iva_conditions "
                "WHERE is_active = 1 AND is_deprecated = 0 ORDER BY name ASC;"))) {
            while (q.next()) {
                m_ivaCombo->addItem(q.value(1).toString(), q.value(0).toInt());
            }
        }
    }

    if (prevDocId.isValid()) {
        const int idx = m_docTypeCombo->findData(prevDocId);
        if (idx >= 0) m_docTypeCombo->setCurrentIndex(idx);
    }
    if (prevIvaId.isValid()) {
        const int idx = m_ivaCombo->findData(prevIvaId);
        if (idx >= 0) m_ivaCombo->setCurrentIndex(idx);
    }
}

void ClientesModule::onFilterChanged()
{
    reloadTable();
}

void ClientesModule::reloadTable()
{
    m_table->setRowCount(0);
    m_visibleRows.clear();

    const QString search = m_searchEdit->text().trimmed();
    const bool showDep = m_showDeprecatedCheck->isChecked();
    const bool clinic = isClinicMode();

    QString sql = QStringLiteral(
        "SELECT c.id, c.full_name, c.trade_name, c.document_type_id, "
        "       COALESCE(dt.code, ''), c.document_number, "
        "       c.iva_condition_id, COALESCE(iva.name, ''), "
        "       c.phone, c.email, c.address, c.birth_date, "
        "       c.health_insurance, c.affiliate_number, "
        "       c.credit_limit, c.current_balance, c.notes, "
        "       c.is_deprecated, c.deprecation_note, c.created_at "
        "FROM clients c "
        "LEFT JOIN document_types dt ON dt.id = c.document_type_id "
        "LEFT JOIN iva_conditions iva ON iva.id = c.iva_condition_id "
        "WHERE 1 = 1 ");

    if (!showDep) {
        sql += QStringLiteral("AND c.is_deprecated = 0 ");
    }

    if (!search.isEmpty()) {
        sql += QStringLiteral(
            "AND (LOWER(c.full_name) LIKE :q "
            "  OR LOWER(c.trade_name) LIKE :q "
            "  OR LOWER(c.document_number) LIKE :q "
            "  OR LOWER(c.email) LIKE :q "
            "  OR LOWER(c.phone) LIKE :q "
            "  OR LOWER(c.health_insurance) LIKE :q) ");
    }

    sql += QStringLiteral("ORDER BY c.is_deprecated ASC, c.full_name ASC;");

    QSqlQuery q(Database::instance().connection());
    q.prepare(sql);
    if (!search.isEmpty()) {
        q.bindValue(QStringLiteral(":q"), QStringLiteral("%%1%").arg(search.toLower()));
    }

    if (!q.exec()) {
        return;
    }

    int activeCount = 0;
    int depCount = 0;
    int rowIdx = 0;

    while (q.next()) {
        ClientRow r;
        r.id               = q.value(0).toInt();
        r.fullName         = q.value(1).toString();
        r.tradeName        = q.value(2).toString();
        r.documentTypeId   = q.value(3).isNull() ? -1 : q.value(3).toInt();
        r.documentTypeCode = q.value(4).toString();
        r.documentNumber   = q.value(5).toString();
        r.ivaConditionId   = q.value(6).isNull() ? -1 : q.value(6).toInt();
        r.ivaConditionName = q.value(7).toString();
        r.phone            = q.value(8).toString();
        r.email            = q.value(9).toString();
        r.address          = q.value(10).toString();
        r.birthDate        = q.value(11).toString();
        r.healthInsurance  = q.value(12).toString();
        r.affiliateNumber  = q.value(13).toString();
        r.creditLimit      = q.value(14).toDouble();
        r.currentBalance   = q.value(15).toDouble();
        r.notes            = q.value(16).toString();
        r.isDeprecated     = (q.value(17).toInt() == 1);
        r.deprecationNote  = q.value(18).toString();
        r.createdAt        = q.value(19).toString();

        m_visibleRows.append(r);
        if (r.isDeprecated) {
            ++depCount;
        } else {
            ++activeCount;
        }

        const QString nameDisplay = (!clinic && !r.tradeName.isEmpty())
            ? QStringLiteral("%1 (%2)").arg(r.fullName, r.tradeName)
            : r.fullName;

        const QString docDisplay = r.documentNumber.isEmpty()
            ? QStringLiteral("-")
            : (r.documentTypeCode.isEmpty()
                   ? r.documentNumber
                   : QStringLiteral("%1 %2").arg(r.documentTypeCode, r.documentNumber));

        QString extraCol;
        if (clinic) {
            extraCol = r.healthInsurance.isEmpty()
                ? QStringLiteral("Particular")
                : (r.affiliateNumber.isEmpty()
                       ? r.healthInsurance
                       : QStringLiteral("%1 · #%2").arg(r.healthInsurance, r.affiliateNumber));
        } else {
            const QString ivaTxt = r.ivaConditionName.isEmpty() ? QStringLiteral("-") : r.ivaConditionName;
            extraCol = QStringLiteral("%1 · Saldo: %2").arg(ivaTxt, formatMoney(r.currentBalance));
        }

        m_table->insertRow(rowIdx);
        m_table->setItem(rowIdx, 0, new QTableWidgetItem(r.isDeprecated ? nameDisplay + QStringLiteral(" [Deprecado]") : nameDisplay));
        m_table->setItem(rowIdx, 1, new QTableWidgetItem(docDisplay));
        m_table->setItem(rowIdx, 2, new QTableWidgetItem(r.phone.isEmpty() ? QStringLiteral("-") : r.phone));
        m_table->setItem(rowIdx, 3, new QTableWidgetItem(r.email.isEmpty() ? QStringLiteral("-") : r.email));
        m_table->setItem(rowIdx, 4, new QTableWidgetItem(extraCol));

        auto *statusItem = new QTableWidgetItem(r.isDeprecated ? QStringLiteral("Deprecado") : QStringLiteral("Activo"));
        statusItem->setForeground(QBrush(QColor(r.isDeprecated ? QStringLiteral("#B7791F") : QStringLiteral("#2E7D32"))));
        m_table->setItem(rowIdx, 5, statusItem);

        if (r.isDeprecated) {
            const QBrush depBg(QColor(QStringLiteral("#FFFBEB")));
            const QBrush depFg(QColor(QStringLiteral("#744210")));
            for (int c = 0; c < 6; ++c) {
                if (QTableWidgetItem *it = m_table->item(rowIdx, c)) {
                    it->setBackground(depBg);
                    if (c != 5) it->setForeground(depFg);
                }
            }
        }

        // Botones incrustados por fila: Editar | Informe | Deprecar/Reactivar
        auto *actWidget = new QWidget(m_table);
        auto *actLayout = new QHBoxLayout(actWidget);
        actLayout->setContentsMargins(4, 3, 4, 3);
        actLayout->setSpacing(6);

        auto *editBtn = new QPushButton(QStringLiteral("Editar"), actWidget);
        editBtn->setIcon(Icons::get(Icons::Name::Edit, QColor(QStringLiteral("#1E3A5F")), 14));
        editBtn->setProperty("secondary", true);

        auto *repBtn = new QPushButton(QStringLiteral("Informe"), actWidget);
        repBtn->setIcon(Icons::get(Icons::Name::Printer, QColor(QStringLiteral("#FFFFFF")), 14));

        actLayout->addWidget(editBtn);
        actLayout->addWidget(repBtn);

        const int idCopy = r.id;
        const QString nameCopy = r.fullName;

        connect(editBtn, &QPushButton::clicked, this, [this, idCopy]() {
            onEditClient(idCopy);
        });
        connect(repBtn, &QPushButton::clicked, this, [this, idCopy]() {
            onGenerateSingleReport(idCopy);
        });

        if (!r.isDeprecated) {
            auto *depBtn = new QPushButton(QStringLiteral("Deprecar"), actWidget);
            depBtn->setIcon(Icons::get(Icons::Name::Lock, QColor(QStringLiteral("#FFFFFF")), 14));
            depBtn->setProperty("danger", true);
            connect(depBtn, &QPushButton::clicked, this, [this, idCopy, nameCopy]() {
                onDeprecateClient(idCopy, nameCopy);
            });
            actLayout->addWidget(depBtn);
        } else {
            auto *reactBtn = new QPushButton(QStringLiteral("Reactivar"), actWidget);
            reactBtn->setIcon(Icons::get(Icons::Name::Unlock, QColor(QStringLiteral("#1E3A5F")), 14));
            reactBtn->setProperty("secondary", true);
            connect(reactBtn, &QPushButton::clicked, this, [this, idCopy, nameCopy]() {
                onReactivateClient(idCopy, nameCopy);
            });
            actLayout->addWidget(reactBtn);
        }

        m_table->setCellWidget(rowIdx, 6, actWidget);
        ++rowIdx;
    }

    m_table->resizeColumnToContents(6);
    m_summaryLabel->setText(
        QStringLiteral("Mostrando %1 registro(s) · Activos: %2 · Deprecados: %3")
            .arg(m_visibleRows.size())
            .arg(activeCount)
            .arg(depCount));
}

bool ClientesModule::loadClientById(int clientId, ClientRow *out) const
{
    if (clientId <= 0 || !out) return false;

    QSqlQuery q(Database::instance().connection());
    q.prepare(QStringLiteral(
        "SELECT c.id, c.full_name, c.trade_name, c.document_type_id, "
        "       COALESCE(dt.code, ''), c.document_number, "
        "       c.iva_condition_id, COALESCE(iva.name, ''), "
        "       c.phone, c.email, c.address, c.birth_date, "
        "       c.health_insurance, c.affiliate_number, "
        "       c.credit_limit, c.current_balance, c.notes, "
        "       c.is_deprecated, c.deprecation_note, c.created_at "
        "FROM clients c "
        "LEFT JOIN document_types dt ON dt.id = c.document_type_id "
        "LEFT JOIN iva_conditions iva ON iva.id = c.iva_condition_id "
        "WHERE c.id = :id LIMIT 1;"));
    q.bindValue(QStringLiteral(":id"), clientId);

    if (!q.exec() || !q.next()) return false;

    out->id               = q.value(0).toInt();
    out->fullName         = q.value(1).toString();
    out->tradeName        = q.value(2).toString();
    out->documentTypeId   = q.value(3).isNull() ? -1 : q.value(3).toInt();
    out->documentTypeCode = q.value(4).toString();
    out->documentNumber   = q.value(5).toString();
    out->ivaConditionId   = q.value(6).isNull() ? -1 : q.value(6).toInt();
    out->ivaConditionName = q.value(7).toString();
    out->phone            = q.value(8).toString();
    out->email            = q.value(9).toString();
    out->address          = q.value(10).toString();
    out->birthDate        = q.value(11).toString();
    out->healthInsurance  = q.value(12).toString();
    out->affiliateNumber  = q.value(13).toString();
    out->creditLimit      = q.value(14).toDouble();
    out->currentBalance   = q.value(15).toDouble();
    out->notes            = q.value(16).toString();
    out->isDeprecated     = (q.value(17).toInt() == 1);
    out->deprecationNote  = q.value(18).toString();
    out->createdAt        = q.value(19).toString();
    return true;
}

bool ClientesModule::isValidCuitCuil(const QString &input, QString *formattedOut)
{
    QString digits;
    for (const QChar &ch : input) {
        if (ch.isDigit()) digits.append(ch);
    }
    if (digits.size() != 11) return false;

    static const int weights[10] = {5, 4, 3, 2, 7, 6, 5, 4, 3, 2};
    int sum = 0;
    for (int i = 0; i < 10; ++i) {
        sum += digits.at(i).digitValue() * weights[i];
    }

    int check = 11 - (sum % 11);
    if (check == 11) check = 0;
    else if (check == 10) check = 9;

    if (check != digits.at(10).digitValue()) return false;

    if (formattedOut) {
        *formattedOut = QStringLiteral("%1-%2-%3")
                            .arg(digits.left(2), digits.mid(2, 8), digits.right(1));
    }
    return true;
}

void ClientesModule::onSaveClicked()
{
    const bool isCreate = (m_editingClientId < 0);
    if (!requirePermission(
            isCreate ? QStringLiteral("create") : QStringLiteral("edit"),
            isCreate ? QStringLiteral("dar de alta %1").arg(entityPlural().toLower())
                     : QStringLiteral("modificar datos de %1").arg(entityPlural().toLower()))) {
        return;
    }

    const QString fullName  = m_fullNameEdit->text().trimmed();
    const QString tradeName = m_tradeNameEdit->text().trimmed();
    const int docTypeId     = m_docTypeCombo->currentData().toInt();
    const QString docCode   = m_docTypeCombo->currentData(Qt::UserRole + 1).toString().toUpper();
    QString docNumber       = m_docNumberEdit->text().trimmed();
    const int ivaId         = m_ivaCombo->currentData().toInt();
    const QString phone     = m_phoneEdit->text().trimmed();
    const QString email     = m_emailEdit->text().trimmed().toLower();
    const QString address   = m_addressEdit->text().trimmed();
    const QString birthDate = m_hasBirthDateCheck->isChecked()
                                  ? m_birthDateEdit->date().toString(QStringLiteral("yyyy-MM-dd"))
                                  : QString();
    const QString insurance = m_insuranceEdit->text().trimmed();
    const QString affiliate = m_affiliateEdit->text().trimmed();
    const double creditLim  = m_creditLimitSpin->value();
    const QString notes     = m_notesEdit->toPlainText().trimmed();

    if (fullName.isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("Dato requerido"),
            QStringLiteral("Ingresa el nombre o razón social del %1.").arg(entitySingular().toLower()));
        m_fullNameEdit->setFocus();
        return;
    }

    if (!docNumber.isEmpty() && (docCode == QStringLiteral("CUIT") || docCode == QStringLiteral("CUIL"))) {
        QString formatted;
        if (!isValidCuitCuil(docNumber, &formatted)) {
            QMessageBox::warning(
                this,
                QStringLiteral("%1 inválido").arg(docCode),
                QStringLiteral("El número de %1 ingresado no supera la validación de dígito verificador.").arg(docCode));
            m_docNumberEdit->setFocus();
            return;
        }
        docNumber = formatted;
        m_docNumberEdit->setText(docNumber);
    }

    if (!email.isEmpty() && !UserCredentials::isValidEmail(email)) {
        QMessageBox::warning(
            this,
            QStringLiteral("Email inválido"),
            QStringLiteral("El formato del correo electrónico no es válido."));
        m_emailEdit->setFocus();
        return;
    }

    // Evitar duplicados de mismo tipo + número de documento
    if (!docNumber.isEmpty()) {
        QSqlQuery dupQ(Database::instance().connection());
        dupQ.prepare(QStringLiteral(
            "SELECT id, full_name FROM clients "
            "WHERE document_number = :dnum AND id <> :id LIMIT 1;"));
        dupQ.bindValue(QStringLiteral(":dnum"), docNumber);
        dupQ.bindValue(QStringLiteral(":id"), m_editingClientId);
        if (dupQ.exec() && dupQ.next()) {
            QMessageBox::warning(
                this,
                QStringLiteral("Documento duplicado"),
                QStringLiteral("Ya existe un registro con el documento \"%1\":\n%2")
                    .arg(docNumber, dupQ.value(1).toString()));
            m_docNumberEdit->setFocus();
            return;
        }
    }

    QJsonObject oldJson;
    if (!isCreate) {
        ClientRow prev;
        if (loadClientById(m_editingClientId, &prev)) {
            oldJson.insert(QStringLiteral("full_name"), prev.fullName);
            oldJson.insert(QStringLiteral("document_number"), prev.documentNumber);
            oldJson.insert(QStringLiteral("email"), prev.email);
            oldJson.insert(QStringLiteral("phone"), prev.phone);
        }
    }

    QSqlQuery q(Database::instance().connection());
    if (isCreate) {
        q.prepare(QStringLiteral(
            "INSERT INTO clients ("
            "  full_name, trade_name, document_type_id, document_number, iva_condition_id, "
            "  phone, email, address, birth_date, health_insurance, affiliate_number, "
            "  credit_limit, notes, is_active, is_deprecated"
            ") VALUES ("
            "  :fname, :tname, :dtid, :dnum, :ivaid, "
            "  :phone, :email, :addr, :bdate, :ins, :aff, "
            "  :clim, :notes, 1, 0"
            ");"));
    } else {
        q.prepare(QStringLiteral(
            "UPDATE clients SET "
            "  full_name = :fname, trade_name = :tname, document_type_id = :dtid, "
            "  document_number = :dnum, iva_condition_id = :ivaid, "
            "  phone = :phone, email = :email, address = :addr, "
            "  birth_date = :bdate, health_insurance = :ins, affiliate_number = :aff, "
            "  credit_limit = :clim, notes = :notes, updated_at = CURRENT_TIMESTAMP "
            "WHERE id = :id;"));
        q.bindValue(QStringLiteral(":id"), m_editingClientId);
    }

    q.bindValue(QStringLiteral(":fname"), fullName);
    q.bindValue(QStringLiteral(":tname"), tradeName);
    q.bindValue(QStringLiteral(":dtid"),  docTypeId > 0 ? QVariant(docTypeId) : QVariant());
    q.bindValue(QStringLiteral(":dnum"),  docNumber);
    q.bindValue(QStringLiteral(":ivaid"), ivaId > 0 ? QVariant(ivaId) : QVariant());
    q.bindValue(QStringLiteral(":phone"), phone);
    q.bindValue(QStringLiteral(":email"), email);
    q.bindValue(QStringLiteral(":addr"),  address);
    q.bindValue(QStringLiteral(":bdate"), birthDate);
    q.bindValue(QStringLiteral(":ins"),   insurance);
    q.bindValue(QStringLiteral(":aff"),   affiliate);
    q.bindValue(QStringLiteral(":clim"),  creditLim);
    q.bindValue(QStringLiteral(":notes"), notes);

    if (!q.exec()) {
        QMessageBox::warning(
            this,
            QStringLiteral("Error al guardar"),
            QStringLiteral("No se pudo guardar el registro:\n%1").arg(q.lastError().text()));
        return;
    }

    const int finalId = isCreate ? q.lastInsertId().toInt() : m_editingClientId;

    QJsonObject newJson;
    newJson.insert(QStringLiteral("full_name"), fullName);
    newJson.insert(QStringLiteral("document_number"), docNumber);
    newJson.insert(QStringLiteral("email"), email);
    newJson.insert(QStringLiteral("phone"), phone);

    AuditEvent ev;
    ev.moduleId = moduleId();
    ev.targetTable = QStringLiteral("clients");
    ev.recordId = QString::number(finalId);
    ev.action = isCreate ? QStringLiteral("CREATE") : QStringLiteral("UPDATE");
    ev.oldValuesJson = isCreate ? QString() : compactJson(oldJson);
    ev.newValuesJson = compactJson(newJson);
    ev.details = QStringLiteral("%1: %2 (%3)").arg(entitySingular(), fullName, docNumber);
    Database::instance().logAuditEvent(ev);

    onResetForm();
    reloadTable();
}

void ClientesModule::onResetForm()
{
    m_editingClientId = -1;
    m_fullNameEdit->clear();
    m_tradeNameEdit->clear();
    m_docTypeCombo->setCurrentIndex(0);
    m_docNumberEdit->clear();
    m_ivaCombo->setCurrentIndex(0);
    m_phoneEdit->clear();
    m_emailEdit->clear();
    m_addressEdit->clear();
    m_hasBirthDateCheck->setChecked(false);
    m_birthDateEdit->setDate(QDate(1990, 1, 1));
    m_insuranceEdit->clear();
    m_affiliateEdit->clear();
    m_creditLimitSpin->setValue(0.0);
    m_notesEdit->clear();

    m_saveBtn->setText(QStringLiteral("Agregar %1").arg(entitySingular().toLower()));
    m_saveBtn->setIcon(Icons::get(Icons::Name::Add, QColor(QStringLiteral("#FFFFFF")), 15));
}

void ClientesModule::onEditClient(int clientId)
{
    if (!requirePermission(QStringLiteral("edit"), QStringLiteral("editar %1").arg(entityPlural().toLower()))) {
        return;
    }

    ClientRow r;
    if (!loadClientById(clientId, &r)) {
        return;
    }

    loadSupportCombos();

    m_editingClientId = r.id;
    m_fullNameEdit->setText(r.fullName);
    m_tradeNameEdit->setText(r.tradeName);

    int dtIdx = m_docTypeCombo->findData(r.documentTypeId);
    if (dtIdx < 0 && r.documentTypeId > 0) {
        m_docTypeCombo->addItem(r.documentTypeCode + QStringLiteral(" [Deprecado]"), r.documentTypeId);
        dtIdx = m_docTypeCombo->count() - 1;
    }
    m_docTypeCombo->setCurrentIndex(dtIdx >= 0 ? dtIdx : 0);

    m_docNumberEdit->setText(r.documentNumber);

    int ivaIdx = m_ivaCombo->findData(r.ivaConditionId);
    if (ivaIdx < 0 && r.ivaConditionId > 0) {
        m_ivaCombo->addItem(r.ivaConditionName + QStringLiteral(" [Deprecado]"), r.ivaConditionId);
        ivaIdx = m_ivaCombo->count() - 1;
    }
    m_ivaCombo->setCurrentIndex(ivaIdx >= 0 ? ivaIdx : 0);

    m_phoneEdit->setText(r.phone);
    m_emailEdit->setText(r.email);
    m_addressEdit->setText(r.address);

    if (!r.birthDate.isEmpty()) {
        const QDate d = QDate::fromString(r.birthDate, QStringLiteral("yyyy-MM-dd"));
        if (d.isValid()) {
            m_hasBirthDateCheck->setChecked(true);
            m_birthDateEdit->setDate(d);
        }
    } else {
        m_hasBirthDateCheck->setChecked(false);
    }

    m_insuranceEdit->setText(r.healthInsurance);
    m_affiliateEdit->setText(r.affiliateNumber);
    m_creditLimitSpin->setValue(r.creditLimit);
    m_notesEdit->setPlainText(r.notes);

    m_saveBtn->setText(QStringLiteral("Guardar cambios"));
    m_saveBtn->setIcon(Icons::get(Icons::Name::Save, QColor(QStringLiteral("#FFFFFF")), 15));
    m_fullNameEdit->setFocus();
}

void ClientesModule::onDeprecateClient(int clientId, const QString &displayName)
{
    if (!requirePermission(QStringLiteral("deprecate"), QStringLiteral("deprecar %1").arg(entityPlural().toLower()))) {
        return;
    }

    bool okInput = false;
    const QString note = QInputDialog::getText(
        this,
        QStringLiteral("Deprecar %1").arg(entitySingular()),
        QStringLiteral("El registro \"%1\" se marcará como Deprecado (se conservará en informes históricos).\n"
                       "Indica un motivo breve (opcional):").arg(displayName),
        QLineEdit::Normal,
        QString(),
        &okInput).trimmed();

    if (!okInput) {
        return;
    }

    QSqlQuery q(Database::instance().connection());
    q.prepare(QStringLiteral(
        "UPDATE clients SET is_deprecated = 1, is_active = 0, "
        "  deprecated_at = :at, deprecated_by = :by, deprecation_note = :note, "
        "  updated_at = CURRENT_TIMESTAMP "
        "WHERE id = :id;"));
    q.bindValue(QStringLiteral(":at"),   QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    q.bindValue(QStringLiteral(":by"),   Session::instance().currentUser().email);
    q.bindValue(QStringLiteral(":note"), note);
    q.bindValue(QStringLiteral(":id"),   clientId);

    if (!q.exec()) {
        QMessageBox::warning(this, QStringLiteral("Error"), q.lastError().text());
        return;
    }

    AuditEvent ev;
    ev.moduleId = moduleId();
    ev.targetTable = QStringLiteral("clients");
    ev.recordId = QString::number(clientId);
    ev.action = QStringLiteral("DEPRECATE");
    ev.oldValuesJson = QStringLiteral("{\"is_deprecated\":false}");
    ev.newValuesJson = QStringLiteral("{\"is_deprecated\":true}");
    ev.details = QStringLiteral("Deprecado %1 '%2'%3")
                     .arg(entitySingular(), displayName,
                          note.isEmpty() ? QString() : QStringLiteral(" · Motivo: %1").arg(note));
    Database::instance().logAuditEvent(ev);

    if (m_editingClientId == clientId) onResetForm();
    reloadTable();
}

void ClientesModule::onReactivateClient(int clientId, const QString &displayName)
{
    if (!requirePermission(QStringLiteral("deprecate"), QStringLiteral("reactivar %1").arg(entityPlural().toLower()))) {
        return;
    }

    const auto ans = QMessageBox::question(
        this,
        QStringLiteral("Reactivar %1").arg(entitySingular()),
        QStringLiteral("¿Deseas reactivar a \"%1\" para volver a operar normalmente?").arg(displayName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes);

    if (ans != QMessageBox::Yes) return;

    QSqlQuery q(Database::instance().connection());
    q.prepare(QStringLiteral(
        "UPDATE clients SET is_deprecated = 0, is_active = 1, "
        "  deprecated_at = '', deprecated_by = '', deprecation_note = '', "
        "  updated_at = CURRENT_TIMESTAMP "
        "WHERE id = :id;"));
    q.bindValue(QStringLiteral(":id"), clientId);

    if (!q.exec()) {
        QMessageBox::warning(this, QStringLiteral("Error"), q.lastError().text());
        return;
    }

    AuditEvent ev;
    ev.moduleId = moduleId();
    ev.targetTable = QStringLiteral("clients");
    ev.recordId = QString::number(clientId);
    ev.action = QStringLiteral("REACTIVATE");
    ev.oldValuesJson = QStringLiteral("{\"is_deprecated\":true}");
    ev.newValuesJson = QStringLiteral("{\"is_deprecated\":false}");
    ev.details = QStringLiteral("Reactivado %1 '%2'").arg(entitySingular(), displayName);
    Database::instance().logAuditEvent(ev);

    reloadTable();
}

void ClientesModule::onGenerateListReport()
{
    if (!requirePermission(QStringLiteral("report"), QStringLiteral("generar informes de %1").arg(entityPlural().toLower()))) {
        return;
    }

    if (m_visibleRows.isEmpty()) {
        QMessageBox::information(
            this,
            QStringLiteral("Sin registros"),
            QStringLiteral("No hay %1 visibles con el filtro actual para incluir en el informe.")
                .arg(entityPlural().toLower()));
        return;
    }

    const bool clinic = isClinicMode();

    Reporting::ReportPayload payload;
    payload.reportTitle    = QStringLiteral("Padrón de %1").arg(entityPlural());
    payload.reportSubtitle = m_searchEdit->text().trimmed().isEmpty()
        ? QStringLiteral("Listado general actualizado al %1")
              .arg(QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yyyy HH:mm")))
        : QStringLiteral("Resultados filtrados por: \"%1\"").arg(m_searchEdit->text().trimmed());
    payload.generatedBy    = Session::instance().currentUser().fullName;

    payload.metadata.append({QStringLiteral("Rubro"), clinic ? QStringLiteral("Consultorio") : QStringLiteral("Comercio")});
    payload.metadata.append({QStringLiteral("Incluye deprecados"), m_showDeprecatedCheck->isChecked() ? QStringLiteral("Sí") : QStringLiteral("No")});
    payload.metadata.append({QStringLiteral("Registros"), QString::number(m_visibleRows.size())});

    Reporting::TableSection section;
    section.title = QStringLiteral("Detalle de %1").arg(entityPlural());

    if (clinic) {
        section.columns = {
            {QStringLiteral("Paciente"), QStringLiteral("L"), 0},
            {QStringLiteral("Documento"), QStringLiteral("C"), 0},
            {QStringLiteral("Teléfono"), QStringLiteral("C"), 0},
            {QStringLiteral("Email"), QStringLiteral("L"), 0},
            {QStringLiteral("Cobertura / Afiliado"), QStringLiteral("L"), 0},
            {QStringLiteral("Estado"), QStringLiteral("C"), 0}
        };
    } else {
        section.columns = {
            {QStringLiteral("Cliente / Razón Social"), QStringLiteral("L"), 0},
            {QStringLiteral("Documento"), QStringLiteral("C"), 0},
            {QStringLiteral("Condición IVA"), QStringLiteral("L"), 0},
            {QStringLiteral("Teléfono / Email"), QStringLiteral("L"), 0},
            {QStringLiteral("Saldo Cta. Cte."), QStringLiteral("R"), 0},
            {QStringLiteral("Estado"), QStringLiteral("C"), 0}
        };
    }

    double totalBalance = 0.0;
    int activeCount = 0;

    for (const ClientRow &r : m_visibleRows) {
        if (!r.isDeprecated) ++activeCount;
        totalBalance += r.currentBalance;

        const QString doc = r.documentNumber.isEmpty()
            ? QStringLiteral("-")
            : (r.documentTypeCode.isEmpty() ? r.documentNumber : QStringLiteral("%1 %2").arg(r.documentTypeCode, r.documentNumber));
        const QString state = r.isDeprecated ? QStringLiteral("Deprecado") : QStringLiteral("Activo");

        if (clinic) {
            const QString cov = r.healthInsurance.isEmpty()
                ? QStringLiteral("Particular")
                : (r.affiliateNumber.isEmpty() ? r.healthInsurance : QStringLiteral("%1 (#%2)").arg(r.healthInsurance, r.affiliateNumber));

            section.rows.append(QStringList{
                r.fullName.toHtmlEscaped(),
                doc.toHtmlEscaped(),
                r.phone.isEmpty() ? QStringLiteral("-") : r.phone.toHtmlEscaped(),
                r.email.isEmpty() ? QStringLiteral("-") : r.email.toHtmlEscaped(),
                cov.toHtmlEscaped(),
                state
            });
        } else {
            const QString nameTxt = r.tradeName.isEmpty()
                ? r.fullName
                : QStringLiteral("%1 (%2)").arg(r.fullName, r.tradeName);
            QStringList contactParts;
            if (!r.phone.isEmpty()) contactParts.append(r.phone);
            if (!r.email.isEmpty()) contactParts.append(r.email);

            section.rows.append(QStringList{
                nameTxt.toHtmlEscaped(),
                doc.toHtmlEscaped(),
                r.ivaConditionName.isEmpty() ? QStringLiteral("-") : r.ivaConditionName.toHtmlEscaped(),
                contactParts.isEmpty() ? QStringLiteral("-") : contactParts.join(QStringLiteral(" · ")).toHtmlEscaped(),
                formatMoney(r.currentBalance),
                state
            });
        }
    }

    payload.tables.append(section);
    payload.totals.append({QStringLiteral("Total de registros"), QString::number(m_visibleRows.size())});
    payload.totals.append({QStringLiteral("Registros activos"), QString::number(activeCount)});
    if (!clinic) {
        payload.totals.append({QStringLiteral("Saldo total en cuenta corriente"), formatMoney(totalBalance)});
    }
    payload.footerNotes = QStringLiteral("Informe generado desde el módulo %1.").arg(entityPlural());

    AuditEvent ev;
    ev.moduleId = moduleId();
    ev.targetTable = QStringLiteral("clients");
    ev.action = QStringLiteral("REPORT");
    ev.details = QStringLiteral("Generación de informe general (%1 registros)").arg(m_visibleRows.size());
    Database::instance().logAuditEvent(ev);

    ReportViewerDialog::showReport(payload, this);
}

void ClientesModule::onGenerateSingleReport(int clientId)
{
    if (!requirePermission(QStringLiteral("report"), QStringLiteral("generar ficha de %1").arg(entitySingular().toLower()))) {
        return;
    }

    ClientRow r;
    if (!loadClientById(clientId, &r)) {
        return;
    }

    const bool clinic = isClinicMode();
    const QString doc = r.documentNumber.isEmpty()
        ? QStringLiteral("No registrado")
        : (r.documentTypeCode.isEmpty() ? r.documentNumber : QStringLiteral("%1 %2").arg(r.documentTypeCode, r.documentNumber));

    Reporting::ReportPayload payload;
    payload.reportTitle      = QStringLiteral("Ficha Individual de %1").arg(entitySingular());
    payload.reportSubtitle   = QStringLiteral("Legajo #%1 · Estado: %2")
                                   .arg(r.id)
                                   .arg(r.isDeprecated ? QStringLiteral("Deprecado") : QStringLiteral("Activo"));
    payload.recipientName    = r.fullName;
    payload.recipientEmail   = r.email; // Se precarga editable en la barra del visor
    payload.recipientDetails = QStringLiteral("Documento: %1 | Condición IVA: %2")
                                   .arg(doc, r.ivaConditionName.isEmpty() ? QStringLiteral("Sin especificar") : r.ivaConditionName);
    payload.generatedBy      = Session::instance().currentUser().fullName;

    Reporting::TableSection infoSec;
    infoSec.title = QStringLiteral("Datos Filiatorios y de Contacto");
    infoSec.columns = {
        {QStringLiteral("Campo"), QStringLiteral("L"), 35},
        {QStringLiteral("Detalle"), QStringLiteral("L"), 65}
    };

    infoSec.rows.append(QStringList{clinic ? QStringLiteral("Paciente") : QStringLiteral("Razón Social / Nombre"), r.fullName.toHtmlEscaped()});
    if (!clinic && !r.tradeName.isEmpty()) {
        infoSec.rows.append(QStringList{QStringLiteral("Nombre de Fantasía"), r.tradeName.toHtmlEscaped()});
    }
    infoSec.rows.append(QStringList{QStringLiteral("Documento"), doc.toHtmlEscaped()});
    infoSec.rows.append(QStringList{QStringLiteral("Condición frente al IVA"), r.ivaConditionName.isEmpty() ? QStringLiteral("-") : r.ivaConditionName.toHtmlEscaped()});
    infoSec.rows.append(QStringList{QStringLiteral("Teléfono / WhatsApp"), r.phone.isEmpty() ? QStringLiteral("-") : r.phone.toHtmlEscaped()});
    infoSec.rows.append(QStringList{QStringLiteral("Correo Electrónico"), r.email.isEmpty() ? QStringLiteral("-") : r.email.toHtmlEscaped()});
    infoSec.rows.append(QStringList{QStringLiteral("Domicilio"), r.address.isEmpty() ? QStringLiteral("-") : r.address.toHtmlEscaped()});

    if (clinic) {
        if (!r.birthDate.isEmpty()) {
            const QDate bd = QDate::fromString(r.birthDate, QStringLiteral("yyyy-MM-dd"));
            infoSec.rows.append(QStringList{
                QStringLiteral("Fecha de Nacimiento"),
                bd.isValid() ? bd.toString(QStringLiteral("dd/MM/yyyy")) : r.birthDate
            });
        }
        infoSec.rows.append(QStringList{
            QStringLiteral("Obra Social / Prepaga"),
            r.healthInsurance.isEmpty() ? QStringLiteral("Particular") : r.healthInsurance.toHtmlEscaped()
        });
        if (!r.affiliateNumber.isEmpty()) {
            infoSec.rows.append(QStringList{QStringLiteral("Número de Afiliado"), r.affiliateNumber.toHtmlEscaped()});
        }
    } else {
        infoSec.rows.append(QStringList{QStringLiteral("Límite de Cuenta Corriente"), formatMoney(r.creditLimit)});
        infoSec.rows.append(QStringList{QStringLiteral("Saldo Actual"), formatMoney(r.currentBalance)});
    }

    if (!r.notes.isEmpty()) {
        infoSec.rows.append(QStringList{QStringLiteral("Observaciones"), r.notes.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br/>"))});
    }
    if (r.isDeprecated && !r.deprecationNote.isEmpty()) {
        infoSec.rows.append(QStringList{QStringLiteral("Nota de Deprecado"), r.deprecationNote.toHtmlEscaped()});
    }

    payload.tables.append(infoSec);
    if (!clinic) {
        payload.totals.append({QStringLiteral("Saldo de Cuenta Corriente"), formatMoney(r.currentBalance)});
    }
    payload.footerNotes = QStringLiteral("Documento interno emitido para consulta o envío al titular.");

    AuditEvent ev;
    ev.moduleId = moduleId();
    ev.targetTable = QStringLiteral("clients");
    ev.recordId = QString::number(r.id);
    ev.action = QStringLiteral("REPORT");
    ev.details = QStringLiteral("Ficha individual generada para %1 (%2)").arg(r.fullName, doc);
    Database::instance().logAuditEvent(ev);

    ReportViewerDialog::showReport(payload, this);
}
