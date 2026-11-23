#include "tableExporter.h"

#include <QByteArray>
#include <QDateTime>
#include <QFile>
#include <QPair>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTextStream>

namespace {

QString sanitizeSheetName(const QString &raw)
{
    QString s = raw.trimmed();
    const QString forbidden = QStringLiteral(":\\/?*[]");
    for (const QChar &ch : forbidden) {
        s.replace(ch, QLatin1Char('_'));
    }
    if (s.isEmpty()) {
        s = QStringLiteral("Datos");
    }
    return s.left(31);
}

QString xmlEscape(const QString &input)
{
    QString out;
    out.reserve(input.size() + 16);
    for (const QChar &ch : input) {
        const ushort u = ch.unicode();
        if (u < 0x20 && u != 0x09 && u != 0x0A && u != 0x0D) {
            continue; // Eliminar caracteres de control inválidos en XML 1.0
        }
        switch (u) {
        case '&':  out += QStringLiteral("&amp;");  break;
        case '<':  out += QStringLiteral("&lt;");   break;
        case '>':  out += QStringLiteral("&gt;");   break;
        case '"':  out += QStringLiteral("&quot;"); break;
        case '\'': out += QStringLiteral("&apos;"); break;
        default:   out += ch; break;
        }
    }
    return out;
}

QString colIndexToLetters(int colIndex)
{
    QString letters;
    int n = colIndex + 1;
    while (n > 0) {
        --n;
        letters.prepend(QChar(QLatin1Char('A').unicode() + (n % 26)));
        n /= 26;
    }
    return letters;
}

bool isNumericVariant(const QVariant &v)
{
    if (v.isNull() || !v.isValid()) {
        return false;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const int tid = v.typeId();
    return tid == QMetaType::Int || tid == QMetaType::UInt ||
           tid == QMetaType::LongLong || tid == QMetaType::ULongLong ||
           tid == QMetaType::Double || tid == QMetaType::Float;
#else
    const QVariant::Type t = v.type();
    return t == QVariant::Int || t == QVariant::UInt ||
           t == QVariant::LongLong || t == QVariant::ULongLong ||
           t == QVariant::Double;
#endif
}

QString variantToDisplayString(const QVariant &v)
{
    if (v.isNull() || !v.isValid()) {
        return QString();
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (v.typeId() == QMetaType::QDateTime) {
        return v.toDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    }
#else
    if (v.type() == QVariant::DateTime) {
        return v.toDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    }
#endif
    return v.toString();
}

quint32 computeCrc32(const QByteArray &data)
{
    static quint32 table[256];
    static bool initialized = false;
    if (!initialized) {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1U) ? (0xEDB88320U ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
        initialized = true;
    }

    quint32 crc = 0xFFFFFFFFU;
    for (int i = 0; i < data.size(); ++i) {
        const auto byte = static_cast<unsigned char>(data.at(i));
        crc = table[(crc ^ byte) & 0xFFU] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFU;
}

void appendLE16(QByteArray &buf, quint16 v)
{
    buf.append(static_cast<char>(v & 0xFFU));
    buf.append(static_cast<char>((v >> 8) & 0xFFU));
}

void appendLE32(QByteArray &buf, quint32 v)
{
    buf.append(static_cast<char>(v & 0xFFU));
    buf.append(static_cast<char>((v >> 8) & 0xFFU));
    buf.append(static_cast<char>((v >> 16) & 0xFFU));
    buf.append(static_cast<char>((v >> 24) & 0xFFU));
}

// Empaqueta archivos XML en un archivo ZIP estándar (método STORE, sin dependencias externas)
QByteArray buildStoredZip(const QVector<QPair<QByteArray, QByteArray>> &entries)
{
    struct CdRecord {
        QByteArray name;
        quint32 crc = 0;
        quint32 size = 0;
        quint32 localOffset = 0;
    };

    QByteArray zip;
    QVector<CdRecord> records;
    records.reserve(entries.size());

    for (const auto &entry : entries) {
        const QByteArray &name = entry.first;
        const QByteArray &content = entry.second;

        CdRecord rec;
        rec.name = name;
        rec.crc = computeCrc32(content);
        rec.size = static_cast<quint32>(content.size());
        rec.localOffset = static_cast<quint32>(zip.size());
        records.append(rec);

        // Local File Header
        appendLE32(zip, 0x04034B50U);
        appendLE16(zip, 20);      // Version needed (2.0)
        appendLE16(zip, 0x0800U); // UTF-8 filename flag
        appendLE16(zip, 0);       // Compression: STORE
        appendLE16(zip, 0);       // Mod time
        appendLE16(zip, 0x5421U); // Mod date
        appendLE32(zip, rec.crc);
        appendLE32(zip, rec.size);
        appendLE32(zip, rec.size);
        appendLE16(zip, static_cast<quint16>(name.size()));
        appendLE16(zip, 0);       // Extra length
        zip.append(name);
        zip.append(content);
    }

    const quint32 cdOffset = static_cast<quint32>(zip.size());

    for (const CdRecord &rec : records) {
        // Central Directory File Header
        appendLE32(zip, 0x02014B50U);
        appendLE16(zip, 20);      // Version made by
        appendLE16(zip, 20);      // Version needed
        appendLE16(zip, 0x0800U); // UTF-8 flag
        appendLE16(zip, 0);       // Compression: STORE
        appendLE16(zip, 0);       // Mod time
        appendLE16(zip, 0x5421U); // Mod date
        appendLE32(zip, rec.crc);
        appendLE32(zip, rec.size);
        appendLE32(zip, rec.size);
        appendLE16(zip, static_cast<quint16>(rec.name.size()));
        appendLE16(zip, 0);       // Extra len
        appendLE16(zip, 0);       // Comment len
        appendLE16(zip, 0);       // Disk start
        appendLE16(zip, 0);       // Internal attrs
        appendLE32(zip, 0);       // External attrs
        appendLE32(zip, rec.localOffset);
        zip.append(rec.name);
    }

    const quint32 cdSize = static_cast<quint32>(zip.size()) - cdOffset;

    // End of Central Directory Record
    appendLE32(zip, 0x06054B50U);
    appendLE16(zip, 0);
    appendLE16(zip, 0);
    appendLE16(zip, static_cast<quint16>(records.size()));
    appendLE16(zip, static_cast<quint16>(records.size()));
    appendLE32(zip, cdSize);
    appendLE32(zip, cdOffset);
    appendLE16(zip, 0);

    return zip;
}

} // namespace

bool TableExporter::exportSqlTable(const QSqlDatabase &db,
                                   bool isSqlite,
                                   const QString &tableName,
                                   const QString &filePath,
                                   Format format,
                                   int *exportedRowsOut,
                                   QString *errorMessage)
{
    if (!db.isOpen()) {
        if (errorMessage) *errorMessage = QStringLiteral("La base de datos no está abierta.");
        return false;
    }

    QString quotedTable;
    if (isSqlite) {
        QString esc = tableName;
        esc.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        quotedTable = QStringLiteral("\"%1\"").arg(esc);
    } else {
        QString esc = tableName;
        esc.replace(QLatin1Char('`'), QStringLiteral("``"));
        quotedTable = QStringLiteral("`%1`").arg(esc);
    }

    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("SELECT * FROM %1;").arg(quotedTable))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No se pudo leer la tabla '%1':\n%2")
                                .arg(tableName, query.lastError().text());
        }
        return false;
    }

