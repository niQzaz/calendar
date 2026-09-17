#include <QApplication>
#include <QPalette>

#include "ui/MainWindow.h"

namespace {

// Простая тёмная тема: базовая палитра Qt + немного QSS для акцентов
// (подсветка сегодняшнего дня, выбранного дня, дней с событиями).
// Отдельный класс "ThemeManager" на этом этапе был бы избыточным
// усложнением - тема настраивается в одном месте при старте приложения.
void applyDarkTheme(QApplication &app)
{
    app.setStyle("Fusion");

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(30, 30, 34));
    palette.setColor(QPalette::WindowText, Qt::white);
    palette.setColor(QPalette::Base, QColor(24, 24, 28));
    palette.setColor(QPalette::AlternateBase, QColor(40, 40, 46));
    palette.setColor(QPalette::Text, Qt::white);
    palette.setColor(QPalette::Button, QColor(45, 45, 52));
    palette.setColor(QPalette::ButtonText, Qt::white);
    palette.setColor(QPalette::Highlight, QColor(94, 129, 244));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(palette);

    const QString qss = R"(
        QPushButton#dayButton {
            border: none;
            border-radius: 6px;
            background-color: #2d2d34;
        }
        QPushButton#dayButton:hover {
            background-color: #3a3a44;
        }
        QPushButton#dayButton:checked {
            background-color: #5e81f4;
            font-weight: bold;
        }
        QPushButton#dayButton[otherMonth="true"] {
            color: #666666;
        }
        QPushButton#dayButton[isToday="true"] {
            border: 1px solid #5e81f4;
        }
        QPushButton#dayButton[hasEvents="true"] {
            border-bottom: 3px solid #f4b45e;
        }
        QLabel#monthLabel {
            font-size: 16px;
            font-weight: bold;
        }
        QLabel#weekDayLabel {
            color: #999999;
        }
        QLabel#selectedDateLabel {
            font-size: 14px;
            font-weight: bold;
            padding-bottom: 4px;
        }
        QLabel#hintLabel {
            color: #888888;
            font-size: 11px;
        }
    )";
    app.setStyleSheet(qss);
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    applyDarkTheme(app);

    MainWindow window;
    window.show();

    return app.exec();
}
