#include "externalSourceManager.h"
#include "db.h"

#include <QDateTime>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>
#include <QUrl>
#include <QVariant>

namespace {

constexpr int kNetworkTimeoutMs = 15000;

QString jsonValueToString(const QJsonValue &v)
{
    if (v.isString()) return v.toString();
    if (v.isDouble()) return QString::number(v.toDouble(), 'g', 12);
    if (v.isBool())   return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (v.isNull() || v.isUndefined()) return QString();
    if (v.isObject()) return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    if (v.isArray())  return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    return QString();
}

QStringList splitCsvLine(const QString &line, QChar delimiter)
{
    QStringList fields;
    QString current;
    bool inQuotes = false;

    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (ch == QLatin1Char('"')) {
            if (inQuotes && i + 1 < line.size() && line.at(i + 1) == QLatin1Char('"')) {
                current.append(QLatin1Char('"'));
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (ch == delimiter && !inQuotes) {
            fields.append(current.trimmed());
            current.clear();
        } else {
            current.append(ch);
        }
    }
    fields.append(current.trimmed());
    return fields;
}

bool tryParseNumber(const QString &raw, double *out)
{
    QString s = raw.trimmed();
    if (s.isEmpty()) return false;

    // Limpiar símbolos monetarios y espacios
    s.remove(QLatin1Char('$'));
    s.remove(QLatin1Char(' '));

    bool ok = false;
    double val = s.toDouble(&ok);
    if (ok) {
        if (out) *out = val;
        return true;
    }

    // Formato latino (1.250,50 -> 1250.50)
    if (s.contains(QLatin1Char(',')) && s.contains(QLatin1Char('.'))) {
        if (s.lastIndexOf(QLatin1Char(',')) > s.lastIndexOf(QLatin1Char('.'))) {
            s.remove(QLatin1Char('.'));
            s.replace(QLatin1Char(','), QLatin1Char('.'));
        } else {
            s.remove(QLatin1Char(','));
        }
    } else if (s.contains(QLatin1Char(','))) {
        s.replace(QLatin1Char(','), QLatin1Char('.'));
    }

    val = s.toDouble(&ok);
    if (ok && out) *out = val;
    return ok;
}

void addFieldOrderUnique(QStringList &ordered, QSet<QString> &seen, const QString &field)
{
    if (!field.isEmpty() && !seen.contains(field)) {
        seen.insert(field);
        ordered.append(field);
    }
}

void flattenJsonObject(const QJsonObject &obj,
                       const QString &prefix,
                       QMap<QString, QString> &rowMap,
                       QStringList &fields,
                       QSet<QString> &seen)
{
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        const QString fullKey = prefix.isEmpty() ? it.key() : (prefix + QLatin1Char('.') + it.key());
        if (it.value().isObject()) {
            flattenJsonObject(it.value().toObject(), fullKey, rowMap, fields, seen);
        } else if (!it.value().isArray()) {
            addFieldOrderUnique(fields, seen, fullKey);
            rowMap.insert(fullKey, jsonValueToString(it.value()));
        }
    }
}

QChar resolveDelimiterChar(const QString &delimStr)
{
    if (delimStr == QStringLiteral("\\t") || delimStr == QStringLiteral("\t") ||
        delimStr.compare(QStringLiteral("TAB"), Qt::CaseInsensitive) == 0) {
        return QLatin1Char('\t');
    }
    if (!delimStr.isEmpty()) {
        return delimStr.at(0);
    }
    return QLatin1Char(';');
}

} // namespace

