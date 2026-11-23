#ifndef UI_SETUPWIZARD_H
#define UI_SETUPWIZARD_H

#include "core/bootstrapConfig.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QStackedWidget;

class SetupWizard : public QDialog
{
    Q_OBJECT

public:
    explicit SetupWizard(QWidget *parent = nullptr);

private slots:
    void onBackClicked();
    void onNextOrFinishClicked();
    void onTerminalNumberChanged(int value);
    void onTestMariaDbClicked();

private:
    enum PageIndex {
        PageModeSelection = 0,
        PageSinglePcUsers = 1,
        PageNetworkConfig = 2,
        PageNetworkAdmin  = 3
    };

    void setupUi();
    QWidget *buildModeSelectionPage();
    QWidget *buildSinglePcUsersPage();
    QWidget *buildNetworkConfigPage();
    QWidget *buildNetworkAdminPage();

    void goToPage(int pageIndex);
    bool finishSinglePcSetup();
    bool handleNetworkConfigNext();
    bool finishNetworkAdminSetup();
    BootstrapData collectMariaDbConfig() const;

    QStackedWidget *m_stack = nullptr;
    QLabel *m_stepSubtitle = nullptr;
    QPushButton *m_backBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;

    // Página 0: Selección de modo
    QRadioButton *m_singlePcRadio = nullptr;
    QRadioButton *m_multiPcRadio = nullptr;
    QLineEdit *m_terminalNameEdit = nullptr;

    // Página 1: Puesto Único (Admin + Usuario Diario estilo UNIX)
    QLineEdit *m_singleAdminName = nullptr;
    QLineEdit *m_singleAdminEmail = nullptr;
    QLineEdit *m_singleAdminPass = nullptr;
    QLineEdit *m_singleAdminConfirm = nullptr;

    QLineEdit *m_dailyUserName = nullptr;
    QLineEdit *m_dailyUserEmail = nullptr;
    QComboBox *m_dailyUserRole = nullptr;
    QLineEdit *m_dailyUserPass = nullptr;
    QLineEdit *m_dailyUserConfirm = nullptr;

    // Página 2: Configuración de Red MariaDB (detección automática de puesto)
    QLabel *m_mysqlDriverStatus = nullptr;
    QLabel *m_terminalHintLabel = nullptr;
    QLineEdit *m_dbHostEdit = nullptr;
    QSpinBox *m_dbPortSpin = nullptr;
    QLineEdit *m_dbNameEdit = nullptr;
    QLineEdit *m_dbUserEdit = nullptr;
    QLineEdit *m_dbPassEdit = nullptr;
    QCheckBox *m_dbShowPassCheck = nullptr;
    QLabel *m_connectionTestResult = nullptr;
    int m_detectedTerminalNumber = 1;

    // Página 3: Creación de Administrador para Puesto #1 MariaDB
    QLineEdit *m_netAdminName = nullptr;
    QLineEdit *m_netAdminEmail = nullptr;
    QLineEdit *m_netAdminPass = nullptr;
    QLineEdit *m_netAdminConfirm = nullptr;
};

#endif // UI_SETUPWIZARD_H
