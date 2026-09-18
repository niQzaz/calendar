#include "SettingsDialog.h"
#include "services/AppSettings.h"

#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QKeySequenceEdit>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QDialogButtonBox>

SettingsDialog::SettingsDialog(AppSettings *settings, QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle("Settings");

    // --- Pomodoro ---
    m_workMinutesSpin = new QSpinBox(this);
    m_workMinutesSpin->setRange(1, 180);
    m_workMinutesSpin->setSuffix(" min");
    m_workMinutesSpin->setValue(settings->workMinutes());

    m_shortBreakSpin = new QSpinBox(this);
    m_shortBreakSpin->setRange(1, 60);
    m_shortBreakSpin->setSuffix(" min");
    m_shortBreakSpin->setValue(settings->shortBreakMinutes());

    m_longBreakSpin = new QSpinBox(this);
    m_longBreakSpin->setRange(1, 120);
    m_longBreakSpin->setSuffix(" min");
    m_longBreakSpin->setValue(settings->longBreakMinutes());

    auto *pomodoroGroup = new QGroupBox("Pomodoro", this);
    auto *pomodoroLayout = new QFormLayout(pomodoroGroup);
    pomodoroLayout->addRow("Work", m_workMinutesSpin);
    pomodoroLayout->addRow("Short break", m_shortBreakSpin);
    pomodoroLayout->addRow("Long break", m_longBreakSpin);

    // --- Appearance & calendar ---
    m_themeCombo = new QComboBox(this);
    m_themeCombo->addItem("Dark", static_cast<int>(AppTheme::Dark));
    m_themeCombo->addItem("Light", static_cast<int>(AppTheme::Light));
    m_themeCombo->setCurrentIndex(m_themeCombo->findData(static_cast<int>(settings->theme())));

    m_firstDayCombo = new QComboBox(this);
    m_firstDayCombo->addItem("Monday", false);
    m_firstDayCombo->addItem("Sunday", true);
    m_firstDayCombo->setCurrentIndex(m_firstDayCombo->findData(settings->sundayFirst()));

    auto *appearanceGroup = new QGroupBox("Appearance && calendar", this);
    auto *appearanceLayout = new QFormLayout(appearanceGroup);
    appearanceLayout->addRow("Theme", m_themeCombo);
    appearanceLayout->addRow("First day of week", m_firstDayCombo);

    // --- Notifications ---
    m_notificationsCheck = new QCheckBox("Enable desktop notifications", this);
    m_notificationsCheck->setChecked(settings->notificationsEnabled());

    // --- Keyboard shortcuts ---
    m_newEventShortcutEdit = new QKeySequenceEdit(settings->shortcut(ShortcutAction::NewEvent), this);
    m_importCsvShortcutEdit = new QKeySequenceEdit(settings->shortcut(ShortcutAction::ImportCsv), this);
    m_goToTodayShortcutEdit = new QKeySequenceEdit(settings->shortcut(ShortcutAction::GoToToday), this);
    m_togglePomodoroShortcutEdit = new QKeySequenceEdit(settings->shortcut(ShortcutAction::TogglePomodoro), this);

    auto *shortcutsGroup = new QGroupBox("Keyboard shortcuts", this);
    auto *shortcutsLayout = new QFormLayout(shortcutsGroup);
    shortcutsLayout->addRow(AppSettings::shortcutLabel(ShortcutAction::NewEvent), m_newEventShortcutEdit);
    shortcutsLayout->addRow(AppSettings::shortcutLabel(ShortcutAction::ImportCsv), m_importCsvShortcutEdit);
    shortcutsLayout->addRow(AppSettings::shortcutLabel(ShortcutAction::GoToToday), m_goToTodayShortcutEdit);
    shortcutsLayout->addRow(AppSettings::shortcutLabel(ShortcutAction::TogglePomodoro), m_togglePomodoroShortcutEdit);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->addWidget(pomodoroGroup);
    rootLayout->addWidget(appearanceGroup);
    rootLayout->addWidget(m_notificationsCheck);
    rootLayout->addWidget(shortcutsGroup);
    rootLayout->addWidget(buttonBox);

    setMinimumWidth(360);
}

void SettingsDialog::onAccept()
{
    const AppTheme newTheme = static_cast<AppTheme>(m_themeCombo->currentData().toInt());
    m_themeChanged = (newTheme != m_settings->theme());

    m_settings->setWorkMinutes(m_workMinutesSpin->value());
    m_settings->setShortBreakMinutes(m_shortBreakSpin->value());
    m_settings->setLongBreakMinutes(m_longBreakSpin->value());
    m_settings->setTheme(newTheme);
    m_settings->setSundayFirst(m_firstDayCombo->currentData().toBool());
    m_settings->setNotificationsEnabled(m_notificationsCheck->isChecked());

    m_settings->setShortcut(ShortcutAction::NewEvent, m_newEventShortcutEdit->keySequence());
    m_settings->setShortcut(ShortcutAction::ImportCsv, m_importCsvShortcutEdit->keySequence());
    m_settings->setShortcut(ShortcutAction::GoToToday, m_goToTodayShortcutEdit->keySequence());
    m_settings->setShortcut(ShortcutAction::TogglePomodoro, m_togglePomodoroShortcutEdit->keySequence());

    accept();
}
