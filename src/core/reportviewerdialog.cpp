#include "reportviewerdialog.h"
#include "core/theme.h"
#include "core/db.h"
#include "core/mailer.h"
#include "core/session.h"

#include <QApplication>
#include <QBuffer>
#include <QIcon>
#include <QPixmap>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextBrowser>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QPrinter>
#include <QPrintDialog>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QByteArray>

ReportViewerDialog::ReportViewerDialog(const Reporting::ReportPayload &payload, QWidget *parent)
    : QDialog(parent), m_payload(payload)
{
    setWindowTitle(payload.reportTitle.isEmpty() ? QStringLiteral("Informe Corporativo") : payload.reportTitle);
    resize(900, 750);
    setupUi();

    const QString html = buildHtmlDocument();
    m_textBrowser->setHtml(html);
}

void ReportViewerDialog::showReport(const Reporting::ReportPayload &payload, QWidget *parent)
{
    ReportViewerDialog dlg(payload, parent);
    dlg.exec();
}

void ReportViewerDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // --- Barra Superior de Acciones ---
    auto *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(8);

    auto *lblEmail = new QLabel(QStringLiteral("Enviar a:"), this);
    lblEmail->setStyleSheet(QStringLiteral("font-weight: 600; color: #1F2933;"));
    toolbarLayout->addWidget(lblEmail);

    m_txtEmailRecipient = new QLineEdit(this);
    m_txtEmailRecipient->setPlaceholderText(QStringLiteral("destinatario@correo.com"));
    m_txtEmailRecipient->setText(m_payload.recipientEmail);
    m_txtEmailRecipient->setMinimumWidth(260);
    toolbarLayout->addWidget(m_txtEmailRecipient);

    m_btnSendEmail = new QPushButton(QStringLiteral("📧 Enviar por Email"), this);
    m_btnSendEmail->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #2B7A78; color: white; font-weight: 600; padding: 6px 14px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #226361; }"
    ));
    toolbarLayout->addWidget(m_btnSendEmail);

    toolbarLayout->addSpacing(16);

    m_btnExportPdf = new QPushButton(QStringLiteral("📄 Exportar a PDF"), this);
    m_btnExportPdf->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #1E3A5F; color: white; font-weight: 600; padding: 6px 14px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #274C77; }"
    ));
    toolbarLayout->addWidget(m_btnExportPdf);

    m_btnPrint = new QPushButton(QStringLiteral("🖨️ Imprimir"), this);
    toolbarLayout->addWidget(m_btnPrint);

    toolbarLayout->addStretch();
    mainLayout->addLayout(toolbarLayout);

    // --- Visualizador Central de Informe ---
    m_textBrowser = new QTextBrowser(this);
    m_textBrowser->setOpenExternalLinks(false);
    m_textBrowser->setStyleSheet(QStringLiteral(
        "QTextBrowser { background-color: #FFFFFF; border: 1px solid #D9E2EC; border-radius: 4px; }"
    ));
    mainLayout->addWidget(m_textBrowser, 1);

    // --- Barra de Estado Inferior ---
    m_lblStatus = new QLabel(this);
    m_lblStatus->setStyleSheet(QStringLiteral("color: #52606D; font-size: 12px;"));
    mainLayout->addWidget(m_lblStatus);

    // Conexiones de slots
    connect(m_btnExportPdf, &QPushButton::clicked, this, &ReportViewerDialog::onExportPdfClicked);
    connect(m_btnSendEmail, &QPushButton::clicked, this, &ReportViewerDialog::onSendEmailClicked);
    connect(m_btnPrint, &QPushButton::clicked, this, &ReportViewerDialog::onPrintClicked);
}

