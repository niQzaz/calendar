#include "PomodoroTimer.h"
#include "AppSettings.h"

PomodoroTimer::PomodoroTimer(AppSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_timer(new QTimer(this))
{
    m_remainingSeconds = durationForMode(m_mode);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &PomodoroTimer::onTimeout);
}

int PomodoroTimer::durationForMode(PomodoroMode mode) const
{
    switch (mode) {
    case PomodoroMode::Work:
        return m_settings->workMinutes() * 60;
    case PomodoroMode::ShortBreak:
        return m_settings->shortBreakMinutes() * 60;
    case PomodoroMode::LongBreak:
        return m_settings->longBreakMinutes() * 60;
    }
    return m_settings->workMinutes() * 60;
}

bool PomodoroTimer::isRunning() const
{
    return m_timer->isActive();
}

void PomodoroTimer::start()
{
    m_timer->start();
}

void PomodoroTimer::startFresh()
{
    m_timer->stop();
    switchToMode(PomodoroMode::Work);
    m_timer->start();
}

void PomodoroTimer::pause()
{
    m_timer->stop();
}

void PomodoroTimer::reset()
{
    m_timer->stop();
    m_remainingSeconds = durationForMode(m_mode);
    emit tick(m_remainingSeconds);
}

void PomodoroTimer::onTimeout()
{
    m_remainingSeconds--;

    if (m_remainingSeconds > 0) {
        emit tick(m_remainingSeconds);
        return;
    }

    // Фаза закончилась - останавливаемся и решаем, что дальше.
    m_timer->stop();

    if (m_mode == PomodoroMode::Work) {
        m_completedPomodoros++;
        emit pomodoroCompleted(m_completedPomodoros);

        const bool timeForLongBreak = (m_completedPomodoros % kPomodorosUntilLongBreak == 0);
        switchToMode(timeForLongBreak ? PomodoroMode::LongBreak : PomodoroMode::ShortBreak);
    } else {
        switchToMode(PomodoroMode::Work);
    }
}

void PomodoroTimer::switchToMode(PomodoroMode mode)
{
    m_mode = mode;
    m_remainingSeconds = durationForMode(mode);
    emit modeChanged(m_mode);
    emit tick(m_remainingSeconds);
}
