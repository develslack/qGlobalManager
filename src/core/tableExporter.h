#ifndef CORE_TABLEEXPORTER_H
#define CORE_TABLEEXPORTER_H

#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVector>

struct TableExportData
{
    QString sheetName;
    QStringList headers;
    QVector<QVariantList> rows;
};

class TableExporter
{
public:
    enum class Format {
        Csv,
        Txt,
        Xlsx
    };

    // Carga todos los registros de una tabla SQL (SQLite o MariaDB) y los exporta
    static bool exportSqlTable(const QSqlDatabase &db,
                               bool isSqlite,
                               const QString &tableName,
                               const QString &filePath,
                               Format format,
                               int *exportedRowsOut = nullptr,
                               QString *errorMessage = nullptr);

    // Exporta datos ya cargados en memoria (útil para grillas o resultados de consultas SQL)
    static bool exportData(const TableExportData &data,
                           const QString &filePath,
                           Format format,
                           QString *errorMessage = nullptr);

private:
    static bool writeCsvOrTxt(const TableExportData &data,
                              const QString &filePath,
                              QChar delimiter,
                              bool quoteFields,
                              QString *errorMessage);

    static bool writeXlsx(const TableExportData &data,
                          const QString &filePath,
                          QString *errorMessage);
};

#endif // CORE_TABLEEXPORTER_H
