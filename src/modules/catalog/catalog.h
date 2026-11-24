#ifndef MODULES_CATALOG_CATALOG_H
#define MODULES_CATALOG_CATALOG_H

#include "core/moduleBase.h"

#include <QMap>
#include <QPair>
#include <QString>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QResizeEvent;
class QSplitter;
class QTableWidget;

class CatalogModule : public ModuleBase
{
    Q_OBJECT

public:
    explicit CatalogModule(QWidget *parent = nullptr);

    QString moduleId() const override { return QStringLiteral("catalog"); }
    QString moduleTitle() const override;
    void onActivated() override;

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onItemTypeChanged(int index);
    void onPricingModeChanged(int index);
    void onExternalSourceChanged(int index);
    void onExternalItemOrPriceChanged();
    void onFilterChanged();
    void onSaveClicked();
    void onResetForm();
    void onEditItem(int itemId);
    void onAdjustStock(int itemId);
    void onDeprecateItem(int itemId, const QString &displayName);
    void onReactivateItem(int itemId, const QString &displayName);
    void onGenerateListReport();
    void onGenerateSingleReport(int itemId);

private:
    struct CatalogRow
    {
        int id = -1;
        QString sku;
        QString name;
        QString itemType = QStringLiteral("PRODUCT"); // "PRODUCT" o "SERVICE"
        QString category;
        QString unit;
        double costPrice = 0.0;
        double salePrice = 0.0;
        QString pricingMode = QStringLiteral("LOCAL"); // "LOCAL" o "EXTERNAL"
        int extSourceId = -1;
        QString extSourceName;
        QString extItemKey;
        QString extItemLabel;
        double extRate = 0.0;
        bool hasExtRate = false;
        double foreignPrice = 0.0;
        double vatRate = 21.0;
        double stockCurrent = 0.0;
        double stockMin = 0.0;
        QString notes;
        bool isDeprecated = false;
        QString deprecationNote;
        QString createdAt;

        double effectiveLocalPrice() const
        {
            if (pricingMode == QStringLiteral("EXTERNAL") && hasExtRate && extRate > 0.0) {
                return foreignPrice * extRate;
            }
            return salePrice;
        }
    };

    void setupUi();
    void applyBusinessModeLabels();
    void loadExternalSourceCombos();
    void reloadExternalItemsCombo(int sourceId, const QString &selectKey = QString());
    void reloadExternalRatesCache();
    void reloadTable();
    bool loadItemById(int itemId, CatalogRow *out) const;
    bool isClinicMode() const;
    QString currencySymbol() const;
    QString formatMoney(double amount) const;

    int m_editingItemId = -1;
    QVector<CatalogRow> m_visibleRows;

    // Cache en vivo: (sourceId, itemKey) -> {label, valorNumerico}
    QMap<int, QString> m_extSourceNames;
    QMap<QPair<int, QString>, QPair<QString, double>> m_extRatesCache;

    QSplitter *m_splitter = nullptr;

    // Panel izquierdo: Formulario de Alta / Edición
    QGroupBox *m_formGroup = nullptr;
    QComboBox *m_typeCombo = nullptr;
    QLabel *m_skuLabel = nullptr;
    QLineEdit *m_skuEdit = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_categoryEdit = nullptr;
    QComboBox *m_unitCombo = nullptr;

    // Precios y multimoneda
    QComboBox *m_pricingModeCombo = nullptr;
    QLabel *m_costLabel = nullptr;
    QDoubleSpinBox *m_costSpin = nullptr;
    QLabel *m_salePriceLabel = nullptr;
    QDoubleSpinBox *m_salePriceSpin = nullptr;
    QLabel *m_extSourceLabel = nullptr;
    QComboBox *m_extSourceCombo = nullptr;
    QLabel *m_extItemLabel = nullptr;
    QComboBox *m_extItemCombo = nullptr;
    QLabel *m_foreignPriceLabel = nullptr;
    QDoubleSpinBox *m_foreignPriceSpin = nullptr;
    QLabel *m_computedPriceLabel = nullptr;
    QDoubleSpinBox *m_vatSpin = nullptr;

    // Stock (solo visible cuando item_type == PRODUCT)
    QLabel *m_stockCurrentLabel = nullptr;
    QDoubleSpinBox *m_stockCurrentSpin = nullptr;
    QLabel *m_stockMinLabel = nullptr;
    QDoubleSpinBox *m_stockMinSpin = nullptr;

    QPlainTextEdit *m_notesEdit = nullptr;
    QPushButton *m_saveBtn = nullptr;
    QPushButton *m_clearBtn = nullptr;

    // Panel derecho: Filtros y grilla
    QGroupBox *m_listGroup = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QComboBox *m_filterTypeCombo = nullptr;
    QCheckBox *m_onlyLowStockCheck = nullptr;
    QCheckBox *m_showDeprecatedCheck = nullptr;
    QPushButton *m_reportListBtn = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QTableWidget *m_table = nullptr;
};

#endif // MODULES_CATALOG_CATALOG_H
