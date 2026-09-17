#pragma once

#include <QObject>
#include <QTimer>

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
// Каждый переход/тик оборачивается в сигнал (tick/modeChanged/
// pomodoroCompleted), поэтому PomodoroWidget (или в будущем - виджет
// из Этапа 4/11) может просто подписаться и не знать, как считается время.
class PomodoroTimer : public QObject
{
    Q_OBJECT

public:
    explicit PomodoroTimer(QObject *parent = nullptr);

    PomodoroMode mode() const { return m_mode; }
    int remainingSeconds() const { return m_remainingSeconds; }
    int completedPomodoros() const { return m_completedPomodoros; }
    bool isRunning() const;

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

    static constexpr int kWorkMinutes = 25;
    static constexpr int kShortBreakMinutes = 5;
    static constexpr int kLongBreakMinutes = 15;
    static constexpr int kPomodorosUntilLongBreak = 4;

    QTimer *m_timer;
    PomodoroMode m_mode = PomodoroMode::Work;
    int m_remainingSeconds;
    int m_completedPomodoros = 0;
};