QString ExternalSourceManager::moduleDisplayName(const QString &moduleId)
{
    if (moduleId.isEmpty() || moduleId.compare(QStringLiteral("ALL"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Transversal (Todos los módulos)");
    }
    if (moduleId == QStringLiteral("panel_control")) return QStringLiteral("Panel de Control");
    if (moduleId == QStringLiteral("usuarios"))      return QStringLiteral("Usuarios");
    if (moduleId == QStringLiteral("ventas"))        return QStringLiteral("Ventas / Facturación");
    if (moduleId == QStringLiteral("caja"))          return QStringLiteral("Caja y Cobros");
    if (moduleId == QStringLiteral("stock"))         return QStringLiteral("Stock / Productos");
    if (moduleId == QStringLiteral("turnos"))        return QStringLiteral("Turnos / Agenda");
    if (moduleId == QStringLiteral("pacientes"))     return QStringLiteral("Pacientes / Historia Clínica");
    return moduleId;
}

QVector<ExternalSourceDef> ExternalSourceManager::loadSources(bool includeInactive)
{
    QVector<ExternalSourceDef> result;
    if (!Database::instance().isOpen()) return result;

    QString sql = QStringLiteral(
        "SELECT id, name, source_type, source_uri, format, delimiter, "
        "       key_field, label_field, value_field, target_module, last_sync_at, is_active "
        "FROM external_sources ");
    if (!includeInactive) {
        sql += QStringLiteral("WHERE is_active = 1 ");
    }
    sql += QStringLiteral("ORDER BY name ASC;");

    QSqlQuery q(Database::instance().connection());
    if (!q.exec(sql)) return result;

    while (q.next()) {
        ExternalSourceDef def;
        def.id           = q.value(0).toInt();
        def.name         = q.value(1).toString();
        def.sourceType   = q.value(2).toString();
        def.sourceUri    = q.value(3).toString();
        def.format       = q.value(4).toString();
        def.delimiter    = q.value(5).toString();
        def.keyField     = q.value(6).toString();
        def.labelField   = q.value(7).toString();
        def.valueField   = q.value(8).toString();
        def.targetModule = q.value(9).toString();
        def.lastSyncAt   = q.value(10).toString();
        def.isActive     = (q.value(11).toInt() == 1);
        result.append(def);
    }
    return result;
}

QVector<ExternalSourceDef> ExternalSourceManager::loadSourcesForModule(const QString &moduleId)
{
    QVector<ExternalSourceDef> result;
    const QVector<ExternalSourceDef> active = loadSources(false);
    for (const ExternalSourceDef &def : active) {
        if (def.targetModule.compare(QStringLiteral("ALL"), Qt::CaseInsensitive) == 0 ||
            def.targetModule.compare(moduleId, Qt::CaseInsensitive) == 0) {
            result.append(def);
        }
    }
    return result;
}

bool ExternalSourceManager::loadSourceById(int sourceId, ExternalSourceDef *out)
{
    if (!Database::instance().isOpen() || sourceId <= 0 || !out) return false;

    QSqlQuery q(Database::instance().connection());
    q.prepare(QStringLiteral(
        "SELECT id, name, source_type, source_uri, format, delimiter, "
        "       key_field, label_field, value_field, target_module, last_sync_at, is_active "
        "FROM external_sources WHERE id = :id LIMIT 1;"));
    q.bindValue(QStringLiteral(":id"), sourceId);

    if (!q.exec() || !q.next()) return false;

    out->id           = q.value(0).toInt();
    out->name         = q.value(1).toString();
    out->sourceType   = q.value(2).toString();
    out->sourceUri    = q.value(3).toString();
    out->format       = q.value(4).toString();
    out->delimiter    = q.value(5).toString();
    out->keyField     = q.value(6).toString();
    out->labelField   = q.value(7).toString();
    out->valueField   = q.value(8).toString();
    out->targetModule = q.value(9).toString();
    out->lastSyncAt   = q.value(10).toString();
    out->isActive     = (q.value(11).toInt() == 1);
    return true;
}

QVector<ExternalSourceItem> ExternalSourceManager::loadItems(int sourceId)
{
    QVector<ExternalSourceItem> list;
    if (!Database::instance().isOpen() || sourceId <= 0) return list;

    QSqlQuery q(Database::instance().connection());
    q.prepare(QStringLiteral(
        "SELECT id, source_id, item_key, item_label, value_num, value_text, updated_at "
        "FROM external_source_items WHERE source_id = :sid ORDER BY item_label ASC, item_key ASC;"));
    q.bindValue(QStringLiteral(":sid"), sourceId);

    if (!q.exec()) return list;

    while (q.next()) {
        ExternalSourceItem item;
        item.id              = q.value(0).toInt();
        item.sourceId        = q.value(1).toInt();
        item.itemKey         = q.value(2).toString();
        item.itemLabel       = q.value(3).toString();
        item.hasNumericValue = !q.value(4).isNull();
        item.valueNum        = item.hasNumericValue ? q.value(4).toDouble() : 0.0;
        item.valueText       = q.value(5).toString();
        item.updatedAt       = q.value(6).toString();
        list.append(item);
    }
    return list;
}

int ExternalSourceManager::countItems(int sourceId)
{
    if (!Database::instance().isOpen() || sourceId <= 0) return 0;

    QSqlQuery q(Database::instance().connection());
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM external_source_items WHERE source_id = :sid;"));
    q.bindValue(QStringLiteral(":sid"), sourceId);
    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }
    return 0;
}

bool ExternalSourceManager::saveSource(const ExternalSourceDef &def,
                                       int *savedIdOut,
                                       QString *errorMessage)
{
    if (!Database::instance().isOpen()) {
        if (errorMessage) *errorMessage = QStringLiteral("Base de datos cerrada.");
        return false;
    }

    const bool isCreate = (def.id <= 0);
    QSqlQuery q(Database::instance().connection());

    if (isCreate) {
        q.prepare(QStringLiteral(
            "INSERT INTO external_sources ("
            "  name, source_type, source_uri, format, delimiter, "
            "  key_field, label_field, value_field, target_module, last_sync_at, is_active"
            ") VALUES ("
            "  :name, :stype, :uri, :fmt, :delim, :kf, :lf, :vf, :tmod, '', 1"
            ");"));
    } else {
        q.prepare(QStringLiteral(
            "UPDATE external_sources SET "
            "  name = :name, source_type = :stype, source_uri = :uri, format = :fmt, "
            "  delimiter = :delim, key_field = :kf, label_field = :lf, value_field = :vf, "
            "  target_module = :tmod, is_active = :active "
            "WHERE id = :id;"));
        q.bindValue(QStringLiteral(":id"), def.id);
        q.bindValue(QStringLiteral(":active"), def.isActive ? 1 : 0);
    }

    q.bindValue(QStringLiteral(":name"),  def.name.trimmed());
    q.bindValue(QStringLiteral(":stype"), def.sourceType.trimmed().toUpper());
    q.bindValue(QStringLiteral(":uri"),   def.sourceUri.trimmed());
    q.bindValue(QStringLiteral(":fmt"),   def.format.trimmed().toUpper());
    q.bindValue(QStringLiteral(":delim"), def.delimiter);
    q.bindValue(QStringLiteral(":kf"),    def.keyField.trimmed());
    q.bindValue(QStringLiteral(":lf"),    def.labelField.trimmed());
    q.bindValue(QStringLiteral(":vf"),    def.valueField.trimmed());
    q.bindValue(QStringLiteral(":tmod"),  def.targetModule.trimmed().isEmpty()
                                              ? QStringLiteral("ALL")
                                              : def.targetModule.trimmed());

    if (!q.exec()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No se pudo guardar la fuente externa:\n%1").arg(q.lastError().text());
        }
        return false;
    }

    int finalId = def.id;
    if (isCreate) {
        finalId = q.lastInsertId().toInt();
    }
    if (savedIdOut) *savedIdOut = finalId;

    QJsonObject newJson;
    newJson.insert(QStringLiteral("name"), def.name.trimmed());
    newJson.insert(QStringLiteral("source_type"), def.sourceType);
    newJson.insert(QStringLiteral("source_uri"), def.sourceUri.trimmed());
    newJson.insert(QStringLiteral("format"), def.format);
    newJson.insert(QStringLiteral("target_module"), def.targetModule);

    AuditEvent ev;
    ev.moduleId = QStringLiteral("panel_control");
    ev.targetTable = QStringLiteral("external_sources");
    ev.recordId = QString::number(finalId);
    ev.action = isCreate ? QStringLiteral("CREATE") : QStringLiteral("UPDATE");
    ev.newValuesJson = QString::fromUtf8(QJsonDocument(newJson).toJson(QJsonDocument::Compact));
    ev.details = QStringLiteral("Fuente externa '%1' (%2) → Módulo: %3")
                     .arg(def.name.trimmed(), def.format, moduleDisplayName(def.targetModule));
    Database::instance().logAuditEvent(ev);

    return true;
}

