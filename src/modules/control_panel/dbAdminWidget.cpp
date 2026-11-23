#include "dbAdminWidget.h"
#include "core/bootstrapConfig.h"
#include "core/db.h"
#include "core/icons.h"
#include "core/session.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QShortcut>
#include <QSplitter>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
constexpr int kVerticalBreakpointPx = 740;
}

DbAdminWidget::DbAdminWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    refreshTables();
}

void DbAdminWidget::setupUi()
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(10);

    // =========================================================================
    // Barra superior: Conexión activa + Dump SQL + Exportar + Refrescar
    // =========================================================================
    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(8);

    m_connectionInfoLabel = new QLabel(this);
    m_connectionInfoLabel->setWordWrap(true);
    m_connectionInfoLabel->setStyleSheet(QStringLiteral("color: #1E3A5F;"));
    topRow->addWidget(m_connectionInfoLabel, 1);

    // Menú desplegable de Dump / Respaldo SQL
    auto *dumpBtn = new QPushButton(QStringLiteral("Dump SQL"), this);
    dumpBtn->setIcon(Icons::get(Icons::Name::Backup, QColor(QStringLiteral("#FFFFFF")), 15));

    auto *dumpMenu = new QMenu(dumpBtn);
    QAction *fullDumpAct = dumpMenu->addAction(
        Icons::get(Icons::Name::Database, QColor(QStringLiteral("#1E3A5F")), 15),
        QStringLiteral("Dump completo (Tablas + Datos + Índices + Triggers)"));
    QAction *schemaDumpAct = dumpMenu->addAction(
        Icons::get(Icons::Name::Table, QColor(QStringLiteral("#1E3A5F")), 15),
        QStringLiteral("Dump solo estructura (Tablas + Índices + Triggers, sin datos)"));
    dumpBtn->setMenu(dumpMenu);

    // Menú desplegable de Exportación (Excel, CSV, TXT)
    m_exportBtn = new QPushButton(QStringLiteral("Exportar datos"), this);
    m_exportBtn->setIcon(Icons::get(Icons::Name::Save, QColor(QStringLiteral("#1E3A5F")), 15));
    m_exportBtn->setProperty("secondary", true);

    auto *exportMenu = new QMenu(m_exportBtn);
    QAction *expTblXlsx = exportMenu->addAction(QStringLiteral("Tabla seleccionada → Excel (.xlsx)"));
    QAction *expTblCsv  = exportMenu->addAction(QStringLiteral("Tabla seleccionada → CSV (.csv)"));
    QAction *expTblTxt  = exportMenu->addAction(QStringLiteral("Tabla seleccionada → Texto tabulado (.txt)"));
    exportMenu->addSeparator();
    QAction *expGridXlsx = exportMenu->addAction(QStringLiteral("Resultado actual (grilla) → Excel (.xlsx)"));
    QAction *expGridCsv  = exportMenu->addAction(QStringLiteral("Resultado actual (grilla) → CSV (.csv)"));
    QAction *expGridTxt  = exportMenu->addAction(QStringLiteral("Resultado actual (grilla) → Texto (.txt)"));
    m_exportBtn->setMenu(exportMenu);

    auto *refreshBtn = new QPushButton(QStringLiteral("Actualizar tablas"), this);
    refreshBtn->setIcon(Icons::get(Icons::Name::Refresh, QColor(QStringLiteral("#1E3A5F")), 15));
    refreshBtn->setProperty("secondary", true);

    topRow->addWidget(dumpBtn);
    topRow->addWidget(m_exportBtn);
    topRow->addWidget(refreshBtn);
    rootLayout->addLayout(topRow);

    // =========================================================================
    // Splitter principal: Izquierda (Explorador de tablas) | Derecha (SQL + Grilla)
    // =========================================================================
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);

    // Panel izquierdo
    auto *leftPanel = new QFrame(m_splitter);
    leftPanel->setObjectName(QStringLiteral("CardFrame"));
    leftPanel->setMinimumWidth(200);
    leftPanel->setMaximumWidth(300);

    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(8, 8, 8, 8);
    leftLayout->setSpacing(6);

    auto *leftTitle = new QLabel(QStringLiteral("<b>Tablas de la Base de Datos</b>"), leftPanel);
    leftTitle->setStyleSheet(QStringLiteral("color: #1E3A5F;"));
    leftLayout->addWidget(leftTitle);

    m_tableFilterEdit = new QLineEdit(leftPanel);
    m_tableFilterEdit->setPlaceholderText(QStringLiteral("Filtrar tabla..."));
    m_tableFilterEdit->setClearButtonEnabled(true);
    leftLayout->addWidget(m_tableFilterEdit);

    m_tablesList = new QListWidget(leftPanel);
    m_tablesList->setAlternatingRowColors(true);
    m_tablesList->setContextMenuPolicy(Qt::CustomContextMenu);
    leftLayout->addWidget(m_tablesList, 1);

    m_tablesCountLabel = new QLabel(leftPanel);
    m_tablesCountLabel->setStyleSheet(QStringLiteral("color: #52606D; font-size: 8.5pt;"));
    leftLayout->addWidget(m_tablesCountLabel);

    m_splitter->addWidget(leftPanel);

    // Panel derecho
    auto *rightPanel = new QWidget(m_splitter);
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(8, 0, 0, 0);
    rightLayout->setSpacing(8);

    m_sqlEdit = new QPlainTextEdit(rightPanel);
    m_sqlEdit->setPlaceholderText(QStringLiteral("Escribe una consulta SQL (ej: SELECT * FROM users;)  ·  Ctrl+Enter para ejecutar"));
    m_sqlEdit->setMaximumHeight(130);
    m_sqlEdit->setStyleSheet(QStringLiteral("font-family: monospace; font-size: 10pt;"));
    rightLayout->addWidget(m_sqlEdit);

    // Barra de acciones SQL
    auto *actionRow = new QHBoxLayout();
    actionRow->setSpacing(6);

    auto *execBtn = new QPushButton(QStringLiteral("Ejecutar SQL"), rightPanel);
    execBtn->setIcon(Icons::get(Icons::Name::Check, QColor(QStringLiteral("#FFFFFF")), 15));

    auto *structBtn = new QPushButton(QStringLiteral("Estructura"), rightPanel);
    structBtn->setIcon(Icons::get(Icons::Name::Table, QColor(QStringLiteral("#1E3A5F")), 15));
    structBtn->setProperty("secondary", true);

    auto *ddlBtn = new QPushButton(QStringLiteral("Ver DDL"), rightPanel);
    ddlBtn->setIcon(Icons::get(Icons::Name::Database, QColor(QStringLiteral("#1E3A5F")), 15));
    ddlBtn->setProperty("secondary", true);

    auto *idxBtn = new QPushButton(QStringLiteral("Índices / Triggers"), rightPanel);
    idxBtn->setIcon(Icons::get(Icons::Name::Shield, QColor(QStringLiteral("#1E3A5F")), 15));
    idxBtn->setProperty("secondary", true);

    auto *clearBtn = new QPushButton(QStringLiteral("Limpiar"), rightPanel);
    clearBtn->setIcon(Icons::get(Icons::Name::Close, QColor(QStringLiteral("#1E3A5F")), 15));
    clearBtn->setProperty("secondary", true);

    m_statusLabel = new QLabel(QStringLiteral("Selecciona una tabla del panel izquierdo o escribe una consulta SQL."), rightPanel);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #52606D;"));
    m_statusLabel->setWordWrap(true);

    actionRow->addWidget(execBtn);
    actionRow->addWidget(structBtn);
    actionRow->addWidget(ddlBtn);
    actionRow->addWidget(idxBtn);
    actionRow->addWidget(clearBtn);
    actionRow->addSpacing(8);
    actionRow->addWidget(m_statusLabel, 1);
    rightLayout->addLayout(actionRow);

    // Grilla de resultados
    m_resultTable = new QTableWidget(rightPanel);
    m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultTable->setAlternatingRowColors(true);
    m_resultTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_resultTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_resultTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_resultTable->horizontalHeader()->setStretchLastSection(false);
    m_resultTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    rightLayout->addWidget(m_resultTable, 1);

    m_splitter->addWidget(rightPanel);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    rootLayout->addWidget(m_splitter, 1);

    // Atajo Ctrl+Enter en el editor SQL
    auto *runShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Return")), m_sqlEdit);
    connect(runShortcut, &QShortcut::activated, this, &DbAdminWidget::onExecuteQuery);

    // Conexiones
    connect(m_tableFilterEdit, &QLineEdit::textChanged, this, &DbAdminWidget::onFilterTablesChanged);
    connect(m_tablesList, &QListWidget::itemClicked, this, &DbAdminWidget::onTableClicked);
    connect(m_tablesList, &QListWidget::customContextMenuRequested, this, &DbAdminWidget::onTablesContextMenu);

    connect(refreshBtn, &QPushButton::clicked, this, &DbAdminWidget::refreshTables);
    connect(execBtn, &QPushButton::clicked, this, &DbAdminWidget::onExecuteQuery);
    connect(structBtn, &QPushButton::clicked, this, &DbAdminWidget::onViewTableStructure);
    connect(ddlBtn, &QPushButton::clicked, this, &DbAdminWidget::onViewTableDdl);
    connect(idxBtn, &QPushButton::clicked, this, &DbAdminWidget::onViewIndexesAndTriggers);
    connect(clearBtn, &QPushButton::clicked, this, &DbAdminWidget::onClearEditor);

    connect(fullDumpAct, &QAction::triggered, this, [this]() { onCreateDump(true); });
    connect(schemaDumpAct, &QAction::triggered, this, [this]() { onCreateDump(false); });

    connect(expTblXlsx, &QAction::triggered, this, [this]() { onExportSelectedTable(TableExporter::Format::Xlsx); });
    connect(expTblCsv,  &QAction::triggered, this, [this]() { onExportSelectedTable(TableExporter::Format::Csv); });
    connect(expTblTxt,  &QAction::triggered, this, [this]() { onExportSelectedTable(TableExporter::Format::Txt); });

    connect(expGridXlsx, &QAction::triggered, this, [this]() { onExportCurrentGrid(TableExporter::Format::Xlsx); });
    connect(expGridCsv,  &QAction::triggered, this, [this]() { onExportCurrentGrid(TableExporter::Format::Csv); });
    connect(expGridTxt,  &QAction::triggered, this, [this]() { onExportCurrentGrid(TableExporter::Format::Txt); });
}

void DbAdminWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    const Qt::Orientation target =
        (event->size().width() < kVerticalBreakpointPx) ? Qt::Vertical : Qt::Horizontal;
    if (m_splitter->orientation() != target) {
        m_splitter->setOrientation(target);
    }
}

void DbAdminWidget::updateConnectionBanner()
{
    const Database &db = Database::instance();
    const QString labelPrefix = db.isMariaDb()
                                    ? QStringLiteral("Conexión MariaDB activa:")
                                    : QStringLiteral("Base de datos SQLite activa:");
    m_connectionInfoLabel->setText(
        QStringLiteral("<b>%1</b> %2").arg(labelPrefix, db.databasePath().toHtmlEscaped()));
}

QString DbAdminWidget::quoteIdent(const QString &ident) const
{
    if (Database::instance().isSqlite()) {
        QString esc = ident;
        esc.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QStringLiteral("\"%1\"").arg(esc);
    }
    QString esc = ident;
    esc.replace(QLatin1Char('`'), QStringLiteral("``"));
    return QStringLiteral("`%1`").arg(esc);
}

QString DbAdminWidget::selectedTableName() const
{
    QListWidgetItem *item = m_tablesList->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void DbAdminWidget::refreshTables()
{
    updateConnectionBanner();
    m_allTables.clear();

    const Database &db = Database::instance();
    if (!db.isOpen()) {
        populateFilteredTables(QString());
        return;
    }

    QSqlQuery query(db.connection());
    if (db.isSqlite()) {
        if (query.exec(QStringLiteral(
                "SELECT name FROM sqlite_master "
                "WHERE type = 'table' AND name NOT LIKE 'sqlite_%' "
                "ORDER BY name ASC;"))) {
            while (query.next()) {
                m_allTables.append(query.value(0).toString());
            }
        }
    } else {
        if (query.exec(QStringLiteral("SHOW FULL TABLES WHERE Table_type = 'BASE TABLE';"))) {
            while (query.next()) {
                m_allTables.append(query.value(0).toString());
            }
            m_allTables.sort(Qt::CaseInsensitive);
        }
    }

    populateFilteredTables(m_tableFilterEdit->text());
}

void DbAdminWidget::populateFilteredTables(const QString &filter)
{
    const QString prevSelected = selectedTableName();
    const QString cleanFilter = filter.trimmed();

    m_tablesList->clear();
    const QIcon tblIcon = Icons::get(Icons::Name::Table, QColor(QStringLiteral("#1E3A5F")), 15);

    int shown = 0;
    for (const QString &tableName : m_allTables) {
        if (!cleanFilter.isEmpty() && !tableName.contains(cleanFilter, Qt::CaseInsensitive)) {
            continue;
        }
        auto *item = new QListWidgetItem(tblIcon, tableName);
        item->setData(Qt::UserRole, tableName);
        m_tablesList->addItem(item);
        if (tableName == prevSelected) {
            m_tablesList->setCurrentItem(item);
        }
        ++shown;
    }

    m_tablesCountLabel->setText(
        QStringLiteral("%1 tabla(s) · Clic derecho para exportar o inspeccionar").arg(shown));
}

void DbAdminWidget::onFilterTablesChanged(const QString &text)
{
    populateFilteredTables(text);
}

void DbAdminWidget::onTableClicked(QListWidgetItem *item)
{
    if (!item) return;
    const QString table = item->data(Qt::UserRole).toString();
    const QString sql = QStringLiteral("SELECT * FROM %1 LIMIT 200;").arg(quoteIdent(table));
    m_sqlEdit->setPlainText(sql);
    executeSqlText(sql);
}

void DbAdminWidget::onTablesContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_tablesList->itemAt(pos);
    if (!item) return;

    m_tablesList->setCurrentItem(item);
    const QString table = item->data(Qt::UserRole).toString();

    QMenu menu(this);
    QAction *viewDataAct = menu.addAction(
        Icons::get(Icons::Name::Table, QColor(QStringLiteral("#1E3A5F")), 14),
        QStringLiteral("Ver datos de '%1'").arg(table));
    QAction *viewStructAct = menu.addAction(QStringLiteral("Ver estructura (columnas)"));
    QAction *viewDdlAct    = menu.addAction(QStringLiteral("Ver DDL (CREATE TABLE)"));
    QAction *viewIdxAct    = menu.addAction(QStringLiteral("Ver índices y triggers"));
    menu.addSeparator();
    QAction *expXlsxAct = menu.addAction(
        Icons::get(Icons::Name::Save, QColor(QStringLiteral("#1E3A5F")), 14),
        QStringLiteral("Exportar '%1' a Excel (.xlsx)").arg(table));
    QAction *expCsvAct  = menu.addAction(QStringLiteral("Exportar '%1' a CSV (.csv)").arg(table));
    QAction *expTxtAct  = menu.addAction(QStringLiteral("Exportar '%1' a Texto (.txt)").arg(table));

    QAction *chosen = menu.exec(m_tablesList->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == viewDataAct)        onTableClicked(item);
    else if (chosen == viewStructAct) onViewTableStructure();
    else if (chosen == viewDdlAct)    onViewTableDdl();
    else if (chosen == viewIdxAct)    onViewIndexesAndTriggers();
    else if (chosen == expXlsxAct)    onExportSelectedTable(TableExporter::Format::Xlsx);
    else if (chosen == expCsvAct)     onExportSelectedTable(TableExporter::Format::Csv);
    else if (chosen == expTxtAct)     onExportSelectedTable(TableExporter::Format::Txt);
}

