#pragma once

#include <QWidget>

#include "services/PomodoroTimer.h"

class QLabel;
class QPushButton;
class NotificationService;
class AppSettings;

// PomodoroWidget - только отображение и кнопки.
//
// Вся логика отсчёта времени живёт в PomodoroTimer (services/), этот
// класс лишь подписывается на его сигналы и обновляет QLabel'ы, а также
// передаёт нажатия кнопок в start()/pause()/reset(). Такое разделение
// позволит в Этапе 4 привязать таймер к конкретному событию календаря,
// не трогая саму логику отсчёта.
class PomodoroWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PomodoroWidget(AppSettings *settings, NotificationService *notificationService = nullptr, QWidget *parent = nullptr);

    // Привязывает таймер к конкретному событию: показывает taskLabel
    // как текущую задачу, принудительно начинает свежий рабочий отрезок
    // и сразу запускает отсчёт. Если до этого был активен другой
    // отрезок (для той же или другой задачи) - он прерывается.
    void startForTask(int eventId, const QString &taskLabel);

public slots:
    // Start, если таймер сейчас на паузе/не запущен; Pause, если запущен.
    // Используется и кнопками Start/Pause, и горячей клавишей (Этап 8).
    void toggleStartPause();

signals:
    // Испускается, когда завершается рабочий отрезок, привязанный
    // к конкретному событию (eventId). MainWindow подписывается на этот
    // сигнал, чтобы сохранить +1 pomodoro для события в БД.
    void pomodoroCompletedForEvent(int eventId);

private slots:
    void onTick(int remainingSeconds);
    void onModeChanged(PomodoroMode mode);
    void onPomodoroCompleted(int totalCompleted);

private:
    static QString formatTime(int totalSeconds);
    static QString modeDisplayName(PomodoroMode mode);
    void setRunningButtonsState(bool running);
    void updateModeDisplay(PomodoroMode mode); // только текст/кнопки, без уведомления

    PomodoroTimer *m_timer;
    NotificationService *m_notificationService = nullptr;
    int m_linkedEventId = -1; // -1 = таймер не привязан к конкретному событию

    QLabel *m_currentTaskLabel = nullptr;
    QLabel *m_modeLabel = nullptr;
    QLabel *m_timeLabel = nullptr;
    QLabel *m_completedLabel = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_pauseButton = nullptr;
    QPushButton *m_resetButton = nullptr;
};