bool ExternalSourceManager::setSourceActive(int sourceId, bool active, QString *errorMessage)
{
    ExternalSourceDef def;
    if (!loadSourceById(sourceId, &def)) {
        if (errorMessage) *errorMessage = QStringLiteral("Fuente externa no encontrada.");
        return false;
    }

    QSqlQuery q(Database::instance().connection());
    q.prepare(QStringLiteral("UPDATE external_sources SET is_active = :a WHERE id = :id;"));
    q.bindValue(QStringLiteral(":a"), active ? 1 : 0);
    q.bindValue(QStringLiteral(":id"), sourceId);

    if (!q.exec()) {
        if (errorMessage) *errorMessage = q.lastError().text();
        return false;
    }

    AuditEvent ev;
    ev.moduleId = QStringLiteral("panel_control");
    ev.targetTable = QStringLiteral("external_sources");
    ev.recordId = QString::number(sourceId);
    ev.action = active ? QStringLiteral("REACTIVATE") : QStringLiteral("DEPRECATE");
    ev.details = QStringLiteral("Fuente externa '%1' marcada como %2")
                     .arg(def.name, active ? QStringLiteral("Activa") : QStringLiteral("Deprecada/Inactiva"));
    Database::instance().logAuditEvent(ev);
    return true;
}

