#pragma once

#include <QDialog>

class AppSettings;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QKeySequenceEdit;

// Диалог настроек приложения (Этап 8).
//
// Простая форма поверх AppSettings: открывается по File → Settings...,
// текущие значения читаются при открытии диалога, а сохраняются только
// по нажатию OK - Cancel не должен менять ничего, поэтому запись
// в AppSettings происходит целиком в onAccept(), а не по мере ввода.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(AppSettings *settings, QWidget *parent = nullptr);

    // true, если после закрытия с OK тема была изменена - MainWindow
    // использует это, чтобы предложить перезапуск приложения (полноценную
    // living-перекраску всех виджетов на лету делать не стали ради простоты).
    bool themeChanged() const { return m_themeChanged; }

private slots:
    void onAccept();

private:
    AppSettings *m_settings;
    bool m_themeChanged = false;

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
