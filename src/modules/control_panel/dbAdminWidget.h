#ifndef MODULES_PANEL_CONTROL_DBADMINWIDGET_H
#define MODULES_PANEL_CONTROL_DBADMINWIDGET_H

#include "core/tableExporter.h"

#include <QStringList>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPlainTextEdit;
class QPushButton;
class QResizeEvent;
class QSplitter;
class QTableWidget;

class DbAdminWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DbAdminWidget(QWidget *parent = nullptr);
    void refreshTables();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onFilterTablesChanged(const QString &text);
    void onTableClicked(QListWidgetItem *item);
    void onTablesContextMenu(const QPoint &pos);
    void onExecuteQuery();
    void onViewTableStructure();
    void onViewTableDdl();
    void onViewIndexesAndTriggers();
    void onClearEditor();
    void onCreateDump(bool includeData);
    void onExportSelectedTable(TableExporter::Format format);
    void onExportCurrentGrid(TableExporter::Format format);

private:
    void setupUi();
    void populateFilteredTables(const QString &filter);
    QString selectedTableName() const;
    QString quoteIdent(const QString &ident) const;
    void executeSqlText(const QString &sql);
    void updateConnectionBanner();

    QStringList m_allTables;

    QLabel *m_connectionInfoLabel = nullptr;
    QSplitter *m_splitter = nullptr;

    // Panel izquierdo: explorador de tablas
    QLineEdit *m_tableFilterEdit = nullptr;
    QListWidget *m_tablesList = nullptr;
    QLabel *m_tablesCountLabel = nullptr;

    // Panel derecho: editor SQL + grilla de resultados
    QPlainTextEdit *m_sqlEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTableWidget *m_resultTable = nullptr;

    QPushButton *m_exportBtn = nullptr;
};

#endif // MODULES_PANEL_CONTROL_DBADMINWIDGET_H