    TableExportData data;
    data.sheetName = tableName;

    const QSqlRecord rec = query.record();
    const int colCount = rec.count();
    for (int c = 0; c < colCount; ++c) {
        data.headers.append(rec.fieldName(c));
    }

    while (query.next()) {
        QVariantList row;
        row.reserve(colCount);
        for (int c = 0; c < colCount; ++c) {
            row.append(query.value(c));
        }
        data.rows.append(row);
    }

    if (exportedRowsOut) {
        *exportedRowsOut = data.rows.size();
    }

    return exportData(data, filePath, format, errorMessage);
}

bool TableExporter::exportData(const TableExportData &data,
                               const QString &filePath,
                               Format format,
                               QString *errorMessage)
{
    switch (format) {
    case Format::Csv:
        return writeCsvOrTxt(data, filePath, QLatin1Char(';'), true, errorMessage);
    case Format::Txt:
        return writeCsvOrTxt(data, filePath, QLatin1Char('\t'), false, errorMessage);
    case Format::Xlsx:
        return writeXlsx(data, filePath, errorMessage);
    }
    return false;
}

bool TableExporter::writeCsvOrTxt(const TableExportData &data,
                                  const QString &filePath,
                                  QChar delimiter,
                                  bool quoteFields,
                                  QString *errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No se pudo crear el archivo:\n%1").arg(filePath);
        }
        return false;
    }

    // BOM UTF-8 en CSV para que Microsoft Excel detecte tildes y ñ automáticamente
    if (quoteFields) {
        file.write("\xEF\xBB\xBF");
    }

    QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
#endif

    auto formatCell = [delimiter, quoteFields](const QString &raw) -> QString {
        if (!quoteFields) {
            QString clean = raw;
            clean.replace(QLatin1Char('\t'), QLatin1Char(' '));
            clean.replace(QLatin1Char('\r'), QLatin1Char(' '));
            clean.replace(QLatin1Char('\n'), QLatin1Char(' '));
            return clean;
        }
        QString esc = raw;
        esc.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        if (esc.contains(delimiter) || esc.contains(QLatin1Char('"')) ||
            esc.contains(QLatin1Char('\n')) || esc.contains(QLatin1Char('\r'))) {
            return QStringLiteral("\"%1\"").arg(esc);
        }
        return esc;
    };

    QStringList headerCells;
    headerCells.reserve(data.headers.size());
    for (const QString &h : data.headers) {
        headerCells.append(formatCell(h));
    }
    out << headerCells.join(delimiter) << "\r\n";

    for (const QVariantList &row : data.rows) {
        QStringList cells;
        cells.reserve(row.size());
        for (const QVariant &v : row) {
            cells.append(formatCell(variantToDisplayString(v)));
        }
        out << cells.join(delimiter) << "\r\n";
    }

    file.close();
    return true;
}