void DbAdminWidget::onExecuteQuery()
{
    const QString sql = m_sqlEdit->toPlainText().trimmed();
    if (sql.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Consulta vacía"),
                                 QStringLiteral("Escribe una sentencia SQL para ejecutar."));
        return;
    }
    executeSqlText(sql);
}

void DbAdminWidget::onViewTableStructure()
{
    const QString table = selectedTableName();
    if (table.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Selecciona una tabla"),
                                 QStringLiteral("Haz clic sobre una tabla del panel izquierdo para ver su estructura."));
        return;
    }

    const QString sql = Database::instance().isSqlite()
                            ? QStringLiteral("PRAGMA table_info(%1);").arg(quoteIdent(table))
                            : QStringLiteral("SHOW FULL COLUMNS FROM %1;").arg(quoteIdent(table));
    m_sqlEdit->setPlainText(sql);
    executeSqlText(sql);
}

void DbAdminWidget::onViewTableDdl()
{
    const QString table = selectedTableName();
    if (table.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Selecciona una tabla"),
                                 QStringLiteral("Selecciona una tabla del panel izquierdo para ver su DDL."));
        return;
    }

    QString escLiteral = table;
    escLiteral.replace(QLatin1Char('\''), QStringLiteral("''"));

    const QString sql = Database::instance().isSqlite()
                            ? QStringLiteral("SELECT type, name, sql FROM sqlite_master WHERE tbl_name = '%1' ORDER BY type DESC;").arg(escLiteral)
                            : QStringLiteral("SHOW CREATE TABLE %1;").arg(quoteIdent(table));
    m_sqlEdit->setPlainText(sql);
    executeSqlText(sql);
}

