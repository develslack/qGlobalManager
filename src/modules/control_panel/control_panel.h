#ifndef MODULES_CONTROL_PANEL_CONTROL_PANEL_H
#define MODULES_CONTROL_PANEL_CONTROL_PANEL_H

#include "core/moduleBase.h"
#include "core/icons.h"

class DbAdminWidget;
class QCheckBox;
class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
struct SmtpConfig;
class QSpinBox;
class QTableWidget;
class QTabWidget;
class SupportTablesWidget;

class PanelControlModule : public ModuleBase
{
    Q_OBJECT

public:
    explicit PanelControlModule(QWidget *parent = nullptr);

    QString moduleId() const override { return QStringLiteral("panel_control"); }
    QString moduleTitle() const override { return QStringLiteral("Panel de Control"); }
    void onActivated() override;

private slots:

    // Control de acceso / candados por solapa
    void onTabChanged(int index);

    // Política de elevación e integridad de bitácora
    void onSaveElevationPolicy();
    void onVerifyAuditIntegrity();

    // Empresa
    void onSelectCompanyLogo();
    void onSaveCompany();

    // SMTP
    void onSmtpModeToggled(bool basicSelected);
    void onSmtpBasicProviderChanged(int index);
    void onOpenProviderHelpUrl();
    void onSaveSmtp();
    void onTestSmtp();
    void onSmtpSecurityChanged(int index);
    void onSmtpAuthMethodChanged(int index);
    void onAuthorizeGoogleOAuth();
    void onClearGoogleOAuth();

    // Impresoras
    void onRefreshPrinters();
    void onSavePrinters();
    void onPrintTestPage();

    // Seguridad, Contraseña Admin y Licencia/Token
    void onChangeAdminPassword();
    void onSaveSecurityTimers();
    void onSaveLicenseToken();
    void onGenerateDevToken();

    // Respaldo y Auditoría
    void onCreateBackup();
    void loadAuditLog();

private:
    void setupUi();
    QWidget *buildCompanyTab();
    QWidget *buildSmtpTab();
    QWidget *buildPrintersTab();
    QWidget *buildSecurityTab();
    QWidget *buildBackupAuditTab();

    void loadAllSettings();
    void updateDbAdminTabVisibility();
    void updateFirstLoginAlert();
    static bool isValidCuitCuil(const QString &input, QString *formattedOut = nullptr);

    QTabWidget *m_tabs = nullptr;
    int m_securityTabIndex = -1;
    int m_dbAdminTabIndex = -1;

    // Empresa
    QLineEdit *m_companyLegalName = nullptr;
    QLineEdit *m_companyTradeName = nullptr;
    QLineEdit *m_companyCuit = nullptr;
    QComboBox *m_companyBusinessType = nullptr;
    QLineEdit *m_companySubRubro = nullptr;
    QComboBox *m_companyIvaCombo = nullptr;
    QLineEdit *m_companyAddress = nullptr;
    QLineEdit *m_companyPhone = nullptr;
    QLineEdit *m_companyCurrency = nullptr;
    QLineEdit *m_companyLogoPath = nullptr;
    QLabel *m_companyLogoPreview = nullptr;

    // SMTP
    QRadioButton *m_smtpModeBasicRadio = nullptr;
    QRadioButton *m_smtpModeAdvancedRadio = nullptr;
    QLineEdit *m_smtpFromEmail = nullptr;
    QLineEdit *m_smtpFromName = nullptr;

    // Controles Modo Básico
    QWidget *m_smtpBasicContainer = nullptr;
    QComboBox *m_smtpBasicProvider = nullptr;
    QLineEdit *m_smtpBasicPass = nullptr;
    QCheckBox *m_smtpBasicShowPass = nullptr;
    QLabel *m_smtpBasicHelpLabel = nullptr;
    QLabel *m_smtpBasicSummaryLabel = nullptr;
    QPushButton *m_smtpProviderHelpBtn = nullptr;

    // Controles Modo Avanzado
    QWidget *m_smtpAdvancedContainer = nullptr;
    QLineEdit *m_smtpHost = nullptr;
    QSpinBox *m_smtpPort = nullptr;
    QComboBox *m_smtpSecurity = nullptr;
    QComboBox *m_smtpAuthMethod = nullptr;
    QWidget *m_smtpPasswordGroup = nullptr;
    QLineEdit *m_smtpUser = nullptr;
    QLineEdit *m_smtpPass = nullptr;
    QCheckBox *m_smtpShowPass = nullptr;
    QWidget *m_smtpOAuthGroup = nullptr;
    QLineEdit *m_oauthClientId = nullptr;
    QLineEdit *m_oauthClientSecret = nullptr;
    QLabel *m_oauthStatusLabel = nullptr;

    QString m_cachedAccessToken;
    QString m_cachedRefreshToken;
    qint64 m_cachedTokenExpiry = 0;
    SmtpConfig collectSmtpConfigFromUi() const;

    // Impresoras
    QComboBox *m_mainPrinterCombo = nullptr;
    QComboBox *m_ticketPrinterCombo = nullptr;
    QComboBox *m_ticketWidthCombo = nullptr;
    QLabel *m_printersInfoLabel = nullptr;

    // Seguridad / Admin / Token
    QLabel *m_firstLoginAlert = nullptr;
    QLineEdit *m_adminCurrentPass = nullptr;
    QLineEdit *m_adminNewPass = nullptr;
    QLineEdit *m_adminConfirmPass = nullptr;
    QSpinBox *m_idleMinutesSpin = nullptr;
    QSpinBox *m_warningSecondsSpin = nullptr;
    QLineEdit *m_licenseTokenEdit = nullptr;
    QLabel *m_licenseStatusLabel = nullptr;

    // Generador dev de tokens (solo visible en APP_DEBUG_BUILD)
    QComboBox *m_devTokenScope = nullptr;
    QLineEdit *m_devTokenCuit = nullptr;
    QDateEdit *m_devTokenExpiry = nullptr;

    // Respaldo y Auditoría
    QTableWidget *m_auditTable = nullptr;

    // Sub-widgets
    SupportTablesWidget *m_supportTablesWidget = nullptr;
    DbAdminWidget *m_dbAdminWidget = nullptr;

    void applyTabPermissions();
    bool isTabUnlocked(int index) const;
    QString tabElevationScope(int index) const;
    QString baseTabTitle(int index) const;
    Icons::Name baseTabIcon(int index) const;

    int m_companyTabIndex = -1;
    int m_smtpTabIndex = -1;
    int m_printersTabIndex = -1;
    int m_supportTablesTabIndex = -1;
    int m_backupAuditTabIndex = -1;
    //int m_dbAdminTabIndex = -1;
    int m_lastAllowedTabIndex = -1;
    bool m_handlingTabChange = false;

    // Política de elevación (en solapa Seguridad y Licencia)
    QSpinBox *m_elevDurationSpin = nullptr;
    QCheckBox *m_elevAllowSupportCheck = nullptr;
    QCheckBox *m_elevAllowBackupCheck = nullptr;
    QCheckBox *m_elevAllowDbAdminCheck = nullptr;
    QCheckBox *m_elevNotifyEmailCheck = nullptr;

    // Verificador de integridad de bitácora (en solapa Respaldo y Auditoría)
    QLabel *m_auditIntegrityLabel = nullptr;
    QPushButton *m_backupBtn = nullptr;

};

#endif // MODULES_PANEL_CONTROL_PANEL_CONTROL_H
