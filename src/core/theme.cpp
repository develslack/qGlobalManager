#include "theme.h"

#include <QApplication>
#include <QFont>

namespace Theme {

void apply(QApplication &app)
{
    QFont baseFont = app.font();
    if (baseFont.pointSize() < 10) {
        baseFont.setPointSize(10);
    }
    app.setFont(baseFont);
    app.setStyleSheet(styleSheet());
}

QString styleSheet()
{
    return QStringLiteral(R"(
        QWidget {
            color: #1F2933;
            background-color: #F4F7FA;
            selection-background-color: #2B7A78;
            selection-color: #FFFFFF;
        }

        QMainWindow, QDialog {
            background-color: #F4F7FA;
        }

        QFrame#TopBar {
            background-color: #1E3A5F;
            border: none;
        }

        QFrame#TopBar QLabel {
            color: #FFFFFF;
            background-color: transparent;
        }

        QFrame#TopBar QPushButton {
            background-color: #274C77;
            color: #FFFFFF;
            border: 1px solid #3B6696;
            border-radius: 5px;
            padding: 5px 12px;
            font-weight: 600;
        }

        QFrame#TopBar QPushButton:hover {
            background-color: #2B7A78;
            border-color: #2B7A78;
        }

        QFrame#TopBar QPushButton#ControlPanelBtn {
            background-color: #2B7A78;
            color: #FFFFFF;
            border: 1px solid #3AAFA9;
            border-radius: 5px;
            padding: 5px 12px;
            font-weight: 600;
        }

        QFrame#TopBar QPushButton#ControlPanelBtn:hover {
            background-color: #226361;
        }

        QWidget#NavPanel {
            background-color: #EAF0F6;
            border-right: 1px solid #D9E2EC;
        }

        QWidget#NavPanel QLabel {
            background-color: transparent;
            color: #1E3A5F;
        }

        QListWidget#SidebarList {
            background-color: #EAF0F6;
            border: none;
            outline: none;
            padding: 4px;
        }

        QListWidget#SidebarList::item {
            padding: 9px 12px;
            margin: 2px 0px;
            border-radius: 6px;
            color: #1F2933;
        }

        QListWidget#SidebarList::item:hover {
            background-color: #DCE6F1;
        }

        QListWidget#SidebarList::item:selected {
            background-color: #1E3A5F;
            color: #FFFFFF;
            font-weight: 600;
        }

        QFrame#ModuleBanner {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                                        stop:0 #1E3A5F, stop:1 #2B7A78);
            border-radius: 8px;
        }

        QFrame#ModuleBanner QLabel {
            color: #FFFFFF;
            background-color: transparent;
        }

        QFrame#CardFrame, QGroupBox {
            background-color: #FFFFFF;
            border: 1px solid #D9E2EC;
            border-radius: 8px;
        }

        QGroupBox {
            margin-top: 14px;
            padding: 14px 12px 12px 12px;
            font-weight: 600;
            color: #1E3A5F;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 12px;
            padding: 0 6px;
            background-color: #F4F7FA;
        }

        QLabel {
            background-color: transparent;
        }

        QLineEdit, QTextEdit, QPlainTextEdit, QComboBox, QSpinBox, QDateEdit {
            background-color: #FFFFFF;
            border: 1px solid #CBD2D9;
            border-radius: 5px;
            padding: 6px 8px;
            min-height: 20px;
        }

        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus,
        QComboBox:focus, QSpinBox:focus, QDateEdit:focus {
            border: 1.5px solid #2B7A78;
        }

        QPushButton {
            background-color: #2B7A78;
            color: #FFFFFF;
            border: none;
            border-radius: 5px;
            padding: 7px 14px;
            font-weight: 600;
        }

        QPushButton:hover {
            background-color: #226361;
        }

        QPushButton:pressed {
            background-color: #1B4F4D;
        }

        QPushButton:disabled {
            background-color: #BCCCDC;
            color: #616E7C;
        }

        QPushButton[secondary="true"] {
            background-color: #E4EBF2;
            color: #1E3A5F;
            border: 1px solid #CBD2D9;
        }

        QPushButton[secondary="true"]:hover {
            background-color: #D6E1EC;
        }

        QPushButton[danger="true"] {
            background-color: #C53030;
            color: #FFFFFF;
        }

        QPushButton[danger="true"]:hover {
            background-color: #9B2C2C;
        }

        QTabWidget::pane {
            border: 1px solid #D9E2EC;
            background-color: #FFFFFF;
            border-radius: 6px;
            top: -1px;
        }

        QTabBar::tab {
            background-color: #E4EBF2;
            color: #334E68;
            padding: 8px 16px;
            margin-right: 3px;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            font-weight: 600;
        }

        QTabBar::tab:selected {
            background-color: #FFFFFF;
            color: #1E3A5F;
            border: 1px solid #D9E2EC;
            border-bottom: 2px solid #2B7A78;
        }

        QTableView, QTableWidget {
            background-color: #FFFFFF;
            alternate-background-color: #F8FAFC;
            gridline-color: #E4EBF2;
            border: 1px solid #D9E2EC;
            border-radius: 6px;
        }

        QHeaderView::section {
            background-color: #1E3A5F;
            color: #FFFFFF;
            padding: 6px 8px;
            border: none;
            font-weight: 600;
        }

        QStatusBar {
            background-color: #EAF0F6;
            color: #334E68;
            border-top: 1px solid #D9E2EC;
        }
    )");
}

} // namespace Theme