void DbAdminWidget::onViewIndexesAndTriggers()
{
    const QString table = selectedTableName();
    if (table.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Selecciona una tabla"),
                                 QStringLiteral("Selecciona una tabla del panel izquierdo para inspeccionar sus índices y triggers."));
        return;
    }

    QString escLiteral = table;
    escLiteral.replace(QLatin1Char('\''), QStringLiteral("''"));

    const QString sql = Database::instance().isSqlite()
                            ? QStringLiteral("SELECT type, name, sql FROM sqlite_master WHERE tbl_name = '%1' AND type IN ('index', 'trigger');").arg(escLiteral)
                            : QStringLiteral("SHOW INDEX FROM %1;").arg(quoteIdent(table));
    m_sqlEdit->setPlainText(sql);
    executeSqlText(sql);
}

void DbAdminWidget::onClearEditor()
{
    m_sqlEdit->clear();
    m_resultTable->clear();
    m_resultTable->setRowCount(0);
    m_resultTable->setColumnCount(0);
    m_statusLabel->setText(QStringLiteral("Editor limpio."));
    m_statusLabel->setStyleSheet(QStringLiteral("color: #52606D;"));
}

void DbAdminWidget::executeSqlText(const QString &sql)
{
    QElapsedTimer timer;
    timer.start();

    QSqlQuery query(Database::instance().connection());
    if (!query.exec(sql)) {
        m_statusLabel->setText(QStringLiteral("Error SQL: %1").arg(query.lastError().text()));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #C53030; font-weight: 600;"));
        return;
    }

    const qint64 elapsedMs = timer.elapsed();

    if (query.isSelect()) {
        const QSqlRecord rec = query.record();
        const int colCount = rec.count();
        m_resultTable->clear();
        m_resultTable->setColumnCount(colCount);

        QStringList headers;
        headers.reserve(colCount);
        for (int c = 0; c < colCount; ++c) {
            headers.append(rec.fieldName(c));
        }
        m_resultTable->setHorizontalHeaderLabels(headers);

        int row = 0;
        m_resultTable->setRowCount(0);
        while (query.next()) {
            m_resultTable->insertRow(row);
            for (int c = 0; c < colCount; ++c) {
                const QVariant val = query.value(c);
                const QString text = val.isNull() ? QStringLiteral("NULL") : val.toString();
                auto *item = new QTableWidgetItem(text);
                if (val.isNull()) {
                    item->setForeground(QBrush(QColor(QStringLiteral("#9AA5B1"))));
                }
                m_resultTable->setItem(row, c, item);
            }
            ++row;
        }
        m_resultTable->resizeColumnsToContents();
        m_statusLabel->setText(QStringLiteral("Filas devueltas: %1 (%2 ms)").arg(row).arg(elapsedMs));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #2E7D32; font-weight: 600;"));
    } else {
        const int affected = query.numRowsAffected();
        m_statusLabel->setText(QStringLiteral("Sentencia ejecutada · Filas afectadas: %1 (%2 ms)")
                                   .arg(affected).arg(elapsedMs));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #2E7D32; font-weight: 600;"));

        Database::instance().logAudit(
            Session::instance().currentUser().email,
            QStringLiteral("db_admin"),
            QStringLiteral("sql_dml_ddl"),
            sql.left(240));

        refreshTables();
    }
}

