#ifndef CORE_SESSION_H
#define CORE_SESSION_H

#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

struct UserSessionInfo
{
    int id = -1;
    QString email;
    QString fullName;
    int roleId = -1;
    QString roleName;
    bool isAdmin = false;
    bool mustChangePassword = false;
    QSet<QString> permissions; // "module_id:action"
};

class Session : public QObject
{
    Q_OBJECT

public:
    static Session &instance();

    bool login(const QString &email, const QString &password, QString *errorMessage = nullptr);
    void logout(bool expiredByInactivity = false);

    bool isAuthenticated() const;
    const UserSessionInfo &currentUser() const;
    bool mustChangePassword() const;
    void markPasswordChanged();

    bool hasPermission(const QString &moduleId, const QString &action = QStringLiteral("access")) const;

    void configureInactivityTimeouts(int idleSeconds, int warningSeconds);
    void reloadInactivityFromDatabase();
    void keepAlive();

signals:
    void sessionStarted();
    void sessionEnded(bool expiredByInactivity);
    void passwordPolicyResolved();
    void inactivityWarningStarted(int secondsRemaining);
    void inactivityCountdownTick(int secondsRemaining);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onIdleTimeout();
    void onWarningTick();

private:
    explicit Session(QObject *parent = nullptr);
    ~Session() override = default;
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;

    void loadUserPermissions(int userId, int roleId);
    void startInactivityWatch();
    void stopInactivityWatch();

    UserSessionInfo m_user;
    bool m_authenticated = false;
    bool m_warningActive = false;

    int m_idleSeconds = 15 * 60;
    int m_warningSeconds = 30;
    int m_remainingSeconds = 0;

    QTimer m_idleTimer;
    QTimer m_warningTimer;
};

#endif // CORE_SESSION_H
