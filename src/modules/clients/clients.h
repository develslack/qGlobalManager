#ifndef MODULES_CLIENTS_CLIENTS_H
#define MODULES_CLIENTS_CLIENTS_H

#include "core/moduleBase.h"

#include <QString>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QResizeEvent;
class QSplitter;
class QTableWidget;

class ClientesModule : public ModuleBase
{
    Q_OBJECT

public:
    explicit ClientesModule(QWidget *parent = nullptr);

    QString moduleId() const override { return QStringLiteral("clientes"); }
    QString moduleTitle() const override;
    void onActivated() override;

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onFilterChanged();
    void onSaveClicked();
    void onResetForm();
    void onEditClient(int clientId);
    void onDeprecateClient(int clientId, const QString &displayName);
    void onReactivateClient(int clientId, const QString &displayName);
    void onGenerateListReport();
    void onGenerateSingleReport(int clientId);

private:
    struct ClientRow
    {
        int id = -1;
        QString fullName;
        QString tradeName;
        int documentTypeId = -1;
        QString documentTypeCode;
        QString documentNumber;
        int ivaConditionId = -1;
        QString ivaConditionName;
        QString phone;
        QString email;
        QString address;
        QString birthDate;
        QString healthInsurance;
        QString affiliateNumber;
        double creditLimit = 0.0;
        double currentBalance = 0.0;
        QString notes;
        bool isDeprecated = false;
        QString deprecationNote;
        QString createdAt;
    };

    void setupUi();
    void applyBusinessModeLabels();
    void loadSupportCombos();
    void reloadTable();
    bool loadClientById(int clientId, ClientRow *out) const;
    QString entitySingular() const;
    QString entityPlural() const;
    bool isClinicMode() const;
    static bool isValidCuitCuil(const QString &input, QString *formattedOut = nullptr);

    int m_editingClientId = -1;
    QVector<ClientRow> m_visibleRows;

    QSplitter *m_splitter = nullptr;

    // Formulario de alta / edición
    QGroupBox *m_formGroup = nullptr;
    QLabel *m_fullNameLabel = nullptr;
    QLineEdit *m_fullNameEdit = nullptr;
    QLabel *m_tradeNameLabel = nullptr;
    QLineEdit *m_tradeNameEdit = nullptr;
    QComboBox *m_docTypeCombo = nullptr;
    QLineEdit *m_docNumberEdit = nullptr;
    QComboBox *m_ivaCombo = nullptr;
    QLineEdit *m_phoneEdit = nullptr;
    QLineEdit *m_emailEdit = nullptr;
    QLineEdit *m_addressEdit = nullptr;

    // Campos específicos de Consultorio (Pacientes)
    QLabel *m_birthDateLabel = nullptr;
    QWidget *m_birthDateContainer = nullptr;
    QCheckBox *m_hasBirthDateCheck = nullptr;
    QDateEdit *m_birthDateEdit = nullptr;
    QLabel *m_insuranceLabel = nullptr;
    QLineEdit *m_insuranceEdit = nullptr;
    QLabel *m_affiliateLabel = nullptr;
    QLineEdit *m_affiliateEdit = nullptr;

    // Campos específicos de Comercio (Cuenta Corriente)
    QLabel *m_creditLimitLabel = nullptr;
    QDoubleSpinBox *m_creditLimitSpin = nullptr;

    QPlainTextEdit *m_notesEdit = nullptr;
    QPushButton *m_saveBtn = nullptr;
    QPushButton *m_clearBtn = nullptr;

    // Listado y barra de herramientas
    QGroupBox *m_listGroup = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QCheckBox *m_showDeprecatedCheck = nullptr;
    QPushButton *m_reportListBtn = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QTableWidget *m_table = nullptr;
};

#endif // MODULES_CLIENTES_CLIENTES_H