void DbAdminWidget::onCreateDump(bool includeData)
{
    const QString modeTag = includeData ? QStringLiteral("completo") : QStringLiteral("solo_estructura");
    const QString defaultFile = QDir(BootstrapConfig::protectedDbDirPath()).filePath(
        QStringLiteral("dump_%1_%2.sql")
            .arg(modeTag, QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))));

    const QString destPath = QFileDialog::getSaveFileName(
        this,
        includeData ? QStringLiteral("Guardar Dump Completo (.sql)")
                    : QStringLiteral("Guardar Dump Solo Estructura (.sql)"),
        defaultFile,
        QStringLiteral("Script SQL (*.sql)"));

    if (destPath.isEmpty()) {
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString outPath;
    QString err;
    const bool ok = Database::instance().createSqlDump(includeData, &outPath, &err, destPath);
    QApplication::restoreOverrideCursor();

    if (ok) {
        Database::instance().logAudit(
            Session::instance().currentUser().email,
            QStringLiteral("db_admin"),
            includeData ? QStringLiteral("sql_dump_full") : QStringLiteral("sql_dump_schema"),
            QStringLiteral("Archivo: %1").arg(outPath));

        QMessageBox::information(
            this,
            QStringLiteral("Dump generado correctamente"),
            QStringLiteral("El respaldo SQL (%1) se guardó en:\n%2")
                .arg(includeData ? QStringLiteral("Tablas + Datos + Índices + Triggers")
                                 : QStringLiteral("Solo Estructura + Índices + Triggers"),
                     outPath));
    } else {
        QMessageBox::critical(this, QStringLiteral("Error generando Dump"), err);
    }
}