bool ExternalSourceManager::fetchRawContent(const QString &sourceType,
                                            const QString &uri,
                                            QByteArray *rawOut,
                                            QString *errorMessage)
{
    const QString cleanUri = uri.trimmed();
    if (cleanUri.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("La URL o ruta de archivo no puede estar vacía.");
        return false;
    }

    if (sourceType.compare(QStringLiteral("FILE"), Qt::CaseInsensitive) == 0) {
        QFile file(cleanUri);
        if (!file.exists()) {
            if (errorMessage) *errorMessage = QStringLiteral("No se encontró el archivo:\n%1").arg(cleanUri);
            return false;
        }
        if (!file.open(QIODevice::ReadOnly)) {
            if (errorMessage) *errorMessage = QStringLiteral("No se pudo leer el archivo:\n%1").arg(file.errorString());
            return false;
        }
        if (rawOut) *rawOut = file.readAll();
        file.close();
        return true;
    }

    // Descarga HTTP / HTTPS con timeout seguro (compatible Qt 5 y Qt 6)
    const QUrl url(cleanUri);
    if (!url.isValid() || (url.scheme() != QStringLiteral("http") && url.scheme() != QStringLiteral("https"))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("La URL debe comenzar con http:// o https://");
        }
        return false;
    }

    QNetworkAccessManager nam;
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("GlobalManager/1.0"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = nam.get(req);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    bool timedOut = false;
    QObject::connect(&timer, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        reply->abort();
        loop.quit();
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    timer.start(kNetworkTimeoutMs);
    loop.exec();
    timer.stop();

    if (timedOut) {
        reply->deleteLater();
        if (errorMessage) {
            *errorMessage = QStringLiteral("Tiempo de espera agotado al consultar:\n%1").arg(cleanUri);
        }
        return false;
    }

    if (reply->error() != QNetworkReply::NoError) {
        const QString netErr = reply->errorString();
        reply->deleteLater();
        if (errorMessage) {
            *errorMessage = QStringLiteral("Error descargando datos desde la URL:\n%1").arg(netErr);
        }
        return false;
    }

    if (rawOut) {
        *rawOut = reply->readAll();
    }
    reply->deleteLater();
    return true;
}

bool ExternalSourceManager::parseContent(const QByteArray &rawData,
                                         const QString &format,
                                         const QString &delimiter,
                                         ExternalPreviewResult *previewOut,
                                         QString *errorMessage,
                                         int maxSampleRows)
{
    ExternalPreviewResult res;
    QSet<QString> seenFields;

    const QString fmt = format.trimmed().toUpper();

    if (fmt == QStringLiteral("JSON")) {
        QJsonParseError jErr;
        const QJsonDocument doc = QJsonDocument::fromJson(rawData, &jErr);
        if (jErr.error != QJsonParseError::NoError || doc.isNull()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("El contenido no es un JSON válido: %1").arg(jErr.errorString());
            }
            return false;
        }

        auto appendRow = [&](const QMap<QString, QString> &row) {
            ++res.totalParsedRows;
            if (maxSampleRows <= 0 || res.sampleRows.size() < maxSampleRows) {
                res.sampleRows.append(row);
            }
        };

        if (doc.isArray()) {
            const QJsonArray arr = doc.array();
            int idx = 0;
            for (const QJsonValue &item : arr) {
                QMap<QString, QString> row;
                if (item.isObject()) {
                    flattenJsonObject(item.toObject(), QString(), row, res.detectedFields, seenFields);
                } else {
                    addFieldOrderUnique(res.detectedFields, seenFields, QStringLiteral("index"));
                    addFieldOrderUnique(res.detectedFields, seenFields, QStringLiteral("value"));
                    row.insert(QStringLiteral("index"), QString::number(idx));
                    row.insert(QStringLiteral("value"), jsonValueToString(item));
                }
                appendRow(row);
                ++idx;
            }
        } else if (doc.isObject()) {
            const QJsonObject root = doc.object();

            // 1. Buscar si contiene un arreglo principal (ej: {"data": [...]} o {"items": [...]})
            QJsonArray nestedArr;
            for (auto it = root.begin(); it != root.end(); ++it) {
                if (it.value().isArray() && !it.value().toArray().isEmpty()) {
                    nestedArr = it.value().toArray();
                    break;
                }
            }

            if (!nestedArr.isEmpty()) {
                for (const QJsonValue &item : nestedArr) {
                    if (item.isObject()) {
                        QMap<QString, QString> row;
                        flattenJsonObject(item.toObject(), QString(), row, res.detectedFields, seenFields);
                        appendRow(row);
                    }
                }
            } else {
                // 2. Objeto clave -> subobjeto (ej: {"oficial": {"venta": 1000}, "blue": {"venta": 1200}})
                //    o clave -> valor directo (ej: {"USD": 1, "ARS": 1150, "BRL": 5.8})
                QJsonObject mapObj = root;
                // Si el objeto raíz tiene un subobjeto "rates" o "cotizaciones", desempaquetarlo automáticamente
                if (root.contains(QStringLiteral("rates")) && root.value(QStringLiteral("rates")).isObject()) {
                    mapObj = root.value(QStringLiteral("rates")).toObject();
                }

                for (auto it = mapObj.begin(); it != mapObj.end(); ++it) {
                    QMap<QString, QString> row;
                    addFieldOrderUnique(res.detectedFields, seenFields, QStringLiteral("_key"));
                    row.insert(QStringLiteral("_key"), it.key());

                    if (it.value().isObject()) {
                        flattenJsonObject(it.value().toObject(), QString(), row, res.detectedFields, seenFields);
                    } else if (!it.value().isArray()) {
                        addFieldOrderUnique(res.detectedFields, seenFields, QStringLiteral("_value"));
                        row.insert(QStringLiteral("_value"), jsonValueToString(it.value()));
                    } else {
                        continue;
                    }
                    appendRow(row);
                }
            }
        }
    } else {
        // CSV o TXT (texto plano delimitado o clave=valor)
        QString text = QString::fromUtf8(rawData);
        if (text.startsWith(QChar(0xFEFF))) {
            text.remove(0, 1); // Quitar BOM UTF-8
        }
        text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        text.replace(QLatin1Char('\r'), QLatin1Char('\n'));

        const QStringList rawLines = text.split(QLatin1Char('\n'));
        QStringList lines;
        lines.reserve(rawLines.size());
        for (const QString &l : rawLines) {
            if (!l.trimmed().isEmpty()) {
                lines.append(l);
            }
        }

        if (lines.isEmpty()) {
            if (errorMessage) *errorMessage = QStringLiteral("La fuente no contiene líneas de datos.");
            return false;
        }

        const QChar sep = resolveDelimiterChar(delimiter);
        const bool looksLikeKeyValue =
            (fmt == QStringLiteral("TXT")) &&
            !lines.first().contains(sep) &&
            lines.first().contains(QLatin1Char('='));

        if (looksLikeKeyValue) {
            res.detectedFields = QStringList{QStringLiteral("clave"), QStringLiteral("valor")};
            for (const QString &line : lines) {
                const int eqPos = line.indexOf(QLatin1Char('='));
                if (eqPos <= 0) continue;
                QMap<QString, QString> row;
                row.insert(QStringLiteral("clave"), line.left(eqPos).trimmed());
                row.insert(QStringLiteral("valor"), line.mid(eqPos + 1).trimmed());
                ++res.totalParsedRows;
                if (maxSampleRows <= 0 || res.sampleRows.size() < maxSampleRows) {
                    res.sampleRows.append(row);
                }
            }
        } else {
            const QStringList firstCols = splitCsvLine(lines.first(), sep);
            QStringList headers;
            for (int c = 0; c < firstCols.size(); ++c) {
                QString h = firstCols.at(c).trimmed();
                if (h.isEmpty()) h = QStringLiteral("Col_%1").arg(c + 1);
                headers.append(h);
                addFieldOrderUnique(res.detectedFields, seenFields, h);
            }

            for (int i = 1; i < lines.size(); ++i) {
                const QStringList cols = splitCsvLine(lines.at(i), sep);
                QMap<QString, QString> row;
                for (int c = 0; c < headers.size(); ++c) {
                    row.insert(headers.at(c), c < cols.size() ? cols.at(c) : QString());
                }
                ++res.totalParsedRows;
                if (maxSampleRows <= 0 || res.sampleRows.size() < maxSampleRows) {
                    res.sampleRows.append(row);
                }
            }
        }
    }

    if (res.totalParsedRows == 0 || res.detectedFields.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No se pudieron interpretar filas de datos. Revisa el formato y el separador.");
        }
        return false;
    }

    if (previewOut) {
        *previewOut = res;
    }
    return true;
}

