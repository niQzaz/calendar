#include "Execution.h"

QString executionStatusToString(ExecutionStatus status)
{
    switch (status) {
    case ExecutionStatus::Planned:   return QStringLiteral("planned");
    case ExecutionStatus::Running:   return QStringLiteral("running");
    case ExecutionStatus::Completed: return QStringLiteral("completed");
    case ExecutionStatus::Missed:    return QStringLiteral("missed");
    case ExecutionStatus::Postponed: return QStringLiteral("postponed");
    case ExecutionStatus::Cancelled: return QStringLiteral("cancelled");
    }
    return QStringLiteral("planned");
}

ExecutionStatus executionStatusFromString(const QString &value)
{
    if (value == QStringLiteral("running"))   return ExecutionStatus::Running;
    if (value == QStringLiteral("completed")) return ExecutionStatus::Completed;
    if (value == QStringLiteral("missed"))    return ExecutionStatus::Missed;
    if (value == QStringLiteral("postponed")) return ExecutionStatus::Postponed;
    if (value == QStringLiteral("cancelled")) return ExecutionStatus::Cancelled;
    return ExecutionStatus::Planned; // неизвестное/пустое значение -> безопасный дефолт
}

ExecutionStatus resolveExecutionStatus(
    const QDate &occurrenceDate,
    const QTime &plannedStart,
    const QTime &plannedEnd,
    const EventExecution *savedExecution,
    const QDateTime &now,
    int gracePeriodMinutes)
{
    // plannedStart в самом решении не участвует (Missed зависит только от
    // plannedEnd + grace), но оставлен в сигнатуре ради единообразия
    // с startDeviationSeconds() ниже - обе функции получают контекст
    // одного и того же вхождения одинаковым набором параметров.
    (void)plannedStart;

    if (savedExecution && savedExecution->status != ExecutionStatus::Planned)
        return savedExecution->status;

    const QDateTime plannedEndDateTime(occurrenceDate, plannedEnd);
    if (plannedEndDateTime.addSecs(gracePeriodMinutes * 60) < now)
        return ExecutionStatus::Missed;

    return ExecutionStatus::Planned;
}

bool actualDurationSeconds(const EventExecution &execution, qint64 &outSeconds)
{
    if (!execution.actualStart.isValid() || !execution.actualEnd.isValid())
        return false;

    outSeconds = execution.actualStart.secsTo(execution.actualEnd);
    return true;
}

bool startDeviationSeconds(const QDate &occurrenceDate, const QTime &plannedStart,
                            const EventExecution &execution, qint64 &outSeconds)
{
    if (!execution.actualStart.isValid())
        return false;

    const QDateTime plannedStartDateTime(occurrenceDate, plannedStart);
    outSeconds = plannedStartDateTime.secsTo(execution.actualStart);
    return true;
}

bool durationDeviationSeconds(const QTime &plannedStart, const QTime &plannedEnd,
                               const EventExecution &execution, qint64 &outSeconds)
{
    qint64 actualSecs = 0;
    if (!actualDurationSeconds(execution, actualSecs))
        return false;

    const qint64 plannedSecs = plannedStart.secsTo(plannedEnd);
    outSeconds = actualSecs - plannedSecs;
    return true;
}

bool findCurrentTask(const QVector<EventWithStatus> &todaysEvents, const QTime &now, EventWithStatus &outCurrent)
{
    // Приоритет 1: явно Running - пользователь сам нажал Start, это
    // всегда "текущая задача", вне зависимости от планового окна.
    for (const EventWithStatus &item : todaysEvents) {
        if (item.status == ExecutionStatus::Running) {
            outCurrent = item;
            return true;
        }
    }

    // Приоритет 2: плановое окно содержит now, и статус ещё "живой".
    // Completed/Cancelled/Postponed не считаются текущими, даже если
    // их окно формально накрывает now - они уже "решены".
    for (const EventWithStatus &item : todaysEvents) {
        const bool isLive = item.status == ExecutionStatus::Planned || item.status == ExecutionStatus::Missed;
        if (isLive && item.event.startTime <= now && now <= item.event.endTime) {
            outCurrent = item;
            return true;
        }
    }

    return false;
}

bool findNextTask(const QVector<EventWithStatus> &todaysEvents, const QTime &now,
                   const EventWithStatus *current, EventWithStatus &outNext)
{
    bool found = false;

    for (const EventWithStatus &item : todaysEvents) {
        if (item.status == ExecutionStatus::Completed
            || item.status == ExecutionStatus::Cancelled
            || item.status == ExecutionStatus::Postponed) {
            continue; // уже решённые - не кандидаты на "следующую"
        }

        if (current && item.event.id == current->event.id && item.event.date == current->event.date)
            continue; // текущая задача не может одновременно быть следующей

        if (item.event.startTime <= now)
            continue; // уже должна была начаться (или идёт) - не "следующая"

        if (!found || item.event.startTime < outNext.event.startTime) {
            outNext = item;
            found = true;
        }
    }

    return found;
}