void DbAdminWidget::onExportSelectedTable(TableExporter::Format format)
{
    const QString table = selectedTableName();
    if (table.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Selecciona una tabla"),
                                 QStringLiteral("Primero selecciona una tabla del panel lateral izquierdo para exportarla."));
        return;
    }

    QString ext;
    QString filter;
    switch (format) {
    case TableExporter::Format::Xlsx:
        ext = QStringLiteral("xlsx");
        filter = QStringLiteral("Libro de Excel (*.xlsx)");
        break;
    case TableExporter::Format::Csv:
        ext = QStringLiteral("csv");
        filter = QStringLiteral("Valores separados por punto y coma (*.csv)");
        break;
    case TableExporter::Format::Txt:
        ext = QStringLiteral("txt");
        filter = QStringLiteral("Archivo de texto tabulado (*.txt)");
        break;
    }

    const QString docsDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString suggested = QDir(docsDir.isEmpty() ? QDir::homePath() : docsDir)
                                  .filePath(QStringLiteral("%1_%2.%3")
                                                .arg(table,
                                                     QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmm")),
                                                     ext));

    const QString destPath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Exportar tabla '%1'").arg(table),
        suggested,
        filter);

    if (destPath.isEmpty()) {
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    int rowsExported = 0;
    QString err;
    const bool ok = TableExporter::exportSqlTable(
        Database::instance().connection(),
        Database::instance().isSqlite(),
        table,
        destPath,
        format,
        &rowsExported,
        &err);
    QApplication::restoreOverrideCursor();

    if (ok) {
        Database::instance().logAudit(
            Session::instance().currentUser().email,
            QStringLiteral("db_admin"),
            QStringLiteral("table_export"),
            QStringLiteral("Tabla: %1 | Formato: %2 | Filas: %3 | Destino: %4")
                .arg(table, ext.toUpper()).arg(rowsExported).arg(destPath));

        QMessageBox::information(
            this,
            QStringLiteral("Exportación completada"),
            QStringLiteral("Se exportaron %1 registro(s) de la tabla \"%2\" hacia:\n%3")
                .arg(rowsExported).arg(table, destPath));
    } else {
        QMessageBox::critical(this, QStringLiteral("Error al exportar"), err);
    }
}

