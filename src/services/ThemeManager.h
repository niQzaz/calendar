#pragma once

#include <QObject>
#include <QVector>

#include "Theme.h"
#include "AppSettings.h"

// ThemeManager - единственное место в приложении, которое знает конкретные
// цвета тем и умеет их применять.
//
// Почему отдельный класс, а не просто функции в main.cpp (как было в Этапе 8):
// тогда тема выбиралась один раз при старте и применялась только к QApplication
// (палитра + QSS) - для смены "на лету" этого недостаточно, потому что
// custom-painted виджеты (MonthDayCell, в будущем Week/Day View) не следят
// за QSS и должны узнавать о смене темы напрямую. ThemeManager решает обе
// задачи из одного места: applyToApplication() красит обычные Qt-виджеты,
// а сигнал themeChanged() уведомляет custom-painted виджеты, чтобы они
// перекрасились сами.
class ThemeManager : public QObject
{
    Q_OBJECT

public:
    explicit ThemeManager(AppSettings *settings, QObject *parent = nullptr);

    const Theme &currentTheme() const { return m_currentTheme; }
    AppTheme currentThemeId() const { return m_currentThemeId; }

    // Меняет тему: сохраняет выбор в AppSettings, красит приложение заново
    // и испускает themeChanged() - всё сразу, без перезапуска приложения.
    void setTheme(AppTheme themeId);

    // Палитра цветов для конкретной темы. Статический метод - это просто
    // таблица цветов, ей не нужно состояние объекта.
    static Theme themeFor(AppTheme themeId);

    // Все доступные темы, в порядке отображения в диалоге настроек.
    static QVector<AppTheme> allThemeIds();

signals:
    // Испускается при каждой смене темы. Виджеты с кастомной отрисовкой
    // (MonthDayCell и т.д.) подписываются на этот сигнал вместо того,
    // чтобы читать тему только один раз при создании.
    void themeChanged(const Theme &theme);

private:
    void applyToApplication() const;
    static QString buildStyleSheet(const Theme &theme);

    AppSettings *m_settings;
    AppTheme m_currentThemeId;
    Theme m_currentTheme;
};
