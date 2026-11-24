#ifndef MODULES_CONTROL_PANEL_SUPPORTTABLESWIDGET_H
#define MODULES_CONTROL_PANEL_SUPPORTTABLESWIDGET_H

#include "core/externalSourceManager.h"

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QResizeEvent;
class QSplitter;
class QStackedWidget;
class QTableWidget;

class SupportTablesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SupportTablesWidget(QWidget *parent = nullptr);
    void reloadAll();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onFilterTablesChanged(const QString &text);
    void onListItemSelected(int row);
    void onShowTableInfoModal();
    void onSaveRecord();
    void onResetForm();
    void onEditRecord(int recordId, const QString &code, const QString &name, const QString &description);
    void onDeprecateRecord(int recordId, const QString &displayName, bool isSystem);
    void onReactivateRecord(int recordId, const QString &displayName);
    void onConfigureRolePermissions(int roleId, const QString &roleName, bool isSystem);

    // Fuentes externas agnósticas
    void onAddExternalSource();
    void onEditCurrentExternalSource();
    void onSyncCurrentExternalSource();
    void onToggleCurrentExternalSourceActive();

private:
    struct SupportTableDef
    {
        QString tableName;
        QString displayName;
        QString subtitle;
        QString scopeType;        // "Transversal" o "Aislada"
        QStringList servedModules;
        QString originType;       // "Manual" o "Mixto (Semillas + Manual)"
        bool hasCode = false;
        bool hasDescription = false;
        bool hasIsSystem = false;
        bool isRolesTable = false;
        QString codePlaceholder;
        QString namePlaceholder;
        QString descPlaceholder;
    };

    struct CatalogListEntry
    {
        bool isExternal = false;
        int indexOrSourceId = -1;
    };

    void registerSupportTables();
    void setupUi();
    void populateTablesList(const QString &filter = QString());
    void loadCurrentStandardTableRecords();
    void loadCurrentExternalSourceItems();
    const SupportTableDef *currentStandardDef() const;

    QString quoteIdent(const QString &ident) const;
    int countStandardRecords(const QString &tableName, int *deprecatedCountOut = nullptr) const;
    QStringList introspectReferencingTables(const QString &tableName) const;

    QVector<SupportTableDef> m_tables;
    QVector<ExternalSourceDef> m_externalSources;
    QVector<CatalogListEntry> m_visibleEntries;

    bool m_selectedIsExternal = false;
    int m_selectedIndexOrSourceId = -1;
    int m_editingRecordId = -1;

    // Layout responsivo
    QSplitter *m_splitter = nullptr;

    // Panel izquierdo: listado maestro
    QLineEdit *m_tableFilterEdit = nullptr;
    QListWidget *m_tablesList = nullptr;

    // Panel derecho: 0 = Vacío, 1 = Tabla Soporte Estándar, 2 = Fuente Externa Dinámica
    QStackedWidget *m_rightStack = nullptr;

    // Encabezado común + controles tabla estándar
    QLabel *m_stdTitleLabel = nullptr;
    QLabel *m_stdSubtitleLabel = nullptr;
    QLabel *m_codeLabel = nullptr;
    QLineEdit *m_codeEdit = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QLabel *m_descLabel = nullptr;
    QLineEdit *m_descEdit = nullptr;
    QPushButton *m_saveBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
    QCheckBox *m_showDeprecatedCheck = nullptr;
    QLabel *m_recordsSummaryLabel = nullptr;
    QTableWidget *m_recordsTable = nullptr;

    // Controles fuente externa dinámica
    QLabel *m_extTitleLabel = nullptr;
    QLabel *m_extMetaLabel = nullptr;
    QPushButton *m_extToggleActiveBtn = nullptr;
    QTableWidget *m_extItemsTable = nullptr;
};

#endif // MODULES_CONTROL_PANEL_SUPPORTTABLESWIDGET_H
