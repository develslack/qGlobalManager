#include "icons.h"

#include <QByteArray>
#include <QPainter>
#include <QPixmap>
#include <QString>
#include <QSvgRenderer>

namespace Icons {

namespace {

QString svgInnerContent(Name iconName)
{
    switch (iconName) {
    case Name::Menu:
        return QStringLiteral(
            "<line x1='3' y1='6' x2='21' y2='6'/>"
            "<line x1='3' y1='12' x2='21' y2='12'/>"
            "<line x1='3' y1='18' x2='21' y2='18'/>");
    case Name::Home:
        return QStringLiteral(
            "<path d='M3 10.5L12 3l9 7.5'/>"
            "<path d='M5 9.5V20h14V9.5'/>"
            "<path d='M9.5 20v-6h5v6'/>");
    case Name::ControlPanel:
        return QStringLiteral(
            "<circle cx='12' cy='12' r='3'/>"
            "<path d='M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 1 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 1 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 1 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9c.26.6.85 1 1.51 1H21a2 2 0 1 1 0 4h-.09c-.66 0-1.25.4-1.51 1z'/>");
    case Name::Logout:
        return QStringLiteral(
            "<path d='M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4'/>"
            "<polyline points='16 17 21 12 16 7'/>"
            "<line x1='21' y1='12' x2='9' y2='12'/>");
    case Name::Login:
        return QStringLiteral(
            "<path d='M15 3h4a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2h-4'/>"
            "<polyline points='10 17 15 12 10 7'/>"
            "<line x1='15' y1='12' x2='3' y2='12'/>");
    case Name::Close:
        return QStringLiteral(
            "<line x1='18' y1='6' x2='6' y2='18'/>"
            "<line x1='6' y1='6' x2='18' y2='18'/>");
    case Name::Users:
        return QStringLiteral(
            "<path d='M17 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2'/>"
            "<circle cx='9' cy='7' r='4'/>"
            "<path d='M23 21v-2a4 4 0 0 0-3-3.87'/>"
            "<path d='M16 3.13a4 4 0 0 1 0 7.75'/>");
    case Name::Role:
        return QStringLiteral(
            "<rect x='3' y='4' width='18' height='16' rx='2'/>"
            "<circle cx='9' cy='10' r='2'/>"
            "<path d='M6 16c0-1.7 1.3-3 3-3s3 1.3 3 3'/>"
            "<line x1='14' y1='9' x2='18' y2='9'/>"
            "<line x1='14' y1='13' x2='18' y2='13'/>");
    case Name::Shield:
        return QStringLiteral(
            "<path d='M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z'/>");
    case Name::Key:
        return QStringLiteral(
            "<circle cx='7.5' cy='15.5' r='4.5'/>"
            "<path d='M21 2l-9.6 9.6'/>"
            "<path d='M15.5 7.5l3 3L22 7l-3-3'/>");
    case Name::Lock:
        return QStringLiteral(
            "<rect x='4' y='11' width='16' height='10' rx='2'/>"
            "<path d='M8 11V7a4 4 0 0 1 8 0v4'/>");
    case Name::Unlock:
        return QStringLiteral(
            "<rect x='4' y='11' width='16' height='10' rx='2'/>"
            "<path d='M8 11V7a4 4 0 0 1 7.4-2'/>");
    case Name::Company:
        return QStringLiteral(
            "<rect x='4' y='2' width='16' height='20' rx='2'/>"
            "<line x1='9' y1='7' x2='9.01' y2='7'/>"
            "<line x1='15' y1='7' x2='15.01' y2='7'/>"
            "<line x1='9' y1='12' x2='9.01' y2='12'/>"
            "<line x1='15' y1='12' x2='15.01' y2='12'/>"
            "<path d='M9 22v-4h6v4'/>");
    case Name::Mail:
        return QStringLiteral(
            "<rect x='2' y='4' width='20' height='16' rx='2'/>"
            "<polyline points='22,6 12,13 2,6'/>");
    case Name::Send:
        return QStringLiteral(
            "<line x1='22' y1='2' x2='11' y2='13'/>"
            "<polygon points='22 2 15 22 11 13 2 9 22 2'/>");
    case Name::Printer:
        return QStringLiteral(
            "<polyline points='6 9 6 2 18 2 18 9'/>"
            "<path d='M6 18H4a2 2 0 0 1-2-2v-5a2 2 0 0 1 2-2h16a2 2 0 0 1 2 2v5a2 2 0 0 1-2 2h-2'/>"
            "<rect x='6' y='14' width='12' height='8'/>");
    case Name::Database:
        return QStringLiteral(
            "<ellipse cx='12' cy='5' rx='9' ry='3'/>"
            "<path d='M21 12c0 1.66-4 3-9 3s-9-1.34-9-3'/>"
            "<path d='M3 5v14c0 1.66 4 3 9 3s9-1.34 9-3V5'/>");
    case Name::Backup:
        return QStringLiteral(
            "<path d='M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4'/>"
            "<polyline points='7 10 12 15 17 10'/>"
            "<line x1='12' y1='15' x2='12' y2='3'/>");
    case Name::Table:
        return QStringLiteral(
            "<rect x='3' y='4' width='18' height='16' rx='2'/>"
            "<line x1='3' y1='10' x2='21' y2='10'/>"
            "<line x1='9' y1='4' x2='9' y2='20'/>");
    case Name::Add:
        return QStringLiteral(
            "<line x1='12' y1='5' x2='12' y2='19'/>"
            "<line x1='5' y1='12' x2='19' y2='12'/>");
    case Name::Edit:
        return QStringLiteral(
            "<path d='M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7'/>"
            "<path d='M18.5 2.5a2.12 2.12 0 0 1 3 3L12 15l-4 1 1-4 9.5-9.5z'/>");
    case Name::Save:
        return QStringLiteral(
            "<path d='M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z'/>"
            "<polyline points='17 21 17 13 7 13 7 21'/>"
            "<polyline points='7 3 7 8 15 8'/>");
    case Name::Delete:
        return QStringLiteral(
            "<polyline points='3 6 5 6 21 6'/>"
            "<path d='M19 6l-1 14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6'/>"
            "<path d='M10 11v6'/>"
            "<path d='M14 11v6'/>"
            "<path d='M9 6V4a1 1 0 0 1 1-1h4a1 1 0 0 1 1 1v2'/>");
    case Name::Search:
        return QStringLiteral(
            "<circle cx='11' cy='11' r='7'/>"
            "<line x1='21' y1='21' x2='16.65' y2='16.65'/>");
    case Name::Refresh:
        return QStringLiteral(
            "<polyline points='23 4 23 10 17 10'/>"
            "<path d='M20.49 15a9 9 0 1 1-2.12-9.36L23 10'/>");
    case Name::Check:
        return QStringLiteral(
            "<polyline points='20 6 9 17 4 12'/>");
    case Name::Play:
        return QStringLiteral(
            "<polygon points='6 4 20 12 6 20 6 4'/>");
    }
    return QString();
}

} // namespace

QIcon get(Name iconName, const QColor &color, int sizePx)
{
    const QString svg = QStringLiteral(
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' "
        "fill='none' stroke='%1' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'>"
        "%2</svg>")
                            .arg(color.name(QColor::HexRgb), svgInnerContent(iconName));

    QSvgRenderer renderer(svg.toUtf8());
    QPixmap pixmap(sizePx, sizePx);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    renderer.render(&painter);
    painter.end();

    return QIcon(pixmap);
}

} // namespace Icons
