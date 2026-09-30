// Тесты для models/Execution.h: resolveExecutionStatus() и
// actualDurationSeconds()/startDeviationSeconds()/durationDeviationSeconds().
//
// Это чистые функции без обращения к БД - именно на них держится идея
// из раздела 6 Product Context'а ("не делать автоматическое изменение
// состояния только по таймеру - лучше иметь функцию, которая определяет
// актуальное состояние"), поэтому они покрываются тестами в первую очередь,
// по тому же принципу, что и eventOccursOnDate() в test_recurrence.cpp.

#include <QtTest>

#include "models/Execution.h"

class TestExecution : public QObject
{
    Q_OBJECT

private:
    // Хелпер: минимальная сохранённая execution-запись с нужным статусом
    // и (опционально) actualStart/actualEnd - остальные поля не важны
    // для resolveExecutionStatus().
    static EventExecution makeExecution(ExecutionStatus status,
                                         const QDateTime &actualStart = QDateTime(),
                                         const QDateTime &actualEnd = QDateTime())
    {
        EventExecution execution;
        execution.status = status;
        execution.actualStart = actualStart;
        execution.actualEnd = actualEnd;
        return execution;
    }

    // Хелпер для findCurrentTask()/findNextTask(): минимальный
    // EventWithStatus с нужным временем/статусом - все события одного
    // и того же условного дня, только id для различения.
    static EventWithStatus makeItem(const QString &title, const QTime &start, const QTime &end,
                                     ExecutionStatus status, int id)
    {
        EventWithStatus item;
        item.event.id = id;
        item.event.title = title;
        item.event.date = QDate(2026, 9, 21);
        item.event.startTime = start;
        item.event.endTime = end;
        item.status = status;
        return item;
    }

private slots:
    // --- resolveExecutionStatus() ---

    void noSavedExecution_beforePlannedEnd_isPlanned()
    {
        const QDate day(2026, 9, 21);
        const QTime start(9, 0);
        const QTime end(10, 0);
        const QDateTime now(day, QTime(9, 30)); // задача идёт прямо сейчас

        QCOMPARE(resolveExecutionStatus(day, start, end, nullptr, now), ExecutionStatus::Planned);
    }

    void noSavedExecution_rightAfterPlannedEnd_isStillPlannedWithinGrace()
    {
        const QDate day(2026, 9, 21);
        const QTime start(9, 0);
        const QTime end(10, 0);
        const QDateTime now(day, QTime(10, 30)); // +30 минут после конца, grace по умолчанию 60

        QCOMPARE(resolveExecutionStatus(day, start, end, nullptr, now), ExecutionStatus::Planned);
    }

    void noSavedExecution_afterGracePeriod_isMissed()
    {
        const QDate day(2026, 9, 21);
        const QTime start(9, 0);
        const QTime end(10, 0);
        const QDateTime now(day, QTime(11, 1)); // +61 минута после конца, grace по умолчанию 60

        QCOMPARE(resolveExecutionStatus(day, start, end, nullptr, now), ExecutionStatus::Missed);
    }

    void savedPlannedExecution_behavesSameAsNoExecution()
    {
        // Сохранённая запись со статусом Planned (на практике таким кодом
        // не создаётся, но резолвер должен вести себя одинаково что для
        // nullptr, что для явного Planned) - "ничего не произошло" в обоих
        // случаях означает одно и то же правило grace period.
        const QDate day(2026, 9, 21);
        const QTime start(9, 0);
        const QTime end(10, 0);
        const QDateTime now(day, QTime(11, 1));
        const EventExecution saved = makeExecution(ExecutionStatus::Planned);

        QCOMPARE(resolveExecutionStatus(day, start, end, &saved, now), ExecutionStatus::Missed);
    }

    void runningTask_staysRunningEvenLongAfterGracePeriod()
    {
        // Раздел 5 - обязательное требование: если задача была Started,
        // она НЕ должна автоматически становиться Missed только потому,
        // что plannedEnd (+ grace) прошёл. Остаётся Running, пока
        // пользователь её не завершит явно.
        const QDate day(2026, 9, 21);
        const QTime start(9, 0);
        const QTime end(10, 0);
        const QDateTime startedAt(day, QTime(9, 5));
        const EventExecution saved = makeExecution(ExecutionStatus::Running, startedAt);

        // "Сейчас" - на следующий день, далеко за пределами любого разумного
        // grace period.
        const QDateTime now(day.addDays(1), QTime(9, 0));

        QCOMPARE(resolveExecutionStatus(day, start, end, &saved, now), ExecutionStatus::Running);
    }

