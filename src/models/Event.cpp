#include "Event.h"

#include <algorithm>

bool eventOccursOnDate(const Event &event, const QDate &date)
{
    if (!event.date.isValid() || !date.isValid())
        return false;

    if (date < event.date)
        return false; // раньше первого повторения

    if (event.recurrenceEndDate.isValid() && date > event.recurrenceEndDate)
        return false; // позже даты окончания серии

    switch (event.recurrenceType) {
    case RecurrenceType::None:
        return date == event.date;

    case RecurrenceType::Daily:
        return true; // любая дата в диапазоне [date, end] уже проверена выше

    case RecurrenceType::Weekdays:
        return date.dayOfWeek() >= 1 && date.dayOfWeek() <= 5; // 1=Пн..5=Пт

    case RecurrenceType::Weekly:
        // Тот же день недели, что и у "якорной" даты - раз в 7 дней.
        return event.date.daysTo(date) % 7 == 0;

    case RecurrenceType::Custom: {
        const int interval = std::max(1, event.recurrenceInterval);
        return event.date.daysTo(date) % interval == 0;
    }
    }

    return false;
}

QString recurrenceSummary(const Event &event)
{
    switch (event.recurrenceType) {
    case RecurrenceType::None:
        return QString();
    case RecurrenceType::Daily:
        return QStringLiteral("Repeats daily");
    case RecurrenceType::Weekdays:
        return QStringLiteral("Repeats on weekdays");
    case RecurrenceType::Weekly:
        return QStringLiteral("Repeats weekly");
    case RecurrenceType::Custom:
        return QStringLiteral("Repeats every %1 day(s)").arg(event.recurrenceInterval);
    }
    return QString();
}
