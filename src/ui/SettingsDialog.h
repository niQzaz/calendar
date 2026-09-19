#pragma once

#include <QDialog>

class AppSettings;
class ThemeManager;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QKeySequenceEdit;

// Диалог настроек приложения.
//
// Простая форма поверх AppSettings: открывается по File → Settings...,
// текущие значения читаются при открытии диалога, а сохраняются только
// по нажатию OK - Cancel не должен менять ничего, поэтому запись
// в AppSettings происходит целиком в onAccept(), а не по мере ввода.
//
// Тема (Phase C) - исключение из этого правила по своей природе: она
// применяется через ThemeManager::setTheme() тоже только в onAccept(),
// но сразу "вживую" (без перезапуска) - ThemeManager сам красит приложение
// и оповещает custom-painted виджеты через сигнал.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(AppSettings *settings, ThemeManager *themeManager, QWidget *parent = nullptr);

private slots:
    void onAccept();

private:
    AppSettings *m_settings;
    ThemeManager *m_themeManager;

    QSpinBox *m_workMinutesSpin = nullptr;
    QSpinBox *m_shortBreakSpin = nullptr;
    QSpinBox *m_longBreakSpin = nullptr;
    QComboBox *m_themeCombo = nullptr;
    QComboBox *m_firstDayCombo = nullptr;
    QCheckBox *m_notificationsCheck = nullptr;

    QKeySequenceEdit *m_newEventShortcutEdit = nullptr;
    QKeySequenceEdit *m_importCsvShortcutEdit = nullptr;
    QKeySequenceEdit *m_goToTodayShortcutEdit = nullptr;
    QKeySequenceEdit *m_togglePomodoroShortcutEdit = nullptr;
};