bool ExternalSourceManager::syncSource(int sourceId, int *syncedRowsOut, QString *errorMessage)
{
    ExternalSourceDef def;
    if (!loadSourceById(sourceId, &def)) {
        if (errorMessage) *errorMessage = QStringLiteral("No se encontró la fuente externa seleccionada.");
        return false;
    }

    QByteArray raw;
    if (!fetchRawContent(def.sourceType, def.sourceUri, &raw, errorMessage)) {
        return false;
    }

    ExternalPreviewResult parsed;
    if (!parseContent(raw, def.format, def.delimiter, &parsed, errorMessage, 0)) {
        return false;
    }

    const QString keyField   = def.keyField.trimmed().isEmpty() ? parsed.detectedFields.first() : def.keyField.trimmed();
    const QString labelField = def.labelField.trimmed().isEmpty() ? keyField : def.labelField.trimmed();
    const QString valueField = def.valueField.trimmed();

    QSqlDatabase db = Database::instance().connection();
    const bool sqlite = Database::instance().isSqlite();

    db.transaction();

    const QString upsertSql = sqlite
        ? QStringLiteral(
              "INSERT INTO external_source_items (source_id, item_key, item_label, value_num, value_text, updated_at) "
              "VALUES (:sid, :ikey, :ilabel, :vnum, :vtxt, CURRENT_TIMESTAMP) "
              "ON CONFLICT(source_id, item_key) DO UPDATE SET "
              "  item_label = excluded.item_label, "
              "  value_num  = excluded.value_num, "
              "  value_text = excluded.value_text, "
              "  updated_at = CURRENT_TIMESTAMP;")
        : QStringLiteral(
              "INSERT INTO external_source_items (source_id, item_key, item_label, value_num, value_text, updated_at) "
              "VALUES (:sid, :ikey, :ilabel, :vnum, :vtxt, CURRENT_TIMESTAMP) "
              "ON DUPLICATE KEY UPDATE "
              "  item_label = VALUES(item_label), "
              "  value_num  = VALUES(value_num), "
              "  value_text = VALUES(value_text), "
              "  updated_at = CURRENT_TIMESTAMP;");

    QSqlQuery upsertQ(db);
    upsertQ.prepare(upsertSql);

    int synced = 0;
    for (int i = 0; i < parsed.sampleRows.size(); ++i) {
        const QMap<QString, QString> &row = parsed.sampleRows.at(i);
        QString key = row.value(keyField).trimmed();
        if (key.isEmpty()) {
            key = QStringLiteral("ROW_%1").arg(i + 1);
        }
        QString label = row.value(labelField).trimmed();
        if (label.isEmpty()) {
            label = key;
        }
        const QString valStr = valueField.isEmpty() ? QString() : row.value(valueField).trimmed();

        double num = 0.0;
        const bool isNum = tryParseNumber(valStr, &num);

        upsertQ.bindValue(QStringLiteral(":sid"),    sourceId);
        upsertQ.bindValue(QStringLiteral(":ikey"),   key.left(180));
        upsertQ.bindValue(QStringLiteral(":ilabel"), label.left(480));
        upsertQ.bindValue(QStringLiteral(":vnum"),   isNum ? QVariant(num) : QVariant());
        upsertQ.bindValue(QStringLiteral(":vtxt"),   valStr.left(480));

        if (!upsertQ.exec()) {
            db.rollback();
            if (errorMessage) {
                *errorMessage = QStringLiteral("Error guardando datos sincronizados:\n%1").arg(upsertQ.lastError().text());
            }
            return false;
        }
        ++synced;
    }

    const QString syncStamp = QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yyyy HH:mm:ss"));
    QSqlQuery updSrc(db);
    updSrc.prepare(QStringLiteral("UPDATE external_sources SET last_sync_at = :ts WHERE id = :id;"));
    updSrc.bindValue(QStringLiteral(":ts"), syncStamp);
    updSrc.bindValue(QStringLiteral(":id"), sourceId);
    updSrc.exec();

    if (!db.commit()) {
        db.rollback();
        if (errorMessage) *errorMessage = db.lastError().text();
        return false;
    }

    if (syncedRowsOut) *syncedRowsOut = synced;

    AuditEvent ev;
    ev.moduleId = QStringLiteral("panel_control");
    ev.targetTable = QStringLiteral("external_source_items");
    ev.recordId = QString::number(sourceId);
    ev.action = QStringLiteral("SYNC");
    ev.details = QStringLiteral("Sincronización '%1' (%2): %3 registro(s) actualizados")
                     .arg(def.name, def.sourceUri)
                     .arg(synced);
    Database::instance().logAuditEvent(ev);
    return true;
}
