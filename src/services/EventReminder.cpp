#include "EventReminder.h"
#include "EventManager.h"
#include "NotificationService.h"

#include <QTimer>
#include <QDateTime>
#include <QDate>
#include <algorithm>

EventReminder::EventReminder(EventManager *eventManager, NotificationService *notificationService, QObject *parent)
    : QObject(parent)
    , m_eventManager(eventManager)
    , m_notificationService(notificationService)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(kCheckIntervalSeconds * 1000);
    connect(m_timer, &QTimer::timeout, this, &EventReminder::checkUpcomingEvents);
    m_timer->start();

    checkUpcomingEvents(); // не ждать первого тика, проверить сразу при старте
}

void EventReminder::checkUpcomingEvents()
{
    const QDateTime now = QDateTime::currentDateTime();
    const QDate today = now.date();

    for (const Event &event : m_eventManager->eventsForDate(today)) {
        const QDateTime eventStart(event.date, event.startTime);
        const qint64 secondsUntilStart = now.secsTo(eventStart);
        const qint64 minutesUntilStart = secondsUntilStart / 60;

        // Уведомляем только о ещё не начавшихся событиях, которые начнутся
        // в пределах ближайших kReminderLeadMinutes минут.
        const bool isUpcoming = secondsUntilStart > 0 && minutesUntilStart <= kReminderLeadMinutes;
        if (!isUpcoming)
            continue;

        const QString key = QString("%1|%2").arg(event.id).arg(event.date.toString(Qt::ISODate));
        if (m_notifiedKeys.contains(key))
            continue;

        m_notifiedKeys.insert(key);

        const QString message = QString("%1\n%2\xE2\x80\x93%3")
            .arg(event.title)
            .arg(event.startTime.toString("HH:mm"))
            .arg(event.endTime.toString("HH:mm"));

        m_notificationService->showNotification(
            QString("In %1 minute(s)").arg(std::max<qint64>(1, minutesUntilStart)), message
        );
    }
}