void DbAdminWidget::onExportCurrentGrid(TableExporter::Format format)
{
    if (m_resultTable->columnCount() == 0) {
        QMessageBox::information(this, QStringLiteral("Sin resultados"),
                                 QStringLiteral("Ejecuta primero una consulta o selecciona una tabla para exportar el resultado."));
        return;
    }

    QString ext;
    QString filter;
    switch (format) {
    case TableExporter::Format::Xlsx:
        ext = QStringLiteral("xlsx");
        filter = QStringLiteral("Libro de Excel (*.xlsx)");
        break;
    case TableExporter::Format::Csv:
        ext = QStringLiteral("csv");
        filter = QStringLiteral("CSV (*.csv)");
        break;
    case TableExporter::Format::Txt:
        ext = QStringLiteral("txt");
        filter = QStringLiteral("Texto tabulado (*.txt)");
        break;
    }

    const QString docsDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString suggested = QDir(docsDir.isEmpty() ? QDir::homePath() : docsDir)
                                  .filePath(QStringLiteral("consulta_sql_%1.%2")
                                                .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmm")),
                                                     ext));

    const QString destPath = QFileDialog::getSaveFileName(
        this, QStringLiteral("Exportar resultado de consulta"), suggested, filter);
    if (destPath.isEmpty()) {
        return;
    }

    TableExportData data;
    data.sheetName = selectedTableName().isEmpty() ? QStringLiteral("ConsultaSQL") : selectedTableName();

    for (int c = 0; c < m_resultTable->columnCount(); ++c) {
        QTableWidgetItem *hdr = m_resultTable->horizontalHeaderItem(c);
        data.headers.append(hdr ? hdr->text() : QStringLiteral("Col_%1").arg(c + 1));
    }

    for (int r = 0; r < m_resultTable->rowCount(); ++r) {
        QVariantList row;
        row.reserve(m_resultTable->columnCount());
        for (int c = 0; c < m_resultTable->columnCount(); ++c) {
            QTableWidgetItem *cell = m_resultTable->item(r, c);
            row.append(cell ? cell->text() : QString());
        }
        data.rows.append(row);
    }

    QString err;
    if (TableExporter::exportData(data, destPath, format, &err)) {
        QMessageBox::information(
            this,
            QStringLiteral("Resultado exportado"),
            QStringLiteral("Se exportaron %1 fila(s) hacia:\n%2").arg(data.rows.size()).arg(destPath));
    } else {
        QMessageBox::critical(this, QStringLiteral("Error al exportar"), err);
    }
}
