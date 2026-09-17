#pragma once

#include <QWidget>

#include "services/PomodoroTimer.h"

class QLabel;
class QPushButton;

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
    explicit PomodoroWidget(QWidget *parent = nullptr);

private slots:
    void onTick(int remainingSeconds);
    void onModeChanged(PomodoroMode mode);
    void onPomodoroCompleted(int totalCompleted);

private:
    static QString formatTime(int totalSeconds);
    static QString modeDisplayName(PomodoroMode mode);

    PomodoroTimer *m_timer;

    QLabel *m_modeLabel = nullptr;
    QLabel *m_timeLabel = nullptr;
    QLabel *m_completedLabel = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_pauseButton = nullptr;
    QPushButton *m_resetButton = nullptr;
};
