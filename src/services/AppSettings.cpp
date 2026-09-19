#include "AppSettings.h"

namespace {

constexpr int kDefaultWorkMinutes = 25;
constexpr int kDefaultShortBreakMinutes = 5;
constexpr int kDefaultLongBreakMinutes = 15;

QString shortcutKey(ShortcutAction action)
{
    switch (action) {
    case ShortcutAction::NewEvent: return QStringLiteral("shortcuts/newEvent");
    case ShortcutAction::ImportCsv: return QStringLiteral("shortcuts/importCsv");
    case ShortcutAction::GoToToday: return QStringLiteral("shortcuts/goToToday");
    case ShortcutAction::TogglePomodoro: return QStringLiteral("shortcuts/togglePomodoro");
    }
    return QString();
}

} // namespace

AppSettings::AppSettings()
    // QSettings(organization, application) - на Linux это файл
    // ~/.config/QtCalendarApp/QtCalendarApp.conf, создаётся автоматически.
    : m_settings(QStringLiteral("QtCalendarApp"), QStringLiteral("QtCalendarApp"))
{
}

int AppSettings::workMinutes() const
{
    return m_settings.value("pomodoro/workMinutes", kDefaultWorkMinutes).toInt();
}

void AppSettings::setWorkMinutes(int minutes)
{
    m_settings.setValue("pomodoro/workMinutes", minutes);
}

int AppSettings::shortBreakMinutes() const
{
    return m_settings.value("pomodoro/shortBreakMinutes", kDefaultShortBreakMinutes).toInt();
}

void AppSettings::setShortBreakMinutes(int minutes)
{
    m_settings.setValue("pomodoro/shortBreakMinutes", minutes);
}

int AppSettings::longBreakMinutes() const
{
    return m_settings.value("pomodoro/longBreakMinutes", kDefaultLongBreakMinutes).toInt();
}

void AppSettings::setLongBreakMinutes(int minutes)
{
    m_settings.setValue("pomodoro/longBreakMinutes", minutes);
}

AppTheme AppSettings::theme() const
{
    const QString value = m_settings.value("appearance/theme", "dark").toString();
    if (value == QLatin1String("light")) return AppTheme::Light;
    if (value == QLatin1String("purple")) return AppTheme::Purple;
    if (value == QLatin1String("ocean")) return AppTheme::Ocean;
    if (value == QLatin1String("forest")) return AppTheme::Forest;
    if (value == QLatin1String("rose")) return AppTheme::Rose;
    return AppTheme::Dark;
}

void AppSettings::setTheme(AppTheme theme)
{
    QString value;
    switch (theme) {
    case AppTheme::Light: value = "light"; break;
    case AppTheme::Purple: value = "purple"; break;
    case AppTheme::Ocean: value = "ocean"; break;
    case AppTheme::Forest: value = "forest"; break;
    case AppTheme::Rose: value = "rose"; break;
    case AppTheme::Dark: value = "dark"; break;
    }
    m_settings.setValue("appearance/theme", value);
}

bool AppSettings::sundayFirst() const
{
    return m_settings.value("calendar/sundayFirst", false).toBool();
}

void AppSettings::setSundayFirst(bool sundayFirst)
{
    m_settings.setValue("calendar/sundayFirst", sundayFirst);
}

bool AppSettings::notificationsEnabled() const
{
    return m_settings.value("notifications/enabled", true).toBool();
}

void AppSettings::setNotificationsEnabled(bool enabled)
{
    m_settings.setValue("notifications/enabled", enabled);
}

QKeySequence AppSettings::defaultShortcut(ShortcutAction action)
{
    switch (action) {
    case ShortcutAction::NewEvent: return QKeySequence(QStringLiteral("Ctrl+N"));
    case ShortcutAction::ImportCsv: return QKeySequence(QStringLiteral("Ctrl+I"));
    case ShortcutAction::GoToToday: return QKeySequence(QStringLiteral("Ctrl+T"));
    case ShortcutAction::TogglePomodoro: return QKeySequence(QStringLiteral("Ctrl+P"));
    }
    return QKeySequence();
}

QString AppSettings::shortcutLabel(ShortcutAction action)
{
    switch (action) {
    case ShortcutAction::NewEvent: return QStringLiteral("New event");
    case ShortcutAction::ImportCsv: return QStringLiteral("Import CSV");
    case ShortcutAction::GoToToday: return QStringLiteral("Go to today");
    case ShortcutAction::TogglePomodoro: return QStringLiteral("Start/Pause Pomodoro");
    }
    return QString();
}

QKeySequence AppSettings::shortcut(ShortcutAction action) const
{
    const QString value = m_settings.value(shortcutKey(action)).toString();
    return value.isEmpty() ? defaultShortcut(action) : QKeySequence(value);
}

void AppSettings::setShortcut(ShortcutAction action, const QKeySequence &sequence)
{
    m_settings.setValue(shortcutKey(action), sequence.toString());
}
