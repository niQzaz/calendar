#include "NowView.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace {

// Секунды -> "5m late"/"5m early"/"on time" - используется и для start
// deviation, и для duration deviation, только со своими словами для
// "плюс"/"минус" направления (late/longer, early/shorter).
QString describeDeviation(qint64 seconds, const QString &positiveWord, const QString &negativeWord)
{
    const qint64 minutes = seconds / 60;
    if (minutes == 0)
        return QStringLiteral("on time");
    if (minutes > 0)
        return QString("%1m %2").arg(minutes).arg(positiveWord);
    return QString("%1m %2").arg(-minutes).arg(negativeWord);
}

QString formatTimeRange(const QTime &start, const QTime &end)
{
    return QString("Planned %1\xE2\x80\x93%2").arg(start.toString("HH:mm"), end.toString("HH:mm"));
}

} // namespace

NowView::NowView(QWidget *parent)
    : QWidget(parent)
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(24, 24, 24, 24);
    rootLayout->setSpacing(16);

    m_currentCard = buildCard("CURRENT TASK", m_currentTitleLabel, m_currentTimeLabel, m_currentStatusLabel);
    m_currentKickerLabel = m_currentCard->findChild<QLabel *>("kicker");

    m_nextCard = buildCard("NEXT TASK", m_nextTitleLabel, m_nextTimeLabel, m_nextStatusLabel);
    m_nextKickerLabel = m_nextCard->findChild<QLabel *>("kicker");

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setObjectName("nowSummaryLabel");

    rootLayout->addWidget(m_currentCard);
    rootLayout->addWidget(m_nextCard);
    rootLayout->addWidget(m_summaryLabel);
    rootLayout->addStretch(1);
}

QWidget *NowView::buildCard(const QString &kicker, QLabel *&titleLabel, QLabel *&timeLabel, QLabel *&statusLabel)
{
    auto *card = new QWidget(this);
    card->setObjectName("nowCard");

    auto *kickerLabel = new QLabel(kicker, card);
    kickerLabel->setObjectName("kicker");

    titleLabel = new QLabel(card);
    titleLabel->setObjectName("nowCardTitle");
    titleLabel->setWordWrap(true);

    timeLabel = new QLabel(card);
    timeLabel->setObjectName("nowCardTime");

    statusLabel = new QLabel(card);
    statusLabel->setObjectName("nowCardStatus");
    statusLabel->setWordWrap(true);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(4);
    layout->addWidget(kickerLabel);
    layout->addWidget(titleLabel);
    layout->addWidget(timeLabel);
    layout->addWidget(statusLabel);

    return card;
}

void NowView::goToPrevious()
{
    // No-op - см. комментарий в NowView.h: NOW всегда "сегодня", листать некуда.
}

void NowView::goToNext()
{
    // No-op.
}

void NowView::goToToday()
{
    // No-op - NOW и так всегда показывает сегодня; MainWindow сам решает,
    // когда пересчитать snapshot (по таймеру и по факту изменений).
}

QString NowView::headerTitle() const
{
    const QDateTime asOf = m_asOf.isValid() ? m_asOf : QDateTime::currentDateTime();
    return asOf.date().toString("dddd, d MMMM yyyy");
}

