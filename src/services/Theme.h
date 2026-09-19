#pragma once

#include <QColor>
#include <QString>

// Единый набор именованных цветов одной темы оформления.
//
// Это просто данные (POD-like), без какой-либо логики - как и Event.
// Любой виджет, которому нужны цвета темы (сейчас - MonthDayCell,
// в будущем - WeekView/DayView), хранит копию Theme и перерисовывается
// при её смене, вместо того чтобы лезть в QSS/палитру приложения напрямую.
struct Theme
{
    QString name; // отображаемое имя для UI диалога настроек ("Dark", "Purple", ...)

    QColor background;       // фон окна целиком
    QColor surface;          // фон панелей/карточек/кнопок
    QColor surfaceElevated;  // hover/приподнятые поверхности поверх surface
    QColor text;             // основной текст
    QColor textSecondary;    // приглушённый текст (подписи, второстепенная информация)
    QColor border;           // тонкие разделительные линии
    QColor accent;           // главный акцентный цвет темы
    QColor accentHover;      // акцент в состоянии hover
    QColor today;            // подсветка "сегодня" в календаре
    QColor selection;        // подсветка выбранного дня/элемента
    QColor eventBackground;  // фон мини-карточек событий
    QColor eventText;        // текст внутри мини-карточек событий
    QColor gridLine;         // линии временной сетки (пригодится в Week/Day View)
};