bool TableExporter::writeXlsx(const TableExportData &data,
                              const QString &filePath,
                              QString *errorMessage)
{
    const QString sheetTitle = xmlEscape(sanitizeSheetName(data.sheetName));

    // 1. [Content_Types].xml
    const QByteArray contentTypesXml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
        "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
        "<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>"
        "</Types>";

    // 2. _rels/.rels
    const QByteArray rootRelsXml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
        "</Relationships>";

    // 3. xl/workbook.xml
    const QByteArray workbookXml =
        QStringLiteral(
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
            "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
            "<sheets><sheet name=\"%1\" sheetId=\"1\" r:id=\"rId1\"/></sheets>"
            "</workbook>")
            .arg(sheetTitle)
            .toUtf8();

    // 4. xl/_rels/workbook.xml.rels
    const QByteArray workbookRelsXml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>"
        "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>"
        "</Relationships>";

    // 5. xl/styles.xml (estilo 0: texto normal; estilo 1: encabezado corporativo azul #1E3A5F con letra blanca en negrita)
    const QByteArray stylesXml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<fonts count=\"2\">"
        "<font><sz val=\"10\"/><name val=\"Calibri\"/></font>"
        "<font><b/><sz val=\"10\"/><color rgb=\"FFFFFFFF\"/><name val=\"Calibri\"/></font>"
        "</fonts>"
        "<fills count=\"3\">"
        "<fill><patternFill patternType=\"none\"/></fill>"
        "<fill><patternFill patternType=\"gray125\"/></fill>"
        "<fill><patternFill patternType=\"solid\"><fgColor rgb=\"FF1E3A5F\"/><bgColor indexed=\"64\"/></patternFill></fill>"
        "</fills>"
        "<borders count=\"1\"><border><left/><right/><top/><bottom/><diagonal/></border></borders>"
        "<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>"
        "<cellXfs count=\"2\">"
        "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/>"
        "<xf numFmtId=\"0\" fontId=\"1\" fillId=\"2\" borderId=\"0\" xfId=\"0\" applyFont=\"1\" applyFill=\"1\"/>"
        "</cellXfs>"
        "</styleSheet>";

    // 6. xl/worksheets/sheet1.xml
    QString sheetXml;
    sheetXml.reserve(4096 + data.rows.size() * 128);
    sheetXml += QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<sheetData>");

    // Fila 1: encabezados
    sheetXml += QStringLiteral("<row r=\"1\">");
    for (int c = 0; c < data.headers.size(); ++c) {
        const QString ref = colIndexToLetters(c) + QStringLiteral("1");
        sheetXml += QStringLiteral("<c r=\"%1\" s=\"1\" t=\"inlineStr\"><is><t>%2</t></is></c>")
                        .arg(ref, xmlEscape(data.headers.at(c)));
    }
    sheetXml += QStringLiteral("</row>");

    // Filas de datos
    for (int r = 0; r < data.rows.size(); ++r) {
        const int excelRow = r + 2;
        sheetXml += QStringLiteral("<row r=\"%1\">").arg(excelRow);
        const QVariantList &row = data.rows.at(r);
        for (int c = 0; c < row.size(); ++c) {
            const QVariant &v = row.at(c);
            if (v.isNull() || !v.isValid()) {
                continue;
            }
            const QString ref = colIndexToLetters(c) + QString::number(excelRow);
            if (isNumericVariant(v)) {
                sheetXml += QStringLiteral("<c r=\"%1\"><v>%2</v></c>")
                                .arg(ref, v.toString());
            } else {
                sheetXml += QStringLiteral("<c r=\"%1\" t=\"inlineStr\"><is><t>%2</t></is></c>")
                                .arg(ref, xmlEscape(variantToDisplayString(v)));
            }
        }
        sheetXml += QStringLiteral("</row>");
    }

    sheetXml += QStringLiteral("</sheetData></worksheet>");

    const QVector<QPair<QByteArray, QByteArray>> entries = {
        {"[Content_Types].xml", contentTypesXml},
        {"_rels/.rels", rootRelsXml},
        {"xl/workbook.xml", workbookXml},
        {"xl/_rels/workbook.xml.rels", workbookRelsXml},
        {"xl/styles.xml", stylesXml},
        {"xl/worksheets/sheet1.xml", sheetXml.toUtf8()}
    };

    const QByteArray zipBytes = buildStoredZip(entries);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No se pudo crear el archivo Excel (.xlsx):\n%1").arg(filePath);
        }
        return false;
    }

    if (file.write(zipBytes) != zipBytes.size()) {
        file.close();
        if (errorMessage) {
            *errorMessage = QStringLiteral("Error escribiendo datos en el archivo Excel:\n%1").arg(filePath);
        }
        return false;
    }

    file.close();
    return true;
}
