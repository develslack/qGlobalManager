#ifndef CORE_EXTERNALSOURCEMANAGER_H
#define CORE_EXTERNALSOURCEMANAGER_H

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

struct ExternalSourceDef
{
    int id = -1;
    QString name;
    QString sourceType = QStringLiteral("URL");   // "URL" o "FILE"
    QString sourceUri;
    QString format = QStringLiteral("JSON");      // "JSON", "CSV", "TXT"
    QString delimiter = QStringLiteral(";");      // ";", ",", "\t", "|"
    QString keyField;
    QString labelField;
    QString valueField;
    QString targetModule = QStringLiteral("ALL"); // "ALL" o moduleId() específico
    QString lastSyncAt;
    bool isActive = true;
};

struct ExternalSourceItem
{
    int id = -1;
    int sourceId = -1;
    QString itemKey;
    QString itemLabel;
    bool hasNumericValue = false;
    double valueNum = 0.0;
    QString valueText;
    QString updatedAt;
};

struct ExternalPreviewResult
{
    QStringList detectedFields;
    QVector<QMap<QString, QString>> sampleRows;
    int totalParsedRows = 0;
};

class ExternalSourceManager
{
public:
    static QVector<ExternalSourceDef> loadSources(bool includeInactive = true);
    static QVector<ExternalSourceDef> loadSourcesForModule(const QString &moduleId);
    static bool loadSourceById(int sourceId, ExternalSourceDef *out);
    static QVector<ExternalSourceItem> loadItems(int sourceId);
    static int countItems(int sourceId);

    static bool saveSource(const ExternalSourceDef &def,
                           int *savedIdOut = nullptr,
                           QString *errorMessage = nullptr);
    static bool setSourceActive(int sourceId, bool active, QString *errorMessage = nullptr);

    // Lectura y previsualización agnóstica
    static bool fetchRawContent(const QString &sourceType,
                                const QString &uri,
                                QByteArray *rawOut,
                                QString *errorMessage = nullptr);

    static bool parseContent(const QByteArray &rawData,
                             const QString &format,
                             const QString &delimiter,
                             ExternalPreviewResult *previewOut,
                             QString *errorMessage = nullptr,
                             int maxSampleRows = 200);

    // Sincronización completa hacia external_source_items + auditoría
    static bool syncSource(int sourceId,
                           int *syncedRowsOut = nullptr,
                           QString *errorMessage = nullptr);

    static QString moduleDisplayName(const QString &moduleId);
};

#endif // CORE_EXTERNALSOURCEMANAGER_H