QString ReportViewerDialog::buildHtmlDocument() const
{
    // Parámetros institucionales (se obtienen de la configuración general de la empresa)
    const Database &db = Database::instance();
    const QString legalName   = db.getSetting(QStringLiteral("company.legal_name"), QStringLiteral("Global Manager"));
    const QString tradeName   = db.getSetting(QStringLiteral("company.trade_name"));
    const QString companyName = tradeName.isEmpty() ? legalName : QStringLiteral("%1 (%2)").arg(tradeName, legalName);
    const QString cuit        = db.getSetting(QStringLiteral("company.cuit"));
    const QString companyId   = cuit.isEmpty() ? QStringLiteral("CUIT sin configurar") : QStringLiteral("CUIT: %1").arg(cuit);
    const QString companyAddr = db.getSetting(QStringLiteral("company.address"), QStringLiteral("Domicilio no registrado"));
    const QString companyPh   = db.getSetting(QStringLiteral("company.phone"));
    const QString companyPhone = companyPh.isEmpty() ? QStringLiteral("Tel: -") : QStringLiteral("Tel: %1").arg(companyPh);
    const QString companyLogo = db.getSetting(QStringLiteral("company.logo_path"));
    const QString businessType = db.getSetting(QStringLiteral("company.business_type"), QStringLiteral("Comercio"));
    const QString subRubro    = db.getSetting(QStringLiteral("company.sub_rubro"));

    const QString resolvedLogo = Theme::resolveLogoResource(companyLogo, businessType, subRubro);

    // Renderizar SVG/imagen a PNG en alta resolución (garantiza visualización idéntica en QTextBrowser y PDF en Qt 5/6)
    QString logoHtml;
    const QPixmap logoPix = QIcon(resolvedLogo).pixmap(140, 140);
    if (!logoPix.isNull()) {
        QByteArray pngBytes;
        QBuffer buffer(&pngBytes);
        buffer.open(QIODevice::WriteOnly);
        logoPix.save(&buffer, "PNG");
        logoHtml = QStringLiteral("<img src=\"data:image/png;base64,%1\" width=\"68\" height=\"68\" style=\"vertical-align: middle;\"/>")
                       .arg(QString::fromLatin1(pngBytes.toBase64()));
    }

    QString html;
    html += QStringLiteral(
        "<html><head><style>"
        "body { font-family: 'Segoe UI', Helvetica, Arial, sans-serif; color: #1F2933; margin: 25px; line-height: 1.4; }"
        ".header-table { width: 100%; border-bottom: 2px solid #1E3A5F; padding-bottom: 12px; margin-bottom: 16px; }"
        ".company-title { font-size: 18px; font-weight: bold; color: #1E3A5F; }"
        ".company-sub { font-size: 11px; color: #52606D; }"
        ".report-title { font-size: 20px; font-weight: bold; color: #2B7A78; margin-top: 10px; margin-bottom: 4px; }"
        ".report-sub { font-size: 12px; color: #52606D; margin-bottom: 14px; }"
        ".recipient-box { background-color: #F4F7FA; border: 1px solid #D9E2EC; border-radius: 4px; padding: 10px; margin-bottom: 16px; }"
        ".meta-badge { display: inline-block; background-color: #E2E8F0; padding: 4px 8px; border-radius: 3px; font-size: 11px; margin-right: 6px; }"
        ".data-table { width: 100%; border-collapse: collapse; margin-top: 12px; margin-bottom: 16px; }"
        ".data-table th { background-color: #1E3A5F; color: #FFFFFF; font-size: 12px; font-weight: 600; padding: 8px 10px; border: 1px solid #1E3A5F; }"
        ".data-table td { font-size: 11px; padding: 7px 10px; border: 1px solid #D9E2EC; }"
        ".data-table tr:nth-child(even) { background-color: #F9FAFB; }"
        ".totals-table { width: 45%; margin-left: auto; border-collapse: collapse; margin-top: 12px; margin-bottom: 16px; }"
        ".totals-table td { padding: 6px 10px; font-size: 12px; border-bottom: 1px solid #D9E2EC; }"
        ".total-label { font-weight: 600; color: #52606D; }"
        ".total-value { text-align: right; font-weight: bold; color: #1E3A5F; }"
        ".footer { border-top: 1px solid #D9E2EC; margin-top: 24px; padding-top: 8px; font-size: 10px; color: #9AA5B1; text-align: center; }"
        "</style></head><body>"
    );

    // Encabezado institucional
    html += QStringLiteral("<table class='header-table'><tr>");
    if (!logoHtml.isEmpty()) {
        html += QStringLiteral("<td width='80'>%1</td>").arg(logoHtml);
    }
    html += QStringLiteral(
        "<td>"
        "  <div class='company-title'>%1</div>"
        "  <div class='company-sub'>%2 | %3 | %4</div>"
        "</td>"
        "<td align='right' style='vertical-align: top; font-size: 11px; color: #52606D;'>"
        "  Fecha: <b>%5</b><br/>Hora: <b>%6</b>"
        "</td>"
        "</tr></table>"
    ).arg(companyName, companyId, companyAddr, companyPhone,
         QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yyyy")),
         QDateTime::currentDateTime().toString(QStringLiteral("hh:mm")));

    // Título y subtítulo
    html += QStringLiteral("<div class='report-title'>%1</div>").arg(m_payload.reportTitle);
    if (!m_payload.reportSubtitle.isEmpty()) {
        html += QStringLiteral("<div class='report-sub'>%1</div>").arg(m_payload.reportSubtitle);
    }

    // Cuadro de destinatario
    if (!m_payload.recipientName.isEmpty() || !m_payload.recipientDetails.isEmpty()) {
        html += QStringLiteral("<div class='recipient-box'>");
        if (!m_payload.recipientName.isEmpty()) {
            html += QStringLiteral("<b>Destinatario / Titular:</b> %1<br/>").arg(m_payload.recipientName);
        }
        if (!m_payload.recipientDetails.isEmpty()) {
            html += QStringLiteral("<span style='font-size: 11px; color: #52606D;'>%1</span>").arg(m_payload.recipientDetails);
        }
        html += QStringLiteral("</div>");
    }

    // Filtros / Metadatos aplicados
    if (!m_payload.metadata.isEmpty()) {
        html += QStringLiteral("<div style='margin-bottom: 12px;'>");
        for (const auto &item : m_payload.metadata) {
            html += QStringLiteral("<span class='meta-badge'><b>%1:</b> %2</span>").arg(item.label, item.value);
        }
        html += QStringLiteral("</div>");
    }

    // Renderizado de tablas
    for (const auto &tbl : m_payload.tables) {
        if (!tbl.title.isEmpty()) {
            html += QStringLiteral("<div style='font-size: 14px; font-weight: bold; color: #1E3A5F; margin-top: 14px;'>%1</div>").arg(tbl.title);
        }
        html += QStringLiteral("<table class='data-table'><thead><tr>");
        for (const auto &col : tbl.columns) {
            QString align = col.alignment == QStringLiteral("R") ? QStringLiteral("right") : (col.alignment == QStringLiteral("C") ? QStringLiteral("center") : QStringLiteral("left"));
            html += QStringLiteral("<th align='%1'>%2</th>").arg(align, col.header);
        }
        html += QStringLiteral("</tr></thead><tbody>");

        for (const auto &row : tbl.rows) {
            html += QStringLiteral("<tr>");
            for (int i = 0; i < tbl.columns.size(); ++i) {
                QString align = (i < tbl.columns.size() && tbl.columns[i].alignment == QStringLiteral("R")) ? QStringLiteral("right") : ((i < tbl.columns.size() && tbl.columns[i].alignment == QStringLiteral("C")) ? QStringLiteral("center") : QStringLiteral("left"));
                QString cellVal = (i < row.size()) ? row[i] : QString();
                html += QStringLiteral("<td align='%1'>%2</td>").arg(align, cellVal);
            }
            html += QStringLiteral("</tr>");
        }
        html += QStringLiteral("</tbody></table>");
    }

    // Bloque de Totales
    if (!m_payload.totals.isEmpty()) {
        html += QStringLiteral("<table class='totals-table'>");
        for (const auto &tot : m_payload.totals) {
            html += QStringLiteral("<tr><td class='total-label'>%1</td><td class='total-value'>%2</td></tr>").arg(tot.label, tot.value);
        }
        html += QStringLiteral("</table>");
    }

    // Observaciones o leyendas
    if (!m_payload.footerNotes.isEmpty()) {
        html += QStringLiteral("<div style='font-size: 11px; color: #52606D; font-style: italic; margin-top: 12px;'>* %1</div>").arg(m_payload.footerNotes);
    }

    // Pie de página legal
    html += QStringLiteral("<div class='footer'>Global Manager — Documento generado automáticamente por el sistema.");
    if (!m_payload.generatedBy.isEmpty()) {
        html += QStringLiteral(" Operador: %1.").arg(m_payload.generatedBy);
    }
    html += QStringLiteral("</div></body></html>");

    return html;
}

