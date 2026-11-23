#ifndef CORE_ELEVATIONMANAGER_H
#define CORE_ELEVATIONMANAGER_H

#include <QObject>
#include <QString>
#include <QtGlobal>

class QTimer;

struct ElevationPolicy
{
    int durationMinutes = 20;
    bool allowSupportTables = true;
    bool allowBackupAudit = true;
    bool allowDbAdmin = true;
    bool notifyByEmail = false;
};

struct ActiveElevation
{
    bool active = false;
    QString tokenCode;
    QString scope;            // "support_tables", "backup_audit", "db_admin"
    QString scopeDisplayName;
    QString reason;
    int userId = -1;
    QString userEmail;
    QString userFullName;
    qint64 issuedAtEpoch = 0;
    qint64 expiresAtEpoch = 0;
};

class ElevationManager : public QObject
{
    Q_OBJECT

public:
    static ElevationManager &instance();

    static ElevationPolicy loadPolicy();
    static bool savePolicy(const ElevationPolicy &policy, QString *errorMessage = nullptr);
    static QString scopeDisplayName(const QString &scope);

    bool isScopeAllowedByPolicy(const QString &scope) const;
    bool canCurrentUserRequestElevation(const QString &scope, QString *denialReasonOut = nullptr) const;

    bool isElevationActive() const;
    bool hasActiveElevationFor(const QString &scope) const;
    const ActiveElevation &activeElevation() const;
    int remainingSeconds() const;

    // Contexto que graba automáticamente Database::logAuditEvent
    QString activeAuthContext() const;

    // Autogeneración desatendida (estilo sudo: revalidación de contraseña + motivo)
    bool requestSelfElevation(const QString &scope,
                              const QString &userPassword,
                              const QString &reason,
                              QString *errorMessage = nullptr);

    void revokeActiveElevation(bool expired = false);

signals:
    void elevationChanged(bool active);
    void elevationTick(int remainingSeconds);
    void elevationExpired(const QString &scopeDisplayName);

private slots:
    void onTimerTick();

private:
    explicit ElevationManager(QObject *parent = nullptr);
    ElevationManager(const ElevationManager &) = delete;
    ElevationManager &operator=(const ElevationManager &) = delete;

    static QString generateTokenCode();
    void sendAuditNotificationEmailAsync(const ActiveElevation &ev);

    QTimer *m_timer = nullptr;
    ActiveElevation m_active;
};

#endif // CORE_ELEVATIONMANAGER_H
