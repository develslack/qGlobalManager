#include "externalSourceDialog.h"
#include "core/icons.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QStandardPaths>
#include <QTableWidget>
#include <QVBoxLayout>

ExternalSourceDialog::ExternalSourceDialog(QWidget *parent, int editSourceId)
    : QDialog(parent)
    , m_editSourceId(editSourceId)
{
    setWindowTitle(editSourceId > 0
                       ? QStringLiteral("Editar Fuente Externa de Datos")
                       : QStringLiteral("Asistente: Nueva Fuente Externa (Tabla Soporte Dinámica)"));
    setModal(true);
    setMinimumSize(520, 560);
    resize(760, 640);

    setupUi();
    if (m_editSourceId > 0) {
        loadExistingSource(m_editSourceId);
    }
}

void ExternalSourceDialog::setupUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(12);

    auto *intro = new QLabel(
        QStringLiteral("Conecta una <b>URL (JSON / texto plano)</b> o un <b>archivo CSV / TXT</b> generado por "
                       "otra aplicación para incorporarlo como tabla soporte dinámica y vincularlo a cualquier módulo."),
        this);
    intro->setWordWrap(true);
    intro->setStyleSheet(
        QStringLiteral("background-color: #EEF4FA; color: #1E3A5F; border: 1px solid #D9E2EC; "
                       "padding: 9px; border-radius: 6px;"));
    root->addWidget(intro);

    // 1. Origen de datos
    auto *srcGroup = new QGroupBox(QStringLiteral("1. Origen y Formato de la Fuente"), this);
    auto *srcForm = new QFormLayout(srcGroup);
    srcForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_nameEdit = new QLineEdit(srcGroup);
    m_nameEdit->setPlaceholderText(QStringLiteral("Ej: Cotizaciones de Divisas, Lista Proveedor X, Nomenclador..."));

    auto *typeRow = new QHBoxLayout();
    m_urlRadio = new QRadioButton(QStringLiteral("URL / Servicio Web (HTTP / HTTPS)"), srcGroup);
    m_fileRadio = new QRadioButton(QStringLiteral("Archivo local o de red (CSV / TXT / JSON)"), srcGroup);
    m_urlRadio->setChecked(true);
    auto *typeBg = new QButtonGroup(srcGroup);
    typeBg->addButton(m_urlRadio);
    typeBg->addButton(m_fileRadio);
    typeRow->addWidget(m_urlRadio);
    typeRow->addSpacing(12);
    typeRow->addWidget(m_fileRadio);
    typeRow->addStretch();

    auto *uriRow = new QHBoxLayout();
    m_uriEdit = new QLineEdit(srcGroup);
    m_uriEdit->setPlaceholderText(QStringLiteral("Ej: https://dolarapi.com/v1/dolares"));

    m_browseBtn = new QPushButton(QStringLiteral("Examinar..."), srcGroup);
    m_browseBtn->setIcon(Icons::get(Icons::Name::Edit, QColor(QStringLiteral("#1E3A5F")), 14));
    m_browseBtn->setProperty("secondary", true);
    m_browseBtn->setVisible(false);

    uriRow->addWidget(m_uriEdit, 1);
    uriRow->addWidget(m_browseBtn);

    auto *fmtRow = new QHBoxLayout();
    m_formatCombo = new QComboBox(srcGroup);
    m_formatCombo->addItem(QStringLiteral("JSON (API / Archivo estructurado)"), QStringLiteral("JSON"));
    m_formatCombo->addItem(QStringLiteral("CSV (Separado por punto y coma / coma)"), QStringLiteral("CSV"));
    m_formatCombo->addItem(QStringLiteral("TXT (Texto tabulado / clave=valor)"), QStringLiteral("TXT"));

    m_delimiterLabel = new QLabel(QStringLiteral("Separador:"), srcGroup);
    m_delimiterCombo = new QComboBox(srcGroup);
    m_delimiterCombo->addItem(QStringLiteral("Punto y coma (;)"), QStringLiteral(";"));
    m_delimiterCombo->addItem(QStringLiteral("Coma (,)"), QStringLiteral(","));
    m_delimiterCombo->addItem(QStringLiteral("Tabulador (TAB)"), QStringLiteral("\\t"));
    m_delimiterCombo->addItem(QStringLiteral("Barra vertical (|)"), QStringLiteral("|"));

    auto *previewBtn = new QPushButton(QStringLiteral("Previsualizar fuente"), srcGroup);
    previewBtn->setIcon(Icons::get(Icons::Name::Refresh, QColor(QStringLiteral("#FFFFFF")), 15));

    fmtRow->addWidget(m_formatCombo, 1);
    fmtRow->addSpacing(8);
    fmtRow->addWidget(m_delimiterLabel);
    fmtRow->addWidget(m_delimiterCombo);
    fmtRow->addSpacing(8);
    fmtRow->addWidget(previewBtn);

    srcForm->addRow(QStringLiteral("Nombre de la tabla soporte *:"), m_nameEdit);
    srcForm->addRow(QStringLiteral("Tipo de origen:"), typeRow);
    srcForm->addRow(QStringLiteral("URL o ruta del archivo *:"), uriRow);
    srcForm->addRow(QStringLiteral("Formato:"), fmtRow);
    root->addWidget(srcGroup);

    // 2. Previsualización de datos detectados
    auto *prevGroup = new QGroupBox(QStringLiteral("2. Previsualización de Datos Detectados"), this);
    auto *prevLayout = new QVBoxLayout(prevGroup);

    m_previewStatusLabel = new QLabel(
        QStringLiteral("Pulsa \"Previsualizar fuente\" para leer una muestra y detectar los campos disponibles."),
        prevGroup);
    m_previewStatusLabel->setWordWrap(true);
    m_previewStatusLabel->setStyleSheet(QStringLiteral("color: #52606D;"));
    prevLayout->addWidget(m_previewStatusLabel);

    m_previewTable = new QTableWidget(prevGroup);
    m_previewTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_previewTable->setAlternatingRowColors(true);
    m_previewTable->setMinimumHeight(130);
    m_previewTable->verticalHeader()->setVisible(false);
    prevLayout->addWidget(m_previewTable, 1);
    root->addWidget(prevGroup, 1);

    // 3. Mapeo y Módulo destino
    auto *mapGroup = new QGroupBox(QStringLiteral("3. Mapeo de Campos y Módulo de Destino"), this);
    auto *mapForm = new QFormLayout(mapGroup);
    mapForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_keyFieldCombo = new QComboBox(mapGroup);
    m_keyFieldCombo->setEditable(true);

    m_labelFieldCombo = new QComboBox(mapGroup);
    m_labelFieldCombo->setEditable(true);

    m_valueFieldCombo = new QComboBox(mapGroup);
    m_valueFieldCombo->setEditable(true);

    m_targetModuleCombo = new QComboBox(mapGroup);
    m_targetModuleCombo->addItem(QStringLiteral("Transversal (Disponible en todos los módulos)"), QStringLiteral("ALL"));
    m_targetModuleCombo->addItem(QStringLiteral("Panel de Control"), QStringLiteral("panel_control"));
    m_targetModuleCombo->addItem(QStringLiteral("Usuarios"), QStringLiteral("usuarios"));
    m_targetModuleCombo->addItem(QStringLiteral("Ventas / Facturación"), QStringLiteral("ventas"));
    m_targetModuleCombo->addItem(QStringLiteral("Caja y Cobros"), QStringLiteral("caja"));
    m_targetModuleCombo->addItem(QStringLiteral("Stock / Productos"), QStringLiteral("stock"));
    m_targetModuleCombo->addItem(QStringLiteral("Turnos / Agenda"), QStringLiteral("turnos"));
    m_targetModuleCombo->addItem(QStringLiteral("Pacientes / Historia Clínica"), QStringLiteral("pacientes"));

    m_syncNowCheck = new QCheckBox(
        QStringLiteral("Crear tabla soporte y sincronizar los datos inmediatamente al guardar"), mapGroup);
    m_syncNowCheck->setChecked(true);

    mapForm->addRow(QStringLiteral("Campo Clave (identificador único) *:"), m_keyFieldCombo);
    mapForm->addRow(QStringLiteral("Campo Nombre / Etiqueta visible *:"), m_labelFieldCombo);
    mapForm->addRow(QStringLiteral("Campo Valor (cotización / dato):"), m_valueFieldCombo);
    mapForm->addRow(QStringLiteral("¿A qué módulo servirá?:"), m_targetModuleCombo);
    mapForm->addRow(QString(), m_syncNowCheck);
    root->addWidget(mapGroup);

    // Botones finales
    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();

    auto *cancelBtn = new QPushButton(QStringLiteral("Cancelar"), this);
    cancelBtn->setIcon(Icons::get(Icons::Name::Close, QColor(QStringLiteral("#1E3A5F")), 15));
    cancelBtn->setProperty("secondary", true);

    auto *saveBtn = new QPushButton(QStringLiteral("Guardar tabla soporte externa"), this);
    saveBtn->setIcon(Icons::get(Icons::Name::Save, QColor(QStringLiteral("#FFFFFF")), 15));
    saveBtn->setDefault(true);

    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(saveBtn);
    root->addLayout(btnRow);

    connect(m_urlRadio, &QRadioButton::toggled, this, &ExternalSourceDialog::onSourceTypeToggled);
    connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ExternalSourceDialog::onFormatChanged);
    connect(m_browseBtn, &QPushButton::clicked, this, &ExternalSourceDialog::onBrowseFile);
    connect(previewBtn, &QPushButton::clicked, this, &ExternalSourceDialog::onPreviewClicked);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, this, &ExternalSourceDialog::onSaveClicked);

    onFormatChanged(0);
}

