#pragma once

#include <QWidget>
#include <QDate>

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
// позволило в Этапе 4 привязать таймер к конкретному событию календаря,
// не трогая саму логику отсчёта.
//
// Начиная с интеграции NOW/Execution/Pomodoro: привязка теперь
// occurrence-aware (eventId + occurrenceDate, не только eventId - см.
// startForTask()), но сам виджет по-прежнему НЕ знает ничего про
// EventManager/Execution - он только просит "хочу начать текущую задачу"
// через startCurrentTaskRequested() и ждёt, что вызывающий код (MainWindow)
// определит Current Task и стартует нужный Execution сам, а потом позовёт
// startForTask() с уже готовыми eventId/occurrenceDate - ровно так же,
// как и раньше для выбора из списка.
class PomodoroWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PomodoroWidget(AppSettings *settings, NotificationService *notificationService = nullptr, QWidget *parent = nullptr);

    // Привязывает таймер к конкретному событию/вхождению: показывает
    // taskLabel как текущую задачу, принудительно начинает свежий рабочий
    // отрезок и сразу запускает отсчёт. Если до этого был активен другой
    // отрезок (для той же или другой задачи) - он прерывается.
    //
    // occurrenceDate обязателен (не только eventId) - для повторяющегося
    // события "Математика" сегодня и "Математика" завтра должны остаться
    // разными привязками, иначе pomodoroCompletedForEvent() ниже не сможет
    // корректно сообщить, к какому именно вхождению относится завершённый
    // отрезок.
    void startForTask(int eventId, const QDate &occurrenceDate, const QString &taskLabel);

    // Ничего не меняет в таймере/привязке - просто показывает, что Current
    // Task не найден (MainWindow вызывает это вместо startForTask(), когда
    // nowSnapshot() ничего не вернул для "Start Current Task").
    void showNoCurrentTask();

public slots:
    // Start, если таймер сейчас на паузе/не запущен; Pause, если запущен.
    // Используется и кнопками Start/Pause, и горячей клавишей (Этап 8).
    void toggleStartPause();

signals:
    // Испускается, когда завершается рабочий отрезок, привязанный
    // к конкретному вхождению (eventId + occurrenceDate). MainWindow
    // подписывается на этот сигнал, чтобы сохранить +1 pomodoro в БД.
    void pomodoroCompletedForEvent(int eventId, const QDate &occurrenceDate);

    // Пользователь нажал "Start Current Task" - сам PomodoroWidget не
    // знает, что такое Current Task (это Execution/NOW-логика, ей тут не
    // место) - просто просит MainWindow разобраться и, если получится,
    // вызвать startForTask()/showNoCurrentTask() в ответ.
    void startCurrentTaskRequested();

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
    int m_linkedEventId = -1;      // -1 = таймер не привязан к конкретному событию
    QDate m_linkedOccurrenceDate;  // валидна, только если m_linkedEventId != -1

    QLabel *m_currentTaskLabel = nullptr;
    QLabel *m_modeLabel = nullptr;
    QLabel *m_timeLabel = nullptr;
    QLabel *m_completedLabel = nullptr;
    QPushButton *m_startCurrentTaskButton = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_pauseButton = nullptr;
    QPushButton *m_resetButton = nullptr;
};
