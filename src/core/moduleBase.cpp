#include "moduleBase.h"
#include "session.h"

#include <QFrame>
#include <QHBoxLayout>
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

void ModuleBase::initBanner(const QString &title,
                            const QString &subtitle,
                            const QString &iconResource)
{
    m_bannerTitle->setText(title);
    m_bannerSubtitle->setText(subtitle);

    const QPixmap pix = QIcon(iconResource).pixmap(44, 44);
    if (!pix.isNull()) {
        m_bannerIcon->setPixmap(pix);
        m_bannerIcon->show();
    } else {
        m_bannerIcon->hide();
    }
}

QVBoxLayout *ModuleBase::bodyLayout() const
{
    return m_bodyLayout;
}

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