    void completedTask_staysCompletedRegardlessOfTime()
    {
        const QDate day(2026, 9, 21);
        const QTime start(9, 0);
        const QTime end(10, 0);
        const EventExecution saved = makeExecution(
            ExecutionStatus::Completed, QDateTime(day, QTime(9, 0)), QDateTime(day, QTime(9, 45)));
        const QDateTime now(day.addYears(1), QTime(0, 0)); // сколько угодно времени спустя

        QCOMPARE(resolveExecutionStatus(day, start, end, &saved, now), ExecutionStatus::Completed);
    }

    void postponedAndCancelled_areReturnedAsIs()
    {
        // Раздел 8 - UI для Postpone/Cancel ещё не строим на этом этапе,
        // но резолвер уже должен уважать эти статусы как явные, если они
        // когда-либо окажутся сохранены (round-trip на будущее).
        const QDate day(2026, 9, 21);
        const QTime start(9, 0);
        const QTime end(10, 0);
        const QDateTime now(day.addDays(30), QTime(0, 0));

        const EventExecution postponed = makeExecution(ExecutionStatus::Postponed);
        QCOMPARE(resolveExecutionStatus(day, start, end, &postponed, now), ExecutionStatus::Postponed);

        const EventExecution cancelled = makeExecution(ExecutionStatus::Cancelled);
        QCOMPARE(resolveExecutionStatus(day, start, end, &cancelled, now), ExecutionStatus::Cancelled);
    }

    void zeroGracePeriod_missedRightAtPlannedEnd()
    {
        const QDate day(2026, 9, 21);
        const QTime start(9, 0);
        const QTime end(10, 0);

        // Ровно в plannedEnd, grace=0: формула - plannedEnd + 0 < now,
        // строгое "<", так что в саму секунду plannedEnd ещё Planned...
        QCOMPARE(resolveExecutionStatus(day, start, end, nullptr, QDateTime(day, end), 0),
                 ExecutionStatus::Planned);
        // ...а секундой позже - уже Missed.
        QCOMPARE(resolveExecutionStatus(day, start, end, nullptr, QDateTime(day, end).addSecs(1), 0),
                 ExecutionStatus::Missed);
    }

    void customGracePeriod_120Minutes_notMissedWhereDefaultWouldBe()
    {
        const QDate day(2026, 9, 21);
        const QTime start(9, 0);
        const QTime end(10, 0);
        const QDateTime now(day, QTime(11, 30)); // +90 минут после конца

        // С дефолтным grace (60 мин) это уже Missed...
        QCOMPARE(resolveExecutionStatus(day, start, end, nullptr, now), ExecutionStatus::Missed);
        // ...а с grace=120 - ещё нет.
        QCOMPARE(resolveExecutionStatus(day, start, end, nullptr, now, 120), ExecutionStatus::Planned);
    }

    // --- actualDurationSeconds() ---

    void actualDuration_computesFromValidStartAndEnd()
    {
        const EventExecution execution = makeExecution(
            ExecutionStatus::Completed,
            QDateTime(QDate(2026, 9, 21), QTime(9, 0)),
            QDateTime(QDate(2026, 9, 21), QTime(9, 45))
        );

        qint64 seconds = 0;
        QVERIFY(actualDurationSeconds(execution, seconds));
        QCOMPARE(seconds, static_cast<qint64>(45 * 60));
    }

    void actualDuration_falseWhenStartMissing()
    {
        EventExecution execution = makeExecution(ExecutionStatus::Completed);
        execution.actualEnd = QDateTime(QDate(2026, 9, 21), QTime(9, 45));

        qint64 seconds = 0;
        QVERIFY(!actualDurationSeconds(execution, seconds));
    }