void ExternalSourceDialog::loadExistingSource(int sourceId)
{
    ExternalSourceDef def;
    if (!ExternalSourceManager::loadSourceById(sourceId, &def)) {
        return;
    }

    m_nameEdit->setText(def.name);
    if (def.sourceType.compare(QStringLiteral("FILE"), Qt::CaseInsensitive) == 0) {
        m_fileRadio->setChecked(true);
    } else {
        m_urlRadio->setChecked(true);
    }
    m_uriEdit->setText(def.sourceUri);

    const int fmtIdx = m_formatCombo->findData(def.format.toUpper());
    if (fmtIdx >= 0) m_formatCombo->setCurrentIndex(fmtIdx);

    const int delimIdx = m_delimiterCombo->findData(def.delimiter);
    if (delimIdx >= 0) m_delimiterCombo->setCurrentIndex(delimIdx);

    m_keyFieldCombo->setCurrentText(def.keyField);
    m_labelFieldCombo->setCurrentText(def.labelField);
    m_valueFieldCombo->setCurrentText(def.valueField);

    const int modIdx = m_targetModuleCombo->findData(def.targetModule);
    if (modIdx >= 0) m_targetModuleCombo->setCurrentIndex(modIdx);
}

void ExternalSourceDialog::onSourceTypeToggled()
{
    const bool isFile = m_fileRadio->isChecked();
    m_browseBtn->setVisible(isFile);
    m_uriEdit->setPlaceholderText(
        isFile ? QStringLiteral("Ruta del archivo (ej: /datos/lista_precios.csv)")
               : QStringLiteral("Ej: https://dolarapi.com/v1/dolares"));
}

