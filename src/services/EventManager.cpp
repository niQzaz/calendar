#include "EventManager.h"

#include <algorithm>

int EventManager::addEvent(const Event &event)
{
    Event newEvent = event;
    newEvent.id = m_nextId++;
    m_events.append(newEvent);
    return newEvent.id;
}

bool EventManager::removeEvent(int id)
{
    for (int i = 0; i < m_events.size(); ++i) {
        if (m_events[i].id == id) {
            m_events.removeAt(i);
            return true;
        }
    }
    return false;
}

QVector<Event> EventManager::eventsForDate(const QDate &date) const
{
    QVector<Event> result;
    for (const Event &event : m_events) {
        if (event.date == date)
            result.append(event);
    }

    std::sort(result.begin(), result.end(), [](const Event &a, const Event &b) {
        return a.startTime < b.startTime;
    });

    return result;
}

QSet<QDate> EventManager::datesWithEvents() const
{
    QSet<QDate> dates;
    for (const Event &event : m_events)
        dates.insert(event.date);
    return dates;
}