    void actualDuration_falseWhenEndMissing()
    {
        EventExecution execution = makeExecution(ExecutionStatus::Running);
        execution.actualStart = QDateTime(QDate(2026, 9, 21), QTime(9, 0));

        qint64 seconds = 0;
        QVERIFY(!actualDurationSeconds(execution, seconds));
    }

    // --- startDeviationSeconds() ---

    void startDeviation_positiveWhenStartedLate()
    {
        const QDate day(2026, 9, 21);
        const QTime plannedStart(9, 0);
        const EventExecution execution = makeExecution(
            ExecutionStatus::Running, QDateTime(day, QTime(9, 15)));

        qint64 seconds = 0;
        QVERIFY(startDeviationSeconds(day, plannedStart, execution, seconds));
        QCOMPARE(seconds, static_cast<qint64>(15 * 60));
    }

    void startDeviation_negativeWhenStartedEarly()
    {
        const QDate day(2026, 9, 21);
        const QTime plannedStart(9, 0);
        const EventExecution execution = makeExecution(
            ExecutionStatus::Running, QDateTime(day, QTime(8, 50)));

        qint64 seconds = 0;
        QVERIFY(startDeviationSeconds(day, plannedStart, execution, seconds));
        QCOMPARE(seconds, static_cast<qint64>(-10 * 60));
    }

    void startDeviation_falseWhenActualStartMissing()
    {
        const QDate day(2026, 9, 21);
        const QTime plannedStart(9, 0);
        const EventExecution execution = makeExecution(ExecutionStatus::Planned);

        qint64 seconds = 0;
        QVERIFY(!startDeviationSeconds(day, plannedStart, execution, seconds));
    }

    // --- durationDeviationSeconds() ---

    void durationDeviation_positiveWhenTookLonger()
    {
        const QDate day(2026, 9, 21);
        const QTime plannedStart(9, 0);
        const QTime plannedEnd(9, 30); // план - 30 минут
        const EventExecution execution = makeExecution(
            ExecutionStatus::Completed,
            QDateTime(day, QTime(9, 0)),
            QDateTime(day, QTime(9, 50)) // факт - 50 минут
        );

        qint64 seconds = 0;
        QVERIFY(durationDeviationSeconds(plannedStart, plannedEnd, execution, seconds));
        QCOMPARE(seconds, static_cast<qint64>(20 * 60));
    }

    void durationDeviation_negativeWhenTookShorter()
    {
        const QDate day(2026, 9, 21);
        const QTime plannedStart(9, 0);
        const QTime plannedEnd(10, 0); // план - 60 минут
        const EventExecution execution = makeExecution(
            ExecutionStatus::Completed,
            QDateTime(day, QTime(9, 0)),
            QDateTime(day, QTime(9, 20)) // факт - 20 минут
        );

        qint64 seconds = 0;
        QVERIFY(durationDeviationSeconds(plannedStart, plannedEnd, execution, seconds));
        QCOMPARE(seconds, static_cast<qint64>(-40 * 60));
    }

    void durationDeviation_falseWhenDataMissing()
    {
        const QTime plannedStart(9, 0);
        const QTime plannedEnd(10, 0);
        const EventExecution execution = makeExecution(ExecutionStatus::Planned);

        qint64 seconds = 0;
        QVERIFY(!durationDeviationSeconds(plannedStart, plannedEnd, execution, seconds));
    }

    // --- executionStatusToString()/executionStatusFromString() ---

    void executionStatus_allValuesRoundTripThroughString()
    {
        const QVector<ExecutionStatus> allStatuses = {
            ExecutionStatus::Planned, ExecutionStatus::Running, ExecutionStatus::Completed,
            ExecutionStatus::Missed, ExecutionStatus::Postponed, ExecutionStatus::Cancelled
        };

        for (ExecutionStatus status : allStatuses) {
            const QString asString = executionStatusToString(status);
            QVERIFY(!asString.isEmpty());
            QCOMPARE(executionStatusFromString(asString), status);
        }
    }

    void executionStatusFromString_unknownDefaultsToPlanned()
    {
        QCOMPARE(executionStatusFromString("not_a_real_status"), ExecutionStatus::Planned);
        QCOMPARE(executionStatusFromString(QString()), ExecutionStatus::Planned);
    }