void ExternalSourceDialog::onFormatChanged(int index)
{
    const QString fmt = m_formatCombo->itemData(index).toString();
    const bool needDelim = (fmt != QStringLiteral("JSON"));
    m_delimiterLabel->setVisible(needDelim);
    m_delimiterCombo->setVisible(needDelim);
}

void ExternalSourceDialog::onBrowseFile()
{
    const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Seleccionar archivo de origen"),
        docs.isEmpty() ? QDir::homePath() : docs,
        QStringLiteral("Archivos de datos (*.csv *.txt *.json);;Todos los archivos (*.*)"));
    if (path.isEmpty()) return;

    m_uriEdit->setText(path);
    const QString lower = path.toLower();
    if (lower.endsWith(QStringLiteral(".json"))) {
        m_formatCombo->setCurrentIndex(m_formatCombo->findData(QStringLiteral("JSON")));
    } else if (lower.endsWith(QStringLiteral(".txt"))) {
        m_formatCombo->setCurrentIndex(m_formatCombo->findData(QStringLiteral("TXT")));
    } else if (lower.endsWith(QStringLiteral(".csv"))) {
        m_formatCombo->setCurrentIndex(m_formatCombo->findData(QStringLiteral("CSV")));
    }
}

void ExternalSourceDialog::populateFieldCombos(const QStringList &fields,
                                               const QString &preferredKey,
                                               const QString &preferredLabel,
                                               const QString &preferredValue)
{
    const QString curKey   = preferredKey.isEmpty()   ? m_keyFieldCombo->currentText().trimmed()   : preferredKey;
    const QString curLabel = preferredLabel.isEmpty() ? m_labelFieldCombo->currentText().trimmed() : preferredLabel;
    const QString curValue = preferredValue.isEmpty() ? m_valueFieldCombo->currentText().trimmed() : preferredValue;

    m_keyFieldCombo->clear();
    m_labelFieldCombo->clear();
    m_valueFieldCombo->clear();

    m_keyFieldCombo->addItems(fields);
    m_labelFieldCombo->addItems(fields);
    m_valueFieldCombo->addItem(QString());
    m_valueFieldCombo->addItems(fields);

    auto pickSmart = [&](QComboBox *combo, const QString &current, const QStringList &hints, int fallbackIdx) {
        if (!current.isEmpty() && fields.contains(current)) {
            combo->setCurrentText(current);
            return;
        }
        for (const QString &h : hints) {
            for (const QString &f : fields) {
                if (f.contains(h, Qt::CaseInsensitive)) {
                    combo->setCurrentText(f);
                    return;
                }
            }
        }
        if (fallbackIdx >= 0 && fallbackIdx < combo->count()) {
            combo->setCurrentIndex(fallbackIdx);
        }
    };

    pickSmart(m_keyFieldCombo,   curKey,   {QStringLiteral("casa"), QStringLiteral("code"), QStringLiteral("codigo"), QStringLiteral("moneda"), QStringLiteral("_key"), QStringLiteral("id")}, 0);
    pickSmart(m_labelFieldCombo, curLabel, {QStringLiteral("nombre"), QStringLiteral("name"), QStringLiteral("descripcion"), QStringLiteral("label"), QStringLiteral("_key")}, 0);
    pickSmart(m_valueFieldCombo, curValue, {QStringLiteral("venta"), QStringLiteral("valor"), QStringLiteral("value"), QStringLiteral("precio"), QStringLiteral("price"), QStringLiteral("_value")}, fields.size() > 1 ? 2 : 1);
}

