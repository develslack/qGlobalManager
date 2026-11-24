#ifndef CORE_REPORTVIEWERDIALOG_H
#define CORE_REPORTVIEWERDIALOG_H

#include <QDialog>
#include "reportpayload.h"

class QTextBrowser;
class QLineEdit;
class QPushButton;
class QLabel;

class ReportViewerDialog : public QDialog {
    Q_OBJECT

public:
    explicit ReportViewerDialog(const Reporting::ReportPayload &payload, QWidget *parent = nullptr);
    ~ReportViewerDialog() override = default;

    /**
     * @brief Método de conveniencia para disparar el diálogo modal en una sola línea
     */
    static void showReport(const Reporting::ReportPayload &payload, QWidget *parent = nullptr);

private slots:
    void onExportPdfClicked();
    void onSendEmailClicked();
    void onPrintClicked();

private:
    void setupUi();
    QString buildHtmlDocument() const;
    bool generatePdfFile(const QString &targetFilePath) const;

    Reporting::ReportPayload m_payload;

    QLineEdit *m_txtEmailRecipient = nullptr;
    QPushButton *m_btnSendEmail = nullptr;
    QPushButton *m_btnExportPdf = nullptr;
    QPushButton *m_btnPrint = nullptr;
    QTextBrowser *m_textBrowser = nullptr;
    QLabel *m_lblStatus = nullptr;
};

#endif // CORE_REPORTVIEWERDIALOG_H
