#ifndef CORE_MODULEBASE_H
#define CORE_MODULEBASE_H

#include <QString>
#include <QWidget>
#include <QFrame>

class QLabel;
class QVBoxLayout;

class ModuleBase : public QWidget
{
    Q_OBJECT

public:
    explicit ModuleBase(QWidget *parent = nullptr);
    ~ModuleBase() override = default;

    virtual QString moduleId() const = 0;
    virtual QString moduleTitle() const = 0;
    virtual void onActivated() {}

    void refreshExternalSourcesBar();

protected:
    void initBanner(const QString &title,
                    const QString &subtitle,
                    const QString &iconResource = QStringLiteral(":/icons/qGlobalManager.svg"));
    QVBoxLayout *bodyLayout() const;

    bool requirePermission(const QString &action, const QString &taskDescription = QString());


private:
    QLabel *m_bannerIcon = nullptr;
    QLabel *m_bannerTitle = nullptr;
    QLabel *m_bannerSubtitle = nullptr;
    QVBoxLayout *m_bodyLayout = nullptr;

    // EXTERNAL SOURCES
    QFrame *m_externalSourcesBar = nullptr;
    QLabel *m_externalSourcesLabel = nullptr;
};

#endif // CORE_MODULEBASE_H