void ExternalSourceDialog::onPreviewClicked()
{
    const QString type  = m_fileRadio->isChecked() ? QStringLiteral("FILE") : QStringLiteral("URL");
    const QString uri   = m_uriEdit->text().trimmed();
    const QString fmt   = m_formatCombo->currentData().toString();
    const QString delim = m_delimiterCombo->currentData().toString();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QByteArray raw;
    QString err;
    if (!ExternalSourceManager::fetchRawContent(type, uri, &raw, &err)) {
        QApplication::restoreOverrideCursor();
        m_previewStatusLabel->setText(err);
        m_previewStatusLabel->setStyleSheet(QStringLiteral("color: #C53030; font-weight: 600;"));
        return;
    }

    ExternalPreviewResult prev;
    if (!ExternalSourceManager::parseContent(raw, fmt, delim, &prev, &err, 15)) {
        QApplication::restoreOverrideCursor();
        m_previewStatusLabel->setText(err);
        m_previewStatusLabel->setStyleSheet(QStringLiteral("color: #C53030; font-weight: 600;"));
        return;
    }
    QApplication::restoreOverrideCursor();

    m_previewTable->clear();
    m_previewTable->setColumnCount(prev.detectedFields.size());
    m_previewTable->setHorizontalHeaderLabels(prev.detectedFields);
    m_previewTable->setRowCount(prev.sampleRows.size());

    for (int r = 0; r < prev.sampleRows.size(); ++r) {
        const auto &row = prev.sampleRows.at(r);
        for (int c = 0; c < prev.detectedFields.size(); ++c) {
            m_previewTable->setItem(r, c, new QTableWidgetItem(row.value(prev.detectedFields.at(c))));
        }
    }
    m_previewTable->resizeColumnsToContents();

    populateFieldCombos(prev.detectedFields);

    m_previewStatusLabel->setText(
        QStringLiteral("Lectura exitosa: %1 registro(s) detectados · %2 campo(s). Verifica el mapeo inferior.")
            .arg(prev.totalParsedRows)
            .arg(prev.detectedFields.size()));
    m_previewStatusLabel->setStyleSheet(QStringLiteral("color: #2E7D32; font-weight: 600;"));
}

