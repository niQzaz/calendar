#include "PomodoroWidget.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

PomodoroWidget::PomodoroWidget(QWidget *parent)
    : QWidget(parent)
    , m_timer(new PomodoroTimer(this))
{
    m_currentTaskLabel = new QLabel("No task selected", this);
    m_currentTaskLabel->setAlignment(Qt::AlignCenter);
    m_currentTaskLabel->setWordWrap(true);
    m_currentTaskLabel->setObjectName("currentTaskLabel");

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
    rootLayout->addWidget(m_currentTaskLabel);
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
        setRunningButtonsState(true);
    });
    connect(m_pauseButton, &QPushButton::clicked, this, [this]() {
        m_timer->pause();
        setRunningButtonsState(false);
    });
    connect(m_resetButton, &QPushButton::clicked, this, [this]() {
        m_timer->reset();
        setRunningButtonsState(false);
    });

    // Начальное отображение: режим Work, полное время, 0 завершённых.
    onModeChanged(m_timer->mode());
    onTick(m_timer->remainingSeconds());
    onPomodoroCompleted(m_timer->completedPomodoros());
}

void PomodoroWidget::startForTask(int eventId, const QString &taskLabel)
{
    m_linkedEventId = eventId;
    m_currentTaskLabel->setText(QString("Task: %1").arg(taskLabel));
    m_timer->startFresh();
    setRunningButtonsState(true);
}

void PomodoroWidget::setRunningButtonsState(bool running)
{
    m_startButton->setEnabled(!running);
    m_pauseButton->setEnabled(running);
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
    setRunningButtonsState(false);
}

void PomodoroWidget::onPomodoroCompleted(int totalCompleted)
{
    m_completedLabel->setText(QString("Completed pomodoros: %1").arg(totalCompleted));

    // Если таймер сейчас привязан к конкретной задаче - сообщаем об этом
    // наружу, чтобы MainWindow сохранил +1 pomodoro для этого события в БД.
    // PomodoroTimer сам не знает про события - эту связь держит только
    // PomodoroWidget.
    if (m_linkedEventId != -1)
        emit pomodoroCompletedForEvent(m_linkedEventId);
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
