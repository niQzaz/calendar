#pragma once

#include <QObject>
#include <QSet>
#include <QString>

class EventManager;
class NotificationService;
class QTimer;

// Периодически проверяет события на сегодня и уведомляет о тех, что
// начинаются в ближайшие kReminderLeadMinutes минут.
//
// Работает поверх QTimer, тикающего раз в kCheckIntervalSeconds секунд -
// более сложный/точный планировщик не нужен: событие с точностью до
// минуты достаточно проверять пару раз в минуту.
class EventReminder : public QObject
{
    Q_OBJECT

public:
    EventReminder(EventManager *eventManager, NotificationService *notificationService, QObject *parent = nullptr);

private slots:
    void checkUpcomingEvents();

private:
    static constexpr int kReminderLeadMinutes = 10;
    static constexpr int kCheckIntervalSeconds = 30;

    EventManager *m_eventManager;
    NotificationService *m_notificationService;
    QTimer *m_timer;

    // Ключи вида "id|дата" для событий, о которых уже уведомили - чтобы не
    // слать одно и то же уведомление на каждом тике таймера. Дата в ключе
    // важна: у повторяющегося события id один и тот же каждый день,
    // а уведомлять о нём нужно заново для каждой даты.
    QSet<QString> m_notifiedKeys;
};
