#include "dbAdminWidget.h"
#include "core/db.h"
#include "core/icons.h"
#include "core/session.h"

#include <QAbstractItemView>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QTableView>
#include <QVBoxLayout>
#include <QVariant>

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

    // Barra informativa de ruta del archivo .sqlite
    auto *headerRow = new QHBoxLayout();
    m_dbPathLabel = new QLabel(this);
    m_dbPathLabel->setWordWrap(true);
    m_dbPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *refreshBtn = new QPushButton(QStringLiteral("Actualizar tablas"), this);
    refreshBtn->setIcon(Icons::get(Icons::Name::Refresh, QColor(QStringLiteral("#1E3A5F")), 15));
    refreshBtn->setProperty("secondary", true);

    auto *schemaBtn = new QPushButton(QStringLiteral("Ver estructura"), this);
    schemaBtn->setIcon(Icons::get(Icons::Name::Table, QColor(QStringLiteral("#1E3A5F")), 15));
    schemaBtn->setProperty("secondary", true);

    headerRow->addWidget(m_dbPathLabel, 1);
    headerRow->addWidget(schemaBtn);
    headerRow->addWidget(refreshBtn);
    rootLayout->addLayout(headerRow);

    // Divisor adaptable: lista de tablas a la izquierda + consola y resultados a la derecha
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    m_tablesList = new QListWidget(splitter);
    m_tablesList->setMinimumWidth(160);
    m_tablesList->setMaximumWidth(260);
    splitter->addWidget(m_tablesList);

    auto *rightPane = new QWidget(splitter);
    auto *rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(6, 0, 0, 0);
    rightLayout->setSpacing(8);

    m_sqlEditor = new QPlainTextEdit(rightPane);
    m_sqlEditor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_sqlEditor->setPlaceholderText(QStringLiteral("Escribe una consulta SQL (ej: SELECT * FROM users;)"));
    m_sqlEditor->setMaximumHeight(120);
    rightLayout->addWidget(m_sqlEditor);

    auto *sqlBtnRow = new QHBoxLayout();
    auto *execBtn = new QPushButton(QStringLiteral("Ejecutar SQL"), rightPane);
    execBtn->setIcon(Icons::get(Icons::Name::Play, QColor(QStringLiteral("#FFFFFF")), 15));

    auto *clearBtn = new QPushButton(QStringLiteral("Limpiar"), rightPane);
    clearBtn->setIcon(Icons::get(Icons::Name::Close, QColor(QStringLiteral("#1E3A5F")), 15));
    clearBtn->setProperty("secondary", true);

    m_statusLabel = new QLabel(QStringLiteral("Listo."), rightPane);
    m_statusLabel->setWordWrap(true);

    sqlBtnRow->addWidget(execBtn);
    sqlBtnRow->addWidget(clearBtn);
    sqlBtnRow->addWidget(m_statusLabel, 1);
    rightLayout->addLayout(sqlBtnRow);

    m_resultModel = new QSqlQueryModel(this);
    m_resultView = new QTableView(rightPane);
    m_resultView->setModel(m_resultModel);
    m_resultView->setAlternatingRowColors(true);
    m_resultView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_resultView->horizontalHeader()->setStretchLastSection(true);
    m_resultView->setMinimumHeight(240);
    rightLayout->addWidget(m_resultView, 1);

    splitter->addWidget(rightPane);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    rootLayout->addWidget(splitter, 1);

    connect(refreshBtn, &QPushButton::clicked, this, &DbAdminWidget::refreshTables);
    connect(schemaBtn, &QPushButton::clicked, this, &DbAdminWidget::onShowTableSchema);
    connect(m_tablesList, &QListWidget::currentTextChanged, this, &DbAdminWidget::onTableSelected);
    connect(execBtn, &QPushButton::clicked, this, &DbAdminWidget::onExecuteSql);
    connect(clearBtn, &QPushButton::clicked, this, &DbAdminWidget::onClearSql);
}