    // --- findCurrentTask() ---

    void findCurrentTask_prefersRunningOverPlannedWindow()
    {
        // Running-задача выигрывает у "плановое окно содержит now", даже
        // если формально уже должна была закончиться по расписанию.
        const QVector<EventWithStatus> events = {
            makeItem("Meeting", QTime(9, 0), QTime(10, 0), ExecutionStatus::Planned, 1),
            makeItem("Deep work", QTime(8, 0), QTime(8, 30), ExecutionStatus::Running, 2),
        };

        EventWithStatus current;
        QVERIFY(findCurrentTask(events, QTime(9, 30), current));
        QCOMPARE(current.event.id, 2);
    }

    void findCurrentTask_findsPlannedTaskWhoseWindowContainsNow()
    {
        const QVector<EventWithStatus> events = {
            makeItem("Meeting", QTime(9, 0), QTime(10, 0), ExecutionStatus::Planned, 1),
        };

        EventWithStatus current;
        QVERIFY(findCurrentTask(events, QTime(9, 30), current));
        QCOMPARE(current.event.id, 1);
    }

    void findCurrentTask_ignoresCompletedEvenIfWindowContainsNow()
    {
        // Завершённая задача не может быть "текущей", даже если now всё ещё
        // формально попадает в её старое плановое окно.
        const QVector<EventWithStatus> events = {
            makeItem("Meeting", QTime(9, 0), QTime(10, 0), ExecutionStatus::Completed, 1),
        };

        EventWithStatus current;
        QVERIFY(!findCurrentTask(events, QTime(9, 30), current));
    }

    void findCurrentTask_returnsFalseWhenNothingMatches()
    {
        const QVector<EventWithStatus> events = {
            makeItem("Later", QTime(14, 0), QTime(15, 0), ExecutionStatus::Planned, 1),
        };

        EventWithStatus current;
        QVERIFY(!findCurrentTask(events, QTime(9, 30), current));
    }

    // --- findNextTask() ---

    void findNextTask_picksEarliestUpcoming()
    {
        const QVector<EventWithStatus> events = {
            makeItem("Later", QTime(15, 0), QTime(16, 0), ExecutionStatus::Planned, 1),
            makeItem("Sooner", QTime(11, 0), QTime(12, 0), ExecutionStatus::Planned, 2),
        };

        EventWithStatus next;
        QVERIFY(findNextTask(events, QTime(9, 0), nullptr, next));
        QCOMPARE(next.event.id, 2);
    }

    void findNextTask_excludesCurrentTask()
    {
        const EventWithStatus current = makeItem("Now", QTime(9, 0), QTime(10, 0), ExecutionStatus::Running, 1);
        const QVector<EventWithStatus> events = {
            current,
            makeItem("Next", QTime(11, 0), QTime(12, 0), ExecutionStatus::Planned, 2),
        };

        EventWithStatus next;
        QVERIFY(findNextTask(events, QTime(9, 30), &current, next));
        QCOMPARE(next.event.id, 2);
    }

    void findNextTask_excludesCompletedCancelledPostponed()
    {
        const QVector<EventWithStatus> events = {
            makeItem("Done already", QTime(11, 0), QTime(12, 0), ExecutionStatus::Completed, 1),
            makeItem("Cancelled", QTime(11, 30), QTime(12, 30), ExecutionStatus::Cancelled, 2),
            makeItem("Postponed", QTime(11, 45), QTime(12, 45), ExecutionStatus::Postponed, 3),
            makeItem("Actually next", QTime(13, 0), QTime(14, 0), ExecutionStatus::Planned, 4),
        };

        EventWithStatus next;
        QVERIFY(findNextTask(events, QTime(9, 0), nullptr, next));
        QCOMPARE(next.event.id, 4);
    }

    void findNextTask_returnsFalseWhenNothingUpcoming()
    {
        const QVector<EventWithStatus> events = {
            makeItem("Already passed", QTime(8, 0), QTime(9, 0), ExecutionStatus::Planned, 1),
        };

        EventWithStatus next;
        QVERIFY(!findNextTask(events, QTime(9, 30), nullptr, next));
    }
};

QTEST_MAIN(TestExecution)
#include "test_execution.moc"
