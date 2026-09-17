#include "PomodoroWidget.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

PomodoroWidget::PomodoroWidget(QWidget *parent)
    : QWidget(parent)
    , m_timer(new PomodoroTimer(this))
{
    m_modeLabel = new QLabel(this);
    m_modeLabel->setAlignment(Qt::AlignCenter);
    m_modeLabel->setObjectName("pomodoroModeLabel");

    m_timeLabel = new QLabel(this);
    m_timeLabel->setAlignment(Qt::AlignCenter);
    m_timeLabel->setObjectName("pomodoroTimeLabel");

    m_completedLabel = new QLabel(this);
    m_completedLabel->setAlignment(Qt::AlignCenter);
    m_completedLabel->setObjectName("pomodoroCompletedLabel");

    m_startButton = new QPushButton("Start", this);
    m_pauseButton = new QPushButton("Pause", this);
    m_resetButton = new QPushButton("Reset", this);
    m_pauseButton->setEnabled(false);

    auto *buttonsLayout = new QHBoxLayout();
    buttonsLayout->addWidget(m_startButton);
    buttonsLayout->addWidget(m_pauseButton);
    buttonsLayout->addWidget(m_resetButton);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->addStretch();
    rootLayout->addWidget(m_modeLabel);
    rootLayout->addWidget(m_timeLabel);
    rootLayout->addLayout(buttonsLayout);
    rootLayout->addWidget(m_completedLabel);
    rootLayout->addStretch();

    connect(m_timer, &PomodoroTimer::tick, this, &PomodoroWidget::onTick);
    connect(m_timer, &PomodoroTimer::modeChanged, this, &PomodoroWidget::onModeChanged);
    connect(m_timer, &PomodoroTimer::pomodoroCompleted, this, &PomodoroWidget::onPomodoroCompleted);

    connect(m_startButton, &QPushButton::clicked, this, [this]() {
        m_timer->start();
        m_startButton->setEnabled(false);
        m_pauseButton->setEnabled(true);
    });
    connect(m_pauseButton, &QPushButton::clicked, this, [this]() {
        m_timer->pause();
        m_startButton->setEnabled(true);
        m_pauseButton->setEnabled(false);
    });
    connect(m_resetButton, &QPushButton::clicked, this, [this]() {
        m_timer->reset();
        m_startButton->setEnabled(true);
        m_pauseButton->setEnabled(false);
    });

    // Начальное отображение: режим Work, полное время, 0 завершённых.
    onModeChanged(m_timer->mode());
    onTick(m_timer->remainingSeconds());
    onPomodoroCompleted(m_timer->completedPomodoros());
}

void PomodoroWidget::onTick(int remainingSeconds)
{
    m_timeLabel->setText(formatTime(remainingSeconds));
}

void PomodoroWidget::onModeChanged(PomodoroMode mode)
{
    m_modeLabel->setText(modeDisplayName(mode).toUpper());

    // Смена режима означает, что фаза закончилась и таймер сам
    // остановился (см. PomodoroTimer::onTimeout) - возвращаем кнопки
    // в состояние "готов к запуску следующей фазы".
    m_startButton->setEnabled(true);
    m_pauseButton->setEnabled(false);
}

void PomodoroWidget::onPomodoroCompleted(int totalCompleted)
{
    m_completedLabel->setText(QString("Completed pomodoros: %1").arg(totalCompleted));
}

QString PomodoroWidget::formatTime(int totalSeconds)
{
    const int minutes = totalSeconds / 60;
    const int seconds = totalSeconds % 60;
    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

QString PomodoroWidget::modeDisplayName(PomodoroMode mode)
{
    switch (mode) {
    case PomodoroMode::Work:
        return "Work";
    case PomodoroMode::ShortBreak:
        return "Short break";
    case PomodoroMode::LongBreak:
        return "Long break";
    }
    return QString();
}
