// Тесты для models/Event.h: eventOccursOnDate() и recurrenceSummary().
//
// Это чистые функции без обращения к БД - именно на них держится идея
// из раздела 8 Product Context'а ("recurring event хранится как правило,
// а не как набор заранее созданных записей на каждую дату"), поэтому
// они покрываются тестами в первую очередь.
//
// Формат - Qt Test (QTest): методы в "private slots:" автоматически
// находятся и запускаются QTEST_MAIN, каждый метод - один тестовый случай.

#include <QtTest>

#include "models/Event.h"

class TestRecurrence : public QObject
{
    Q_OBJECT

private:
    // Хелпер: минимальное событие с нужным правилом повтора - остальные
    // поля (title/time/...) не влияют на eventOccursOnDate(), поэтому
    // здесь не задаются.
    static Event makeEvent(const QDate &anchorDate, RecurrenceType type,
                            int interval = 1, const QDate &endDate = QDate())
    {
        Event event;
        event.date = anchorDate;
        event.recurrenceType = type;
        event.recurrenceInterval = interval;
        event.recurrenceEndDate = endDate;
        return event;
    }

private slots:
    void oneTime_matchesOnlyAnchorDate()
    {
        const Event event = makeEvent(QDate(2026, 9, 21), RecurrenceType::None);

        QVERIFY(eventOccursOnDate(event, QDate(2026, 9, 21)));
        QVERIFY(!eventOccursOnDate(event, QDate(2026, 9, 22)));
        QVERIFY(!eventOccursOnDate(event, QDate(2026, 9, 14)));
    }

    void oneTime_hasEmptySummary()
    {
        const Event event = makeEvent(QDate(2026, 9, 21), RecurrenceType::None);
        QVERIFY(recurrenceSummary(event).isEmpty());
    }

    void weekly_matchesSameWeekdayEverySevenDays()
    {
        // 21.09.2026 - понедельник (якорная дата).
        const Event event = makeEvent(QDate(2026, 9, 21), RecurrenceType::Weekly);

        QVERIFY(eventOccursOnDate(event, QDate(2026, 9, 21)));  // сама якорная дата
        QVERIFY(eventOccursOnDate(event, QDate(2026, 9, 28)));  // +1 неделя
        QVERIFY(eventOccursOnDate(event, QDate(2026, 10, 12))); // +3 недели
    }

    void weekly_doesNotMatchOtherWeekdays()
    {
        const Event event = makeEvent(QDate(2026, 9, 21), RecurrenceType::Weekly);

        QVERIFY(!eventOccursOnDate(event, QDate(2026, 9, 22))); // вторник
        QVERIFY(!eventOccursOnDate(event, QDate(2026, 9, 27))); // воскресенье
    }

    void weekly_doesNotMatchBeforeAnchor()
    {
        const Event event = makeEvent(QDate(2026, 9, 21), RecurrenceType::Weekly);
        QVERIFY(!eventOccursOnDate(event, QDate(2026, 9, 14))); // неделей раньше якоря
    }

    void weekly_respectsEndDateInclusive()
    {
        const Event event = makeEvent(QDate(2026, 9, 21), RecurrenceType::Weekly,
                                       1, QDate(2026, 10, 5));

        QVERIFY(eventOccursOnDate(event, QDate(2026, 10, 5)));   // дата окончания - последнее вхождение
        QVERIFY(!eventOccursOnDate(event, QDate(2026, 10, 12))); // уже после окончания серии
    }

    void weekly_hasReadableSummary()
    {
        const Event event = makeEvent(QDate(2026, 9, 21), RecurrenceType::Weekly);
        QCOMPARE(recurrenceSummary(event), QStringLiteral("Repeats weekly"));
    }

    void daily_matchesEveryDayFromAnchor()
    {
        const Event event = makeEvent(QDate(2026, 9, 21), RecurrenceType::Daily);

        QVERIFY(eventOccursOnDate(event, QDate(2026, 9, 21)));
        QVERIFY(eventOccursOnDate(event, QDate(2026, 9, 22)));
        QVERIFY(eventOccursOnDate(event, QDate(2026, 10, 1)));
        QVERIFY(!eventOccursOnDate(event, QDate(2026, 9, 20))); // до якоря
    }

    void weekdays_matchesMondayToFridayOnly()
    {
        const Event event = makeEvent(QDate(2026, 9, 21), RecurrenceType::Weekdays);

        // Неделя 21.09 (Пн) - 27.09 (Вс).
        QVERIFY(eventOccursOnDate(event, QDate(2026, 9, 21)));  // Пн
        QVERIFY(eventOccursOnDate(event, QDate(2026, 9, 25)));  // Пт
        QVERIFY(!eventOccursOnDate(event, QDate(2026, 9, 26))); // Сб
        QVERIFY(!eventOccursOnDate(event, QDate(2026, 9, 27))); // Вс
    }

    void custom_matchesEveryNDaysFromAnchor()
    {
        const Event event = makeEvent(QDate(2026, 9, 21), RecurrenceType::Custom, 3);

        QVERIFY(eventOccursOnDate(event, QDate(2026, 9, 21))); // +0 дней
        QVERIFY(eventOccursOnDate(event, QDate(2026, 9, 24))); // +3 дня
        QVERIFY(!eventOccursOnDate(event, QDate(2026, 9, 23))); // +2 дня
    }

    void custom_hasReadableSummaryWithInterval()
    {
        const Event event = makeEvent(QDate(2026, 9, 21), RecurrenceType::Custom, 3);
        QCOMPARE(recurrenceSummary(event), QStringLiteral("Repeats every 3 day(s)"));
    }

    void custom_clampsNonPositiveIntervalToOne()
    {
        // recurrenceInterval = 0 не должен приводить к делению на 0 -
        // eventOccursOnDate() защищается std::max(1, interval) внутри.
        const Event event = makeEvent(QDate(2026, 9, 21), RecurrenceType::Custom, 0);

        QVERIFY(eventOccursOnDate(event, QDate(2026, 9, 21)));
        QVERIFY(eventOccursOnDate(event, QDate(2026, 9, 22))); // ведёт себя как interval=1
    }

    void invalidAnchorDate_neverMatches()
    {
        const Event event = makeEvent(QDate(), RecurrenceType::Daily);
        QVERIFY(!eventOccursOnDate(event, QDate(2026, 9, 21)));
    }

    void invalidQueryDate_neverMatches()
    {
        const Event event = makeEvent(QDate(2026, 9, 21), RecurrenceType::Daily);
        QVERIFY(!eventOccursOnDate(event, QDate()));
    }
};

QTEST_MAIN(TestRecurrence)
#include "test_recurrence.moc"
