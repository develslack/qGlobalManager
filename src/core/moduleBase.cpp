#include "moduleBase.h"
#include "session.h"
#include "externalSourceManager.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QVBoxLayout>

ModuleBase::ModuleBase(QWidget *parent)
    : QWidget(parent)
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 14, 16, 16);
    rootLayout->setSpacing(14);

    // Banner corporativo superior estándar para todos los módulos
    auto *banner = new QFrame(this);
    banner->setObjectName(QStringLiteral("ModuleBanner"));
    banner->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);

    auto *bannerLayout = new QHBoxLayout(banner);
    bannerLayout->setContentsMargins(16, 12, 16, 12);
    bannerLayout->setSpacing(14);

    m_bannerIcon = new QLabel(banner);
    m_bannerIcon->setFixedSize(44, 44);
    m_bannerIcon->setScaledContents(true);
    bannerLayout->addWidget(m_bannerIcon);

    auto *textLayout = new QVBoxLayout();
    textLayout->setSpacing(2);

    m_bannerTitle = new QLabel(banner);
    m_bannerTitle->setWordWrap(true);
    m_bannerTitle->setStyleSheet(QStringLiteral("font-size: 16pt; font-weight: 700; color: #FFFFFF;"));

    m_bannerSubtitle = new QLabel(banner);
    m_bannerSubtitle->setWordWrap(true);
    m_bannerSubtitle->setStyleSheet(QStringLiteral("font-size: 10pt; color: #DCE6F1;"));

    textLayout->addWidget(m_bannerTitle);
    textLayout->addWidget(m_bannerSubtitle);
    bannerLayout->addLayout(textLayout, 1);

    rootLayout->addWidget(banner);

    // Contenedor responsivo del cuerpo del módulo
    auto *bodyContainer = new QWidget(this);
    bodyContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_bodyLayout = new QVBoxLayout(bodyContainer);
    m_bodyLayout->setContentsMargins(0, 0, 0, 0);
    m_bodyLayout->setSpacing(12);

    rootLayout->addWidget(bodyContainer, 1);
}


// ============================================================================================================================================= //


void ModuleBase::initBanner(const QString &title,
                            const QString &subtitle,
                            const QString &iconResource)
{
    m_bannerTitle->setText(title);
    m_bannerSubtitle->setText(subtitle);

    m_externalSourcesBar = new QFrame(this);
    m_externalSourcesBar->setStyleSheet(
        QStringLiteral("QFrame { background-color: #EEF4FA; border: 1px solid #D9E2EC; border-radius: 6px; }"));
    auto *extBarLayout = new QHBoxLayout(m_externalSourcesBar);
    extBarLayout->setContentsMargins(10, 5, 10, 5);
    extBarLayout->setSpacing(8);

    m_externalSourcesLabel = new QLabel(m_externalSourcesBar);
    m_externalSourcesLabel->setWordWrap(true);
    m_externalSourcesLabel->setStyleSheet(QStringLiteral("color: #1E3A5F; font-size: 9pt; border: none; background: transparent;"));
    extBarLayout->addWidget(m_externalSourcesLabel, 1);

    m_externalSourcesBar->hide();
    m_bodyLayout->addWidget(m_externalSourcesBar);

    const QPixmap pix = QIcon(iconResource).pixmap(44, 44);
    if (!pix.isNull()) {
        m_bannerIcon->setPixmap(pix);
        m_bannerIcon->show();
    } else {
        m_bannerIcon->hide();
    }
}


// ============================================================================================================================================= //


QVBoxLayout *ModuleBase::bodyLayout() const
{
    return m_bodyLayout;
}


// ============================================================================================================================================= //


bool ModuleBase::requirePermission(const QString &action, const QString &taskDescription)
{
    if (Session::instance().hasPermission(moduleId(), action)) {
        return true;
    }

    const QString taskText = taskDescription.isEmpty()
                                 ? QStringLiteral("realizar esta tarea")
                                 : taskDescription;

    QMessageBox::warning(
        this,
        QStringLiteral("Permiso denegado"),
        QStringLiteral("Tu usuario (%1) no tiene permisos para %2 en el módulo \"%3\".\n\n"
                       "Si necesitas acceso, solicítalo al administrador del sistema.")
            .arg(Session::instance().currentUser().email, taskText, moduleTitle()));
    return false;
}


// ============================================================================================================================================= //


void ModuleBase::refreshExternalSourcesBar()
{
    if (!m_externalSourcesBar || !m_externalSourcesLabel) {
        return;
    }

    const QVector<ExternalSourceDef> linked = ExternalSourceManager::loadSourcesForModule(moduleId());
    if (linked.isEmpty()) {
        m_externalSourcesBar->hide();
        return;
    }

    QStringList chunks;
    for (const ExternalSourceDef &src : linked) {
        const QVector<ExternalSourceItem> items = ExternalSourceManager::loadItems(src.id);
        if (items.isEmpty()) continue;

        QStringList previewPairs;
        const int limit = qMin(5, items.size());
        for (int i = 0; i < limit; ++i) {
            const ExternalSourceItem &it = items.at(i);
            const QString val = it.hasNumericValue
                                    ? QString::number(it.valueNum, 'f', 2)
                                    : it.valueText;
            previewPairs.append(val.isEmpty()
                                    ? it.itemLabel.toHtmlEscaped()
                                    : QStringLiteral("%1: <b>%2</b>").arg(it.itemLabel.toHtmlEscaped(), val.toHtmlEscaped()));
        }
        if (items.size() > limit) {
            previewPairs.append(QStringLiteral("(+%1 más)").arg(items.size() - limit));
        }
        chunks.append(QStringLiteral("<b>%1:</b> %2").arg(src.name.toHtmlEscaped(), previewPairs.join(QStringLiteral(" · "))));
    }

    if (chunks.isEmpty()) {
        m_externalSourcesBar->hide();
        return;
    }

    m_externalSourcesLabel->setText(chunks.join(QStringLiteral(" &nbsp;|&nbsp; ")));
    m_externalSourcesBar->show();
}
