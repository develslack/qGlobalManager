#ifndef MODULES_CONTROL_PANEL_EXTERNALSOURCEDIALOG_H
#define MODULES_CONTROL_PANEL_EXTERNALSOURCEDIALOG_H

#include "core/externalSourceManager.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QTableWidget;

class ExternalSourceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExternalSourceDialog(QWidget *parent = nullptr, int editSourceId = -1);
    int savedSourceId() const { return m_savedSourceId; }

private slots:
    void onSourceTypeToggled();
    void onFormatChanged(int index);
    void onBrowseFile();
    void onPreviewClicked();
    void onSaveClicked();

private:
    void setupUi();
    void loadExistingSource(int sourceId);
    void populateFieldCombos(const QStringList &fields,
                             const QString &preferredKey = QString(),
                             const QString &preferredLabel = QString(),
                             const QString &preferredValue = QString());

    int m_editSourceId = -1;
    int m_savedSourceId = -1;

    QLineEdit *m_nameEdit = nullptr;
    QRadioButton *m_urlRadio = nullptr;
    QRadioButton *m_fileRadio = nullptr;
    QLineEdit *m_uriEdit = nullptr;
    QPushButton *m_browseBtn = nullptr;

    QComboBox *m_formatCombo = nullptr;
    QComboBox *m_delimiterCombo = nullptr;
    QLabel *m_delimiterLabel = nullptr;

    QLabel *m_previewStatusLabel = nullptr;
    QTableWidget *m_previewTable = nullptr;

    QComboBox *m_keyFieldCombo = nullptr;
    QComboBox *m_labelFieldCombo = nullptr;
    QComboBox *m_valueFieldCombo = nullptr;
    QComboBox *m_targetModuleCombo = nullptr;
    QCheckBox *m_syncNowCheck = nullptr;
};

#endif // MODULES_CONTROL_PANEL_EXTERNALSOURCEDIALOG_H