void ExternalSourceDialog::onSaveClicked()
{
    ExternalSourceDef def;
    def.id           = m_editSourceId;
    def.name         = m_nameEdit->text().trimmed();
    def.sourceType   = m_fileRadio->isChecked() ? QStringLiteral("FILE") : QStringLiteral("URL");
    def.sourceUri    = m_uriEdit->text().trimmed();
    def.format       = m_formatCombo->currentData().toString();
    def.delimiter    = m_delimiterCombo->currentData().toString();
    def.keyField     = m_keyFieldCombo->currentText().trimmed();
    def.labelField   = m_labelFieldCombo->currentText().trimmed();
    def.valueField   = m_valueFieldCombo->currentText().trimmed();
    def.targetModule = m_targetModuleCombo->currentData().toString();
    def.isActive     = true;

    if (def.name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Dato requerido"),
                             QStringLiteral("Ingresa un nombre descriptivo para esta tabla soporte externa."));
        m_nameEdit->setFocus();
        return;
    }
    if (def.sourceUri.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Dato requerido"),
                             QStringLiteral("Ingresa la URL o selecciona el archivo de origen."));
        m_uriEdit->setFocus();
        return;
    }

    QString err;
    int savedId = -1;
    if (!ExternalSourceManager::saveSource(def, &savedId, &err)) {
        QMessageBox::warning(this, QStringLiteral("Error al guardar"), err);
        return;
    }
    m_savedSourceId = savedId;

    if (m_syncNowCheck->isChecked()) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        int synced = 0;
        QString syncErr;
        const bool ok = ExternalSourceManager::syncSource(savedId, &synced, &syncErr);
        QApplication::restoreOverrideCursor();

        if (ok) {
            QMessageBox::information(
                this,
                QStringLiteral("Fuente externa sincronizada"),
                QStringLiteral("La tabla soporte \"%1\" se creó y sincronizó %2 registro(s) correctamente.")
                    .arg(def.name).arg(synced));
        } else {
            QMessageBox::warning(
                this,
                QStringLiteral("Fuente guardada con aviso"),
                QStringLiteral("La configuración de \"%1\" se guardó, pero la sincronización inicial falló:\n%2\n\n"
                               "Podrás reintentar con \"Sincronizar ahora\".")
                    .arg(def.name, syncErr));
        }
    }

    accept();
}