bool ReportViewerDialog::generatePdfFile(const QString &targetFilePath) const
{
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(targetFilePath);
    printer.setPageSize(QPageSize(QPageSize::A4));
    printer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);

    m_textBrowser->document()->print(&printer);
    return QFile::exists(targetFilePath);
}

void ReportViewerDialog::onExportPdfClicked()
{
    QString defaultName = m_payload.reportTitle.simplified().replace(QStringLiteral(" "), QStringLiteral("_"));
    if (defaultName.isEmpty()) {
        defaultName = QStringLiteral("Informe");
    }
    defaultName += QStringLiteral("_") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss")) + QStringLiteral(".pdf");

    const QString dest = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Exportar Informe a PDF"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/") + defaultName,
        QStringLiteral("Archivos PDF (*.pdf)")
    );

    if (dest.isEmpty()) {
        return;
    }

    if (generatePdfFile(dest)) {
        m_lblStatus->setText(QStringLiteral("✓ PDF generado con éxito en: ") + dest);
        QMessageBox::information(this, QStringLiteral("Exportación Exitosa"), QStringLiteral("El archivo PDF ha sido guardado correctamente."));
    } else {
        QMessageBox::critical(this, QStringLiteral("Error"), QStringLiteral("No se pudo generar el archivo PDF. Verifique los permisos de escritura."));
    }
}

