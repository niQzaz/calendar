#pragma once

#include <QSettings>
#include <QKeySequence>
#include <QString>

// Тема оформления приложения.
enum class AppTheme
{
    Dark,
    Light
};

// Действия, для которых можно настроить горячую клавишу.
enum class ShortcutAction
{
    NewEvent,
    ImportCsv,
    GoToToday,
    TogglePomodoro
};

// Тонкая обёртка над QSettings - типизированный доступ к настройкам вместо
// разбросанных по коду строковых ключей ("pomodoro/workMinutes" и т.п.
// остаются только внутри этого класса).
//
// Значения по умолчанию совпадают с тем, что было "зашито" в код на
// предыдущих этапах (25/5/15 минут, тёмная тема, неделя с понедельника,
// уведомления включены) - так что до первого похода в настройки поведение
// приложения не меняется.
class AppSettings
{
public:
    AppSettings();

    int workMinutes() const;
    void setWorkMinutes(int minutes);

    int shortBreakMinutes() const;
    void setShortBreakMinutes(int minutes);

    int longBreakMinutes() const;
    void setLongBreakMinutes(int minutes);

    AppTheme theme() const;
    void setTheme(AppTheme theme);

    // false = неделя начинается с понедельника, true - с воскресенья.
    bool sundayFirst() const;
    void setSundayFirst(bool sundayFirst);

    bool notificationsEnabled() const;
    void setNotificationsEnabled(bool enabled);

    QKeySequence shortcut(ShortcutAction action) const;
    void setShortcut(ShortcutAction action, const QKeySequence &sequence);

    // Горячая клавиша "из коробки", пока пользователь её не изменил.
    static QKeySequence defaultShortcut(ShortcutAction action);

    // Человекочитаемое название действия для UI диалога настроек.
    static QString shortcutLabel(ShortcutAction action);

private:
    QSettings m_settings;
};
