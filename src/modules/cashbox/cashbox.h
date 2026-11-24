#ifndef MODULES_CASHBOX_CASHBOX_H
#define MODULES_CASHBOX_CASHBOX_H

#include "core/moduleBase.h"

#include <QMap>
#include <QPair>
#include <QString>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFrame;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QResizeEvent;
class QSplitter;
class QTabWidget;
class QTableWidget;

class CashboxModule : public ModuleBase
{
    Q_OBJECT

public:
    explicit CashboxModule(QWidget *parent = nullptr);

    QString moduleId() const override { return QStringLiteral("cashbox"); }
    QString moduleTitle() const override { return QStringLiteral("Caja y Fondos"); }
    void onActivated() override;

    // Utilidad transversal para futuros módulos (ej. Ventas / Cobros)
    static int activeOpenSessionIdForCurrentTerminal();

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onOpenSessionClicked();
    void onCloseSessionClicked();
    void onMovementTypeChanged(int index);
    void onRegisterMovementClicked();
    void onClearMovementForm();
    void onFilterChanged();
    void onVoidMovement(int movementId, const QString &summaryText);
    void onGenerateActiveSessionReport();
    void onGenerateHistoricalSessionReport(int sessionId);

private:
    struct CashSessionInfo
    {
        int id = -1;
        int terminalNumber = 1;
        QString workstation;
        QString openedByEmail;
        QString openedByName;
        QString openedAt;
        double openingAmount = 0.0;
        QString closedByEmail;
        QString closedByName;
        QString closedAt;
        double expectedCash = 0.0;
        double countedCash = 0.0;
        double differenceAmount = 0.0;
        QString status;
        QString notes;
    };

    struct SessionTotals
    {
        double totalIncome = 0.0;
        double totalExpense = 0.0;
        double cashIncome = 0.0;
        double cashExpense = 0.0;
        double expectedCashInDrawer = 0.0;
        QMap<QString, QPair<double, double>> byMethod; // methodName -> {incomes, expenses}
    };

    void setupUi();
    void loadSupportCombos();
    void reloadAll();
    void loadActiveSession();
    void reloadActiveMovements();
    void reloadHistoryTable();
    void updateSessionHeaderUi();

    bool loadSessionById(int sessionId, CashSessionInfo *out) const;
    SessionTotals computeSessionTotals(int sessionId, double openingAmount) const;
    void showSessionReport(int sessionId);
    static bool isCashMethodName(const QString &methodName);

    QString currencySymbol() const;
    QString formatMoney(double amount) const;
    bool isClinicMode() const;

    CashSessionInfo m_activeSession;
    SessionTotals m_activeTotals;

    // Barra superior de estado del turno
    QFrame *m_statusCard = nullptr;
    QLabel *m_statusTitleLabel = nullptr;
    QLabel *m_statusDetailLabel = nullptr;
    QPushButton *m_openSessionBtn = nullptr;
    QPushButton *m_closeSessionBtn = nullptr;
    QPushButton *m_reportSessionBtn = nullptr;

    // Tarjetas resumen en vivo
    QLabel *m_kpiOpeningLabel = nullptr;
    QLabel *m_kpiIncomeLabel = nullptr;
    QLabel *m_kpiExpenseLabel = nullptr;
    QLabel *m_kpiExpectedCashLabel = nullptr;
    QLabel *m_methodsBreakdownLabel = nullptr;

    // Cuerpo principal (Solapas: Turno Activo | Historial de Cierres)
    QTabWidget *m_tabs = nullptr;
    QSplitter *m_activeSplitter = nullptr;

    // Formulario de movimiento (panel izquierdo del turno activo)
    QGroupBox *m_movFormGroup = nullptr;
    QLabel *m_closedNoticeLabel = nullptr;
    QComboBox *m_movTypeCombo = nullptr;
    QComboBox *m_categoryCombo = nullptr;
    QComboBox *m_paymentMethodCombo = nullptr;
    QComboBox *m_clientCombo = nullptr;
    QDoubleSpinBox *m_amountSpin = nullptr;
    QLineEdit *m_referenceEdit = nullptr;
    QLineEdit *m_descriptionEdit = nullptr;
    QPushButton *m_registerMovBtn = nullptr;
    QPushButton *m_clearMovBtn = nullptr;

    // Grilla de movimientos del turno activo (panel derecho)
    QLineEdit *m_searchEdit = nullptr;
    QComboBox *m_filterTypeCombo = nullptr;
    QCheckBox *m_showVoidedCheck = nullptr;
    QTableWidget *m_movementsTable = nullptr;

    // Grilla de historial de cierres
    QTableWidget *m_historyTable = nullptr;
};

#endif // MODULES_CASHBOX_CASHBOX_H
