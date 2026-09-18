#include <QApplication>
#include <QPalette>

#include "ui/MainWindow.h"
#include "services/AppSettings.h"

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
        QLabel#currentTaskLabel {
            font-size: 13px;
            font-weight: bold;
            color: #dddddd;
        }
        QLabel#pomodoroModeLabel {
            font-size: 14px;
            color: #999999;
        }
        QLabel#pomodoroTimeLabel {
            font-size: 48px;
            font-weight: bold;
        }
        QLabel#pomodoroCompletedLabel {
            color: #999999;
        }
    )";
    app.setStyleSheet(qss);
}

// Светлая тема (Этап 8) - та же структура QSS, что и у тёмной,
// просто с другими цветами. Отдельная функция вместо параметризации
// одной общей ради ясности: тут всего два варианта темы, и держать
// в голове "какой цвет от какого параметра" было бы сложнее, чем просто
// читать два похожих, но самостоятельных списка цветов.
void applyLightTheme(QApplication &app)
{
    app.setStyle("Fusion");

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(245, 245, 247));
    palette.setColor(QPalette::WindowText, Qt::black);
    palette.setColor(QPalette::Base, QColor(255, 255, 255));
    palette.setColor(QPalette::AlternateBase, QColor(235, 235, 238));
    palette.setColor(QPalette::Text, Qt::black);
    palette.setColor(QPalette::Button, QColor(230, 230, 233));
    palette.setColor(QPalette::ButtonText, Qt::black);
    palette.setColor(QPalette::Highlight, QColor(74, 108, 247));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(palette);

    const QString qss = R"(
        QPushButton#dayButton {
            border: none;
            border-radius: 6px;
            background-color: #eaeaee;
            color: #1a1a1a;
        }
        QPushButton#dayButton:hover {
            background-color: #dcdce2;
        }
        QPushButton#dayButton:checked {
            background-color: #4a6cf7;
            color: white;
            font-weight: bold;
        }
        QPushButton#dayButton[otherMonth="true"] {
            color: #a0a0a5;
        }
        QPushButton#dayButton[isToday="true"] {
            border: 1px solid #4a6cf7;
        }
        QPushButton#dayButton[hasEvents="true"] {
            border-bottom: 3px solid #d98a1f;
        }
        QLabel#monthLabel {
            font-size: 16px;
            font-weight: bold;
        }
        QLabel#weekDayLabel {
            color: #6b6b70;
        }
        QLabel#selectedDateLabel {
            font-size: 14px;
            font-weight: bold;
            padding-bottom: 4px;
        }
        QLabel#hintLabel {
            color: #6b6b70;
            font-size: 11px;
        }
        QLabel#currentTaskLabel {
            font-size: 13px;
            font-weight: bold;
            color: #2a2a2a;
        }
        QLabel#pomodoroModeLabel {
            font-size: 14px;
            color: #6b6b70;
        }
        QLabel#pomodoroTimeLabel {
            font-size: 48px;
            font-weight: bold;
        }
        QLabel#pomodoroCompletedLabel {
            color: #6b6b70;
        }
    )";
    app.setStyleSheet(qss);
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Тема читается из настроек один раз при старте (Этап 8). Смена темы
    // "на лету" из диалога настроек не делается - слишком много мест
    // пришлось бы перекрашивать вручную ради не такой уж частой операции;
    // вместо этого после смены темы предлагается перезапустить приложение.
    AppSettings settings;
    if (settings.theme() == AppTheme::Light)
        applyLightTheme(app);
    else
        applyDarkTheme(app);

    MainWindow window;
    window.show();

    return app.exec();
}
