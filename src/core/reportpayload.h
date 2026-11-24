#ifndef CORE_REPORTPAYLOAD_H
#define CORE_REPORTPAYLOAD_H

#include <QString>
#include <QStringList>
#include <QList>

namespace Reporting {

struct TableColumn {
    QString header;
    QString alignment = QStringLiteral("L"); // "L" = Izquierda, "C" = Centro, "R" = Derecha
    int widthPercent = 0;                    // 0 = automático
};

struct TableSection {
    QString title;                           // Ej: "Detalle de Movimientos" (opcional)
    QList<TableColumn> columns;              // Columnas y alineación
    QList<QStringList> rows;                 // Filas de datos ya formateadas como texto
};

struct KeyValueItem {
    QString label;
    QString value;
};

struct ReportPayload {
    // Títulos y subtítulos
    QString reportTitle;                     // Ej: "Historial de Atenciones Clínicas"
    QString reportSubtitle;                  // Ej: "Período: 01/10/2026 al 31/10/2026"

    // Destinatario y correo predeterminado
    QString recipientName;                   // Ej: "Dr. Carlos Rossi" o "Juan Pérez"
    QString recipientEmail;                  // Email editable en la barra superior
    QString recipientDetails;                // Ej: "DNI: 30.123.456 | Condición IVA: Consumidor Final"

    // Parámetros y filtros aplicados (se muestran en una barra destacada)
    QList<KeyValueItem> metadata;            // Ej: [{"Sucursal", "Central"}, {"Moneda", "USD"}]

    // Tablas de contenido dinámicas
    QList<TableSection> tables;

    // Resumen / Totales destacados al pie
    QList<KeyValueItem> totals;              // Ej: [{"Subtotal", "$ 150.000"}, {"Total General", "$ 150.000"}]

    // Notas al pie o aclaraciones legales
    QString footerNotes;                     // Ej: "Documento emitido sin valor fiscal como comprobante interno."
    QString generatedBy;                     // Usuario o rol que emitió el informe
};

} // namespace Reporting

#endif // CORE_REPORTPAYLOAD_H