void ReportViewerDialog::onSendEmailClicked()
{
    const QString recipient = m_txtEmailRecipient->text().trimmed();
    if (recipient.isEmpty() || !recipient.contains(QLatin1Char('@'))) {
        QMessageBox::warning(
            this,
            QStringLiteral("Email requerido"),
            QStringLiteral("Ingresa una dirección de correo válida para enviar el informe."));
        m_txtEmailRecipient->setFocus();
        return;
    }

    const QString tempPdf = QDir::tempPath()
        + QStringLiteral("/informe_")
        + QString::number(QDateTime::currentMSecsSinceEpoch())
        + QStringLiteral(".pdf");

    if (!generatePdfFile(tempPdf)) {
        QMessageBox::critical(
            this,
            QStringLiteral("Error"),
            QStringLiteral("No se pudo compilar el archivo PDF adjunto para el envío."));
        return;
    }

    m_lblStatus->setText(QStringLiteral("Enviando informe por correo a %1...").arg(recipient));
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents();

    const QString subject = m_payload.reportTitle.isEmpty()
        ? QStringLiteral("Global Manager - Informe Corporativo")
        : QStringLiteral("Global Manager - %1").arg(m_payload.reportTitle);

    const QString body = QStringLiteral(
        "Estimado/a%1,\n\n"
        "Adjuntamos en formato PDF el informe solicitado: %2.\n\n"
        "Saludos cordiales,\n%3")
        .arg(m_payload.recipientName.isEmpty() ? QString() : QStringLiteral(" %1").arg(m_payload.recipientName),
             m_payload.reportTitle,
             Database::instance().getSetting(QStringLiteral("company.legal_name"), QStringLiteral("Global Manager")));

    QString mailErr;
    const bool sent = Mailer::sendMail(recipient, subject, body, QStringList{tempPdf}, &mailErr);

    QApplication::restoreOverrideCursor();
    QFile::remove(tempPdf);

    if (sent) {
        AuditEvent ev;
        ev.moduleId = QStringLiteral("reporting");
        ev.action = QStringLiteral("REPORT_EMAIL_SENT");
        ev.details = QStringLiteral("Informe '%1' enviado a %2").arg(m_payload.reportTitle, recipient);
        Database::instance().logAuditEvent(ev);

        m_lblStatus->setText(QStringLiteral("Informe enviado correctamente a %1.").arg(recipient));
        QMessageBox::information(
            this,
            QStringLiteral("Envío exitoso"),
            QStringLiteral("El informe en PDF fue enviado correctamente a:\n%1").arg(recipient));
    } else {
        m_lblStatus->setText(QStringLiteral("Error al enviar correo: %1").arg(mailErr));
        QMessageBox::critical(this, QStringLiteral("Falló el envío por email"), mailErr);
    }
}

void ReportViewerDialog::onPrintClicked()
{
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog printDialog(&printer, this);
    if (printDialog.exec() == QDialog::Accepted) {
        m_textBrowser->document()->print(&printer);
    }
}