void NowView::setSnapshot(const NowSnapshot &snapshot, const QDateTime &asOf)
{
    m_asOf = asOf;
    const QDate today = asOf.date();

    // --- Текущая задача ---
    if (snapshot.hasCurrent) {
        const Event &event = snapshot.current.event;
        m_currentTitleLabel->setText(event.title);
        m_currentTimeLabel->setText(formatTimeRange(event.startTime, event.endTime));

        QString status;
        switch (snapshot.current.status) {
        case ExecutionStatus::Running: {
            status = QStringLiteral("Running");
            qint64 deviationSecs = 0;
            if (startDeviationSeconds(today, event.startTime, snapshot.current.execution, deviationSecs)) {
                status += QString(" \xE2\x80\x94 started %1")
                    .arg(describeDeviation(deviationSecs, "late", "early"));
            }
            break;
        }
        case ExecutionStatus::Missed:
            status = QStringLiteral("Missed \xE2\x80\x94 was planned until ") + event.endTime.toString("HH:mm");
            break;
        case ExecutionStatus::Completed: {
            status = QStringLiteral("Completed");
            qint64 deviationSecs = 0;
            if (durationDeviationSeconds(event.startTime, event.endTime, snapshot.current.execution, deviationSecs)) {
                status += QString(" \xE2\x80\x94 took %1")
                    .arg(describeDeviation(deviationSecs, "longer than planned", "less than planned"));
            }
            break;
        }
        case ExecutionStatus::Planned: {
            // findCurrentTask() кладёт сюда только события, чьё окно
            // [start, end] содержит "сейчас" - но само "сейчас" могло
            // наступить уже после startTime (задача должна была начаться,
            // но Start ещё не нажали) - полезно показать это отдельно
            // от простого "Planned".
            const QTime nowTime = asOf.time();
            if (nowTime > event.startTime) {
                const qint64 lateSecs = event.startTime.secsTo(nowTime);
                status = QString("%1m behind schedule \xE2\x80\x94 not started yet").arg(lateSecs / 60);
            } else {
                status = QStringLiteral("Not started yet");
            }
            break;
        }
        case ExecutionStatus::Postponed:
            status = QStringLiteral("Postponed");
            break;
        case ExecutionStatus::Cancelled:
            status = QStringLiteral("Cancelled");
            break;
        }
        m_currentStatusLabel->setText(status);
    } else {
        m_currentTitleLabel->setText(QStringLiteral("Nothing scheduled right now"));
        m_currentTimeLabel->clear();
        m_currentStatusLabel->clear();
    }

    // --- Следующая задача ---
    if (snapshot.hasNext) {
        const Event &event = snapshot.next.event;
        m_nextTitleLabel->setText(event.title);
        m_nextTimeLabel->setText(formatTimeRange(event.startTime, event.endTime));

        const QTime nowTime = asOf.time();
        const qint64 untilStartSecs = nowTime.secsTo(event.startTime);
        m_nextStatusLabel->setText(untilStartSecs > 0
            ? QString("Starts in %1m").arg(untilStartSecs / 60)
            : QStringLiteral("Starting now"));
    } else {
        m_nextTitleLabel->setText(QStringLiteral("Nothing else planned today"));
        m_nextTimeLabel->clear();
        m_nextStatusLabel->clear();
    }

    // --- Краткая сводка дня (раздел 2 Product Context: "краткое состояние дня") ---
    int completed = 0;
    int missed = 0;
    int running = 0;
    for (const EventWithStatus &item : snapshot.allForDate) {
        switch (item.status) {
        case ExecutionStatus::Completed: ++completed; break;
        case ExecutionStatus::Missed:    ++missed; break;
        case ExecutionStatus::Running:   ++running; break;
        default: break;
        }
    }
    QString summary = QString("Today: %1 planned \xC2\xB7 %2 done").arg(snapshot.allForDate.size()).arg(completed);
    if (running > 0)
        summary += QString(" \xC2\xB7 %1 running").arg(running);
    if (missed > 0)
        summary += QString(" \xC2\xB7 %1 missed").arg(missed);
    m_summaryLabel->setText(summary);
}

void NowView::setTheme(const Theme &theme)
{
    m_theme = theme;
    applyPalette();
}

void NowView::applyPalette()
{
    const QString cardStyle = QString(
        "QWidget#nowCard { background-color: %1; border: 1px solid %2; border-radius: 8px; }"
        "QLabel#kicker { color: %3; font-size: 11px; font-weight: 600; letter-spacing: 1px; }"
        "QLabel#nowCardTitle { color: %4; font-size: 18px; font-weight: 600; }"
        "QLabel#nowCardTime { color: %3; font-size: 13px; }"
        "QLabel#nowCardStatus { color: %5; font-size: 13px; }"
    ).arg(m_theme.surface.name(), m_theme.border.name(), m_theme.textSecondary.name(),
          m_theme.text.name(), m_theme.accent.name());

    m_currentCard->setStyleSheet(cardStyle);
    m_nextCard->setStyleSheet(cardStyle);

    m_summaryLabel->setStyleSheet(QString("color: %1; font-size: 13px;").arg(m_theme.textSecondary.name()));
}
