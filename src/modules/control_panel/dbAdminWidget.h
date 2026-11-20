#ifndef MODULES_PANEL_CONTROL_DBADMINWIDGET_H
#define MODULES_PANEL_CONTROL_DBADMINWIDGET_H

#include <QWidget>

class QLabel;
class QListWidget;
class QPlainTextEdit;
class QSqlQueryModel;
class QTableView;

class DbAdminWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DbAdminWidget(QWidget *parent = nullptr);
    void refreshTables();

private slots:
    void onTableSelected(const QString &tableName);
    void onShowTableSchema();
    void onExecuteSql();
    void onClearSql();

private:
    void setupUi();
    void runSelectQuery(const QString &sql);

    QLabel *m_dbPathLabel = nullptr;
    QListWidget *m_tablesList = nullptr;
    QPlainTextEdit *m_sqlEditor = nullptr;
    QTableView *m_resultView = nullptr;
    QSqlQueryModel *m_resultModel = nullptr;
    QLabel *m_statusLabel = nullptr;
};

#endif // MODULES_PANEL_CONTROL_DBADMINWIDGET_H