void DbAdminWidget::refreshTables()
{
    m_dbPathLabel->setText(
        QStringLiteral("<b>Archivo SQLite activo:</b> %1")
            .arg(Database::instance().databasePath().toHtmlEscaped()));

    const QString prevSelected = m_tablesList->currentItem()
                                     ? m_tablesList->currentItem()->text()
                                     : QString();

    m_tablesList->blockSignals(true);
    m_tablesList->clear();

    QSqlQuery query(Database::instance().connection());
    if (query.exec(QStringLiteral(
            "SELECT name FROM sqlite_master "
            "WHERE type = 'table' AND name NOT LIKE 'sqlite_%' "
            "ORDER BY name;"))) {
        while (query.next()) {
            m_tablesList->addItem(query.value(0).toString());
        }
    }
    m_tablesList->blockSignals(false);

    if (!prevSelected.isEmpty()) {
        const auto items = m_tablesList->findItems(prevSelected, Qt::MatchExactly);
        if (!items.isEmpty()) {
            m_tablesList->setCurrentItem(items.first());
            return;
        }
    }
    if (m_tablesList->count() > 0) {
        m_tablesList->setCurrentRow(0);
    }
}

void DbAdminWidget::onTableSelected(const QString &tableName)
{
    if (tableName.trimmed().isEmpty()) {
        return;
    }
    const QString sql = QStringLiteral("SELECT * FROM \"%1\" LIMIT 200;").arg(tableName);
    m_sqlEditor->setPlainText(sql);
    runSelectQuery(sql);
}

void DbAdminWidget::onShowTableSchema()
{
    if (!m_tablesList->currentItem()) {
        return;
    }
    const QString tableName = m_tablesList->currentItem()->text();
    const QString sql = QStringLiteral("PRAGMA table_info(\"%1\");").arg(tableName);
    m_sqlEditor->setPlainText(sql);
    runSelectQuery(sql);
}

void DbAdminWidget::onExecuteSql()
{
    const QString sql = m_sqlEditor->toPlainText().trimmed();
    if (sql.isEmpty()) {
        return;
    }

    const QString upper = sql.toUpper();
    const bool isReadOnly = upper.startsWith(QStringLiteral("SELECT")) ||
                            upper.startsWith(QStringLiteral("PRAGMA")) ||
                            upper.startsWith(QStringLiteral("EXPLAIN")) ||
                            upper.startsWith(QStringLiteral("WITH"));

    if (isReadOnly) {
        runSelectQuery(sql);
        return;
    }

    const auto reply = QMessageBox::warning(
        this,
        QStringLiteral("Confirmar consulta de escritura"),
        QStringLiteral("Esta sentencia SQL puede modificar o eliminar datos de la base de datos.\n\n"
                       "¿Deseas ejecutarla de todos modos?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return;
    }

    QSqlQuery query(Database::instance().connection());
    if (!query.exec(sql)) {
        m_statusLabel->setText(QStringLiteral("Error SQL: %1").arg(query.lastError().text()));
        QMessageBox::warning(this, QStringLiteral("Error SQL"), query.lastError().text());
        return;
    }

    const int affected = query.numRowsAffected();
    m_statusLabel->setText(
        QStringLiteral("Sentencia ejecutada correctamente. Filas afectadas: %1")
            .arg(affected < 0 ? 0 : affected));

    Database::instance().logAudit(
        Session::instance().currentUser().email,
        QStringLiteral("panel_control"),
        QStringLiteral("db_admin_exec"),
        sql.left(180));

    refreshTables();
}

void DbAdminWidget::onClearSql()
{
    m_sqlEditor->clear();
    m_statusLabel->setText(QStringLiteral("Editor limpio."));
}

void DbAdminWidget::runSelectQuery(const QString &sql)
{
    m_resultModel->setQuery(sql, Database::instance().connection());
    if (m_resultModel->lastError().isValid()) {
        m_statusLabel->setText(QStringLiteral("Error SQL: %1").arg(m_resultModel->lastError().text()));
        return;
    }

    m_resultView->resizeColumnsToContents();
    m_statusLabel->setText(QStringLiteral("Filas devueltas: %1").arg(m_resultModel->rowCount()));
}
