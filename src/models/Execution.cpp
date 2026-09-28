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
