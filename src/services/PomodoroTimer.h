#pragma once

#include <QObject>
#include <QTimer>

class AppSettings;

// Режим Pomodoro-таймера: рабочий отрезок, короткий или длинный перерыв.
enum class PomodoroMode
{
    Work,
    ShortBreak,
    LongBreak
};

// PomodoroTimer - вся логика отсчёта времени, без какого-либо UI.
//
// Работает как простой конечный автомат: есть текущий режим (mode),
// сколько секунд до конца текущей фазы (remainingSeconds) и сколько
// рабочих отрезков уже завершено (completedPomodoros). QTimer внутри
// тикает раз в секунду; когда время фазы заканчивается, класс сам решает,
// в какой режим переключиться дальше, и останавливается - следующую фазу
// нужно запустить вызовом start() (обычно по нажатию кнопки в UI).
//
// Длительности фаз (Этап 8) берутся из AppSettings при каждом переключении
// режима, а не зашиты в код - значит, если пользователь поменяет их
// в настройках, следующая же фаза (или Reset текущей) будет уже новой
// длины, без необходимости перезапускать приложение.
class PomodoroTimer : public QObject
{
    Q_OBJECT

public:
    explicit PomodoroTimer(AppSettings *settings, QObject *parent = nullptr);

    PomodoroMode mode() const { return m_mode; }
    int remainingSeconds() const { return m_remainingSeconds; }
    int completedPomodoros() const { return m_completedPomodoros; }
    bool isRunning() const;

    // Принудительно переключает на свежий рабочий отрезок (Work) и сразу
    // запускает отсчёт - используется при старте Pomodoro для конкретной
    // задачи (Этап 4), независимо от того, что было "до".
    void startFresh();

public slots:
    void start();
    void pause();

    // Сбрасывает отсчёт текущей фазы к её полной длительности.
    // Режим и количество завершённых pomodoro не трогает.
    void reset();

signals:
    void tick(int remainingSeconds);
    void modeChanged(PomodoroMode mode);
    void pomodoroCompleted(int totalCompleted);

private slots:
    void onTimeout();

private:
    int durationForMode(PomodoroMode mode) const;
    void switchToMode(PomodoroMode mode);

    // Сколько pomodoro подряд нужно завершить, чтобы наступил длинный
    // перерыв - это особенность техники Pomodoro, а не то, что имеет
    // смысл делать настраиваемым (в задании такого пункта нет).
    static constexpr int kPomodorosUntilLongBreak = 4;

    AppSettings *m_settings;
    QTimer *m_timer;
    PomodoroMode m_mode = PomodoroMode::Work;
    int m_remainingSeconds;
    int m_completedPomodoros = 0;
};
