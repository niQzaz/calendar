// Тесты для services/EventManager.h - CRUD и выборки по датам
// (eventsForDate/eventsInRange/datesWithEvents), включая expansion
// повторяющихся событий.
//
// Каждый тест работает со своей собственной in-memory SQLite базой
// (":memory:" + уникальное имя подключения на тест) - см. init()/cleanup()
// ниже. Реальный файл пользователя (~/.../calendar.db) не затрагивается:
// это возможно благодаря конструктору EventManager(databasePath,
// connectionName), добавленному специально для тестов.
//
// CsvImporter здесь не тестируется - по договорённости, т.к. уже проверен
// на полноценной сборке отдельно.

#include <QtTest>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "services/EventManager.h"
#include "models/Category.h"
#include "models/Execution.h"

class TestEventManager : public QObject
{
    Q_OBJECT

private slots:
    void init();    // создаёт свежий EventManager перед каждым тестом
    void cleanup(); // закрывает его после каждого теста

    void addEvent_assignsIdAndPersistsAllFields();
    void addEvent_and_updateEvent_workWhenTimezoneFieldIsUnset();
    void eventById_returnsFalseForMissingId();

    void updateEvent_changesFieldsAndReturnsTrue();
    void updateEvent_returnsFalseForMissingId();

    void removeEvent_deletesAndReturnsTrue();
    void removeEvent_returnsFalseForMissingId();

    void incrementPomodoroCount_increasesCounter();
    void incrementPomodoroCount_returnsFalseForMissingId();

    void eventsForDate_returnsOneTimeEventOnExactDateOnly();
    void eventsForDate_includesWeeklyOccurrenceOnMatchingWeekday();
    void eventsForDate_excludesWeeklyOccurrenceOnOtherWeekday();
    void eventsForDate_sortedByStartTime();

    void datesWithEvents_includesOneTimeAndRecurringDates();
    void datesWithEvents_excludesDatesWithoutEvents();

    void eventsInRange_expandsRecurringOccurrencesAcrossRange();
    void eventsInRange_sortedByDateThenStartTime();

    // --- Категории (MVP2) ---
    void addCategory_assignsIdAndPersists();
    void categoryById_returnsFalseForMissingId();
    void updateCategory_changesFieldsAndReturnsTrue();
    void removeCategory_clearsCategoryIdOnAffectedEvents();
    void allCategories_returnsSortedByNameCaseInsensitive();
    void eventById_includesCategoryNameAndColorViaJoin();
    void eventById_hasEmptyCategoryFieldsWhenUncategorized();
    void eventsInRange_includesCategoryColorForRecurringOccurrence();

    // --- Execution (MVP 3.0) ---
    void executionForOccurrence_returnsFalseWhenNoAction();
    void startOccurrence_createsRunningExecutionWithActualStart();
    void completeOccurrence_afterStart_setsCompletedWithActualEnd();
    void completeOccurrence_withoutStart_setsCompletedWithoutActualTimes();
    void effectiveStatus_isMissedPastGracePeriodWithNoAction();
    void effectiveStatus_startedTaskDoesNotBecomeMissedPastPlannedEnd();
    void recurringEvent_occurrencesTrackExecutionIndependently();
    void removeEvent_alsoRemovesItsExecutionRecords();
    void nowSnapshot_findsCurrentAndNextTask();
    void nowSnapshot_runningTaskIsCurrentEvenAfterItsWindowPassed();

    // --- Pomodoro/Execution интеграция ---
    // MainWindow::startPomodoroForOccurrence() - GUI-код, в этом проекте
    // виджеты headless QTest'ом не покрываются (тот же прецедент, что и
    // drag/resize в TimeGridView). Тесты ниже проверяют ту же самую
    // "check-then-start" логику (effectiveStatus(), и только если не
    // Running - startOccurrence()) напрямую через EventManager - это и
    // есть единственная НОВАЯ логика этой интеграции, PomodoroWidget сам
    // в неё не добавляет ничего, что стоило бы тестировать на этом уровне.
    void pomodoroStartPattern_firstCallStartsOccurrence();
    void pomodoroStartPattern_secondCallDoesNotOverwriteActualStart();
    void nowSnapshot_currentTaskOccurrenceDateMatchesQueriedDay();
    void nowSnapshot_differentDaysYieldIndependentExecutionsForRecurringEvent();
    void nowSnapshot_noCurrentTaskWhenNothingScheduled();

    // --- MVP 3.1 - Missed Task ---
    void recurringOccurrence_missedTodayDoesNotAffectDifferentOccurrence();
    void nowSnapshot_missedCountReflectsMissedEvents();

    // --- MVP3 - Reschedule ---
    void rescheduleOccurrence_oneTimePlanned_updatesDateTimeKeepingSameId();
    void rescheduleOccurrence_oneTimeMissed_succeeds();
    void rescheduleOccurrence_oneTimeRunning_rejected();
    void rescheduleOccurrence_oneTimeCompleted_rejected();
    void rescheduleOccurrence_recurringPlanned_createsNewOneTimeEventWithCopiedFields();
    void rescheduleOccurrence_recurringPlanned_originalDisappearsOthersRemain();
    void rescheduleOccurrence_recurringTemplateUnchangedAfterReschedule();
    void rescheduleOccurrence_recurringMissed_succeeds();
    void rescheduleOccurrence_recurringRunning_rejected();
    void rescheduleOccurrence_recurringCompleted_rejected();
    void rescheduleOccurrence_duplicateRescheduleOfSameOccurrenceRejected();
    void rescheduleOccurrence_exceptionForEventADoesNotAffectEventBSameDate();
    void rescheduleOccurrence_doesNotCreateExecutionForPlannedOrMissed();
    void rescheduleOccurrence_newEventCanSubsequentlyStartAndComplete();

    // --- Этап C ---
    void datesWithEvents_excludesRescheduledRecurringOccurrence();
    void rescheduleOccurrence_persistsAcrossManagerRecreation();

    // --- Этап D1: защита переходов ---
    void startOccurrence_fromNoRecordAndMissed_succeeds();
    void startOccurrence_fromRunning_rejectedAndKeepsActualStart();
    void startOccurrence_fromCompleted_rejectedAndKeepsRecord();
    void startOccurrence_fromPostponedAndCancelled_rejected();
    void startOccurrence_rejectionAffectsOnlyThatOccurrenceOfSeries();
    void completeOccurrence_fromNoRecordAndRunning_succeeds();
    void completeOccurrence_fromCompleted_rejectedAndKeepsActualEnd();
    void completeOccurrence_fromPostponedAndCancelled_rejected();

    // --- Этап D1: защита редактирования ---
    void updateEvent_oneTimeWithHistory_rejectsDateAndStartChange();
    void updateEvent_oneTimeCompleted_allowsEndTitleAndDescriptionChange();
    void updateEvent_oneTimePlanned_allowsAnyChange();
    void updateEvent_seriesWithCompletedOccurrence_allowsTitleAndTimeChange();
    void updateEvent_seriesWithCompletedOccurrence_anchorShift();
    void updateEvent_seriesWithCompletedOccurrence_intervalChange();
    void updateEvent_seriesWithCompletedOccurrence_recurrenceEndDate();
    void updateEvent_seriesToOneTime_blockedOnlyWhenOtherDatesHaveHistory();
    void updateEvent_preExistingOrphanHistoryDoesNotBlockEdits();
    void updateEvent_rejectedEditChangesNothing();
    void executionConflictsForEdit_reportsConflictingDates();

    // --- Этап D2: NOW ---
    void nowSnapshot_backToBackTasks_switchAtBoundary();
    void nowSnapshot_rescheduledIntoToday_becomesCurrentThenNext();

    // --- Этап D4: удаление ---
    void removeEvent_oneTimeDeletesEventAndExecutions();
    void removeEvent_seriesClearsExceptionsAndExecutionsKeepsMovedCopy();
    void removeEvent_missingIdReturnsFalse();
    void removeEvent_failureRollsBackEverything();

private:
    // Хелперы Этапа D. Сырой SQL нужен, чтобы создавать состояния, которых
    // нет в UI (Postponed/Cancelled), и чтобы смоделировать сбой удаления.
    void insertRawExecution(int eventId, const QDate &date, const QString &status);
    int countRows(const QString &table, int eventId) const;

    EventManager *m_manager = nullptr;
    QString m_connectionName;
};

void TestEventManager::init()
{
    // Уникальное имя подключения на каждый тест - иначе повторное
    // addDatabase() с тем же именем в рамках одного процесса будет либо
    // переиспользовать старое соединение, либо ругаться в консоль.
    static int counter = 0;
    const QString connectionName = QStringLiteral("test_event_manager_%1").arg(++counter);
    m_connectionName = connectionName;
    m_manager = new EventManager(QStringLiteral(":memory:"), connectionName);
}

void TestEventManager::cleanup()
{
    delete m_manager; // деструктор закрывает и удаляет соединение
    m_manager = nullptr;
}

void TestEventManager::addEvent_assignsIdAndPersistsAllFields()
{
    Event event;
    event.title = QStringLiteral("Standup");
    event.date = QDate(2026, 9, 21);
    event.startTime = QTime(9, 0);
    event.endTime = QTime(9, 15);
    event.description = QStringLiteral("Daily sync");
    event.priority = 2;
    event.timezone = QStringLiteral("Europe/Moscow");

    const int id = m_manager->addEvent(event);
    QVERIFY(id > 0);

    Event stored;
    QVERIFY(m_manager->eventById(id, stored));
    QCOMPARE(stored.title, event.title);
    QCOMPARE(stored.date, event.date);
    QCOMPARE(stored.startTime, event.startTime);
    QCOMPARE(stored.endTime, event.endTime);
    QCOMPARE(stored.description, event.description);
    QCOMPARE(stored.priority, event.priority);
    QCOMPARE(stored.timezone, event.timezone);
    QCOMPARE(stored.recurrenceType, RecurrenceType::None);
    QCOMPARE(stored.pomodorosCompleted, 0);
}

void TestEventManager::addEvent_and_updateEvent_workWhenTimezoneFieldIsUnset()
{
    // Регрессионный тест. Event::timezone без явного присваивания -
    // default-constructed ("null") QString, а не просто пустая строка.
    // Qt SQL биндит null-QString как SQL NULL, а не как "" - из-за этого
    // addEvent()/updateEvent() падали с "NOT NULL constraint failed:
    // events.timezone" (колонка объявлена NOT NULL). Это основной путь:
    // EventDialog (создание/редактирование события через UI) и "простой"
    // формат CSV никогда не трогают event.timezone.
    Event event;
    event.title = QStringLiteral("No timezone set");
    event.date = QDate(2026, 9, 21);
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    // event.timezone намеренно не трогаем - остаётся null QString,
    // именно так, как его оставляет EventDialog.

    const int id = m_manager->addEvent(event);
    QVERIFY(id > 0);

    Event stored;
    QVERIFY(m_manager->eventById(id, stored));
    QVERIFY(stored.timezone.isEmpty());

    // Тот же сценарий для updateEvent() - id уже назначен базой, остальные
    // поля меняем, timezone по-прежнему не трогаем.
    Event updated = stored;
    updated.title = QStringLiteral("Still no timezone");
    QVERIFY(m_manager->updateEvent(updated));

    Event storedAfterUpdate;
    QVERIFY(m_manager->eventById(id, storedAfterUpdate));
    QCOMPARE(storedAfterUpdate.title, QStringLiteral("Still no timezone"));
    QVERIFY(storedAfterUpdate.timezone.isEmpty());
}

void TestEventManager::eventById_returnsFalseForMissingId()
{
    Event outEvent;
    QVERIFY(!m_manager->eventById(9999, outEvent));
}

void TestEventManager::updateEvent_changesFieldsAndReturnsTrue()
{
    Event event;
    event.title = QStringLiteral("Original");
    event.date = QDate(2026, 9, 21);
    event.startTime = QTime(10, 0);
    event.endTime = QTime(11, 0);
    const int id = m_manager->addEvent(event);
    QVERIFY(id > 0);

    Event updated = event;
    updated.id = id;
    updated.title = QStringLiteral("Renamed");
    updated.startTime = QTime(12, 0);
    updated.endTime = QTime(13, 0);

    QVERIFY(m_manager->updateEvent(updated));

    Event stored;
    QVERIFY(m_manager->eventById(id, stored));
    QCOMPARE(stored.title, QStringLiteral("Renamed"));
    QCOMPARE(stored.startTime, QTime(12, 0));
    QCOMPARE(stored.endTime, QTime(13, 0));
}

void TestEventManager::updateEvent_returnsFalseForMissingId()
{
    Event event;
    event.id = 9999;
    event.title = QStringLiteral("Ghost");
    event.date = QDate(2026, 9, 21);
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);

    QVERIFY(!m_manager->updateEvent(event));
}

void TestEventManager::removeEvent_deletesAndReturnsTrue()
{
    Event event;
    event.title = QStringLiteral("To delete");
    event.date = QDate(2026, 9, 21);
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    const int id = m_manager->addEvent(event);

    QVERIFY(m_manager->removeEvent(id));

    Event outEvent;
    QVERIFY(!m_manager->eventById(id, outEvent));
}

void TestEventManager::removeEvent_returnsFalseForMissingId()
{
    QVERIFY(!m_manager->removeEvent(9999));
}

void TestEventManager::incrementPomodoroCount_increasesCounter()
{
    Event event;
    event.title = QStringLiteral("Focus block");
    event.date = QDate(2026, 9, 21);
    event.startTime = QTime(9, 0);
    event.endTime = QTime(9, 25);
    const int id = m_manager->addEvent(event);

    QVERIFY(m_manager->incrementPomodoroCount(id));
    QVERIFY(m_manager->incrementPomodoroCount(id));

    Event stored;
    QVERIFY(m_manager->eventById(id, stored));
    QCOMPARE(stored.pomodorosCompleted, 2);
}

void TestEventManager::incrementPomodoroCount_returnsFalseForMissingId()
{
    QVERIFY(!m_manager->incrementPomodoroCount(9999));
}

void TestEventManager::eventsForDate_returnsOneTimeEventOnExactDateOnly()
{
    Event event;
    event.title = QStringLiteral("Dentist");
    event.date = QDate(2026, 9, 22);
    event.startTime = QTime(14, 0);
    event.endTime = QTime(15, 0);
    m_manager->addEvent(event);

    const QVector<Event> onDate = m_manager->eventsForDate(QDate(2026, 9, 22));
    QCOMPARE(onDate.size(), 1);
    QCOMPARE(onDate.first().title, QStringLiteral("Dentist"));

    const QVector<Event> otherDate = m_manager->eventsForDate(QDate(2026, 9, 23));
    QVERIFY(otherDate.isEmpty());
}

void TestEventManager::eventsForDate_includesWeeklyOccurrenceOnMatchingWeekday()
{
    // 21.09.2026 - понедельник (якорная дата серии).
    Event weekly;
    weekly.title = QStringLiteral("Wake up + stretch");
    weekly.date = QDate(2026, 9, 21);
    weekly.startTime = QTime(6, 20);
    weekly.endTime = QTime(6, 30);
    weekly.recurrenceType = RecurrenceType::Weekly;
    m_manager->addEvent(weekly);

    // Через 3 недели, тот же понедельник - occurrence должен сгенерироваться
    // "на лету", а не быть отдельной строкой в БД.
    const QVector<Event> occurrences = m_manager->eventsForDate(QDate(2026, 10, 12));
    QCOMPARE(occurrences.size(), 1);
    QCOMPARE(occurrences.first().title, QStringLiteral("Wake up + stretch"));
    QCOMPARE(occurrences.first().date, QDate(2026, 10, 12)); // подставлена запрошенная дата
    QCOMPARE(occurrences.first().startTime, QTime(6, 20));   // время взято из шаблона
}

void TestEventManager::eventsForDate_excludesWeeklyOccurrenceOnOtherWeekday()
{
    Event weekly;
    weekly.title = QStringLiteral("Wake up + stretch");
    weekly.date = QDate(2026, 9, 21); // понедельник
    weekly.startTime = QTime(6, 20);
    weekly.endTime = QTime(6, 30);
    weekly.recurrenceType = RecurrenceType::Weekly;
    m_manager->addEvent(weekly);

    const QVector<Event> occurrences = m_manager->eventsForDate(QDate(2026, 9, 22)); // вторник
    QVERIFY(occurrences.isEmpty());
}

void TestEventManager::eventsForDate_sortedByStartTime()
{
    Event later;
    later.title = QStringLiteral("Later");
    later.date = QDate(2026, 9, 21);
    later.startTime = QTime(15, 0);
    later.endTime = QTime(16, 0);

    Event earlier;
    earlier.title = QStringLiteral("Earlier");
    earlier.date = QDate(2026, 9, 21);
    earlier.startTime = QTime(9, 0);
    earlier.endTime = QTime(10, 0);

    m_manager->addEvent(later);  // намеренно добавлены в "неправильном" порядке
    m_manager->addEvent(earlier);

    const QVector<Event> events = m_manager->eventsForDate(QDate(2026, 9, 21));
    QCOMPARE(events.size(), 2);
    QCOMPARE(events.at(0).title, QStringLiteral("Earlier"));
    QCOMPARE(events.at(1).title, QStringLiteral("Later"));
}

void TestEventManager::datesWithEvents_includesOneTimeAndRecurringDates()
{
    Event oneTime;
    oneTime.title = QStringLiteral("One-off");
    oneTime.date = QDate(2026, 9, 23);
    oneTime.startTime = QTime(10, 0);
    oneTime.endTime = QTime(11, 0);
    m_manager->addEvent(oneTime);

    Event weekly;
    weekly.title = QStringLiteral("Weekly");
    weekly.date = QDate(2026, 9, 21); // понедельник
    weekly.startTime = QTime(6, 20);
    weekly.endTime = QTime(6, 30);
    weekly.recurrenceType = RecurrenceType::Weekly;
    m_manager->addEvent(weekly);

    const QSet<QDate> dates = m_manager->datesWithEvents(QDate(2026, 9, 21), QDate(2026, 10, 4));
    QVERIFY(dates.contains(QDate(2026, 9, 23))); // разовое событие
    QVERIFY(dates.contains(QDate(2026, 9, 21))); // повторяющееся, неделя 1
    QVERIFY(dates.contains(QDate(2026, 9, 28))); // повторяющееся, неделя 2
}

void TestEventManager::datesWithEvents_excludesDatesWithoutEvents()
{
    Event weekly;
    weekly.title = QStringLiteral("Weekly");
    weekly.date = QDate(2026, 9, 21); // понедельник
    weekly.startTime = QTime(6, 20);
    weekly.endTime = QTime(6, 30);
    weekly.recurrenceType = RecurrenceType::Weekly;
    m_manager->addEvent(weekly);

    const QSet<QDate> dates = m_manager->datesWithEvents(QDate(2026, 9, 21), QDate(2026, 10, 4));
    QVERIFY(!dates.contains(QDate(2026, 9, 22))); // вторник - нет событий
}

void TestEventManager::eventsInRange_expandsRecurringOccurrencesAcrossRange()
{
    Event weekly;
    weekly.title = QStringLiteral("Weekly");
    weekly.date = QDate(2026, 9, 21); // понедельник
    weekly.startTime = QTime(6, 20);
    weekly.endTime = QTime(6, 30);
    weekly.recurrenceType = RecurrenceType::Weekly;
    m_manager->addEvent(weekly);

    // Диапазон [21.09 Пн, 04.10 Вс] содержит ровно два понедельника: 21.09 и 28.09.
    const QVector<Event> events = m_manager->eventsInRange(QDate(2026, 9, 21), QDate(2026, 10, 4));
    QCOMPARE(events.size(), 2);
    QCOMPARE(events.at(0).date, QDate(2026, 9, 21));
    QCOMPARE(events.at(1).date, QDate(2026, 9, 28));
}

void TestEventManager::eventsInRange_sortedByDateThenStartTime()
{
    Event second;
    second.title = QStringLiteral("Second day");
    second.date = QDate(2026, 9, 22);
    second.startTime = QTime(8, 0);
    second.endTime = QTime(9, 0);

    Event first;
    first.title = QStringLiteral("First day");
    first.date = QDate(2026, 9, 21);
    first.startTime = QTime(9, 0);
    first.endTime = QTime(10, 0);

    m_manager->addEvent(second); // намеренно добавлены в "неправильном" порядке
    m_manager->addEvent(first);

    const QVector<Event> events = m_manager->eventsInRange(QDate(2026, 9, 21), QDate(2026, 9, 22));
    QCOMPARE(events.size(), 2);
    QCOMPARE(events.at(0).title, QStringLiteral("First day"));
    QCOMPARE(events.at(1).title, QStringLiteral("Second day"));
}

void TestEventManager::addCategory_assignsIdAndPersists()
{
    Category category;
    category.name = QStringLiteral("Work");
    category.color = QStringLiteral("#4A90D9");

    const int id = m_manager->addCategory(category);
    QVERIFY(id > 0);

    Category stored;
    QVERIFY(m_manager->categoryById(id, stored));
    QCOMPARE(stored.name, category.name);
    QCOMPARE(stored.color, category.color);
}

void TestEventManager::categoryById_returnsFalseForMissingId()
{
    Category outCategory;
    QVERIFY(!m_manager->categoryById(9999, outCategory));
}

void TestEventManager::updateCategory_changesFieldsAndReturnsTrue()
{
    Category category;
    category.name = QStringLiteral("Work");
    category.color = QStringLiteral("#4A90D9");
    const int id = m_manager->addCategory(category);
    QVERIFY(id > 0);

    Category updated;
    updated.id = id;
    updated.name = QStringLiteral("Deep Work");
    updated.color = QStringLiteral("#D94A4A");
    QVERIFY(m_manager->updateCategory(updated));

    Category stored;
    QVERIFY(m_manager->categoryById(id, stored));
    QCOMPARE(stored.name, QStringLiteral("Deep Work"));
    QCOMPARE(stored.color, QStringLiteral("#D94A4A"));
}

void TestEventManager::removeCategory_clearsCategoryIdOnAffectedEvents()
{
    Category category;
    category.name = QStringLiteral("Work");
    category.color = QStringLiteral("#4A90D9");
    const int categoryId = m_manager->addCategory(category);
    QVERIFY(categoryId > 0);

    Event event;
    event.title = QStringLiteral("Categorized");
    event.date = QDate(2026, 9, 21);
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    event.categoryId = categoryId;
    const int eventId = m_manager->addEvent(event);
    QVERIFY(eventId > 0);

    QVERIFY(m_manager->removeCategory(categoryId));

    // Категории больше нет...
    Category outCategory;
    QVERIFY(!m_manager->categoryById(categoryId, outCategory));

    // ...но само событие осталось, просто потеряло категорию, а не исчезло вместе с ней.
    Event storedEvent;
    QVERIFY(m_manager->eventById(eventId, storedEvent));
    QCOMPARE(storedEvent.categoryId, -1);
    QVERIFY(storedEvent.categoryName.isEmpty());
    QVERIFY(storedEvent.categoryColor.isEmpty());
}

void TestEventManager::allCategories_returnsSortedByNameCaseInsensitive()
{
    Category work;
    work.name = QStringLiteral("work");
    work.color = QStringLiteral("#4A90D9");
    m_manager->addCategory(work);

    Category health;
    health.name = QStringLiteral("Health");
    health.color = QStringLiteral("#4AD97A");
    m_manager->addCategory(health);

    const QVector<Category> categories = m_manager->allCategories();
    QCOMPARE(categories.size(), 2);
    QCOMPARE(categories.at(0).name, QStringLiteral("Health")); // 'H' < 'w' без учёта регистра
    QCOMPARE(categories.at(1).name, QStringLiteral("work"));
}

void TestEventManager::eventById_includesCategoryNameAndColorViaJoin()
{
    Category category;
    category.name = QStringLiteral("Health");
    category.color = QStringLiteral("#4AD97A");
    const int categoryId = m_manager->addCategory(category);
    QVERIFY(categoryId > 0);

    Event event;
    event.title = QStringLiteral("Gym");
    event.date = QDate(2026, 9, 21);
    event.startTime = QTime(7, 0);
    event.endTime = QTime(8, 0);
    event.categoryId = categoryId;
    const int eventId = m_manager->addEvent(event);
    QVERIFY(eventId > 0);

    Event stored;
    QVERIFY(m_manager->eventById(eventId, stored));
    QCOMPARE(stored.categoryId, categoryId);
    QCOMPARE(stored.categoryName, QStringLiteral("Health"));
    QCOMPARE(stored.categoryColor, QStringLiteral("#4AD97A"));
}

void TestEventManager::eventById_hasEmptyCategoryFieldsWhenUncategorized()
{
    Event event;
    event.title = QStringLiteral("No category");
    event.date = QDate(2026, 9, 21);
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    // event.categoryId намеренно не трогаем - остаётся -1 (значение по умолчанию).
    const int eventId = m_manager->addEvent(event);
    QVERIFY(eventId > 0);

    Event stored;
    QVERIFY(m_manager->eventById(eventId, stored));
    QCOMPARE(stored.categoryId, -1);
    QVERIFY(stored.categoryName.isEmpty());
    QVERIFY(stored.categoryColor.isEmpty());
}

void TestEventManager::eventsInRange_includesCategoryColorForRecurringOccurrence()
{
    // allRecurringTemplates() - отдельный SQL-запрос от прямого запроса
    // "не повторяющихся" событий в eventsInRange()/eventsForDate(); JOIN
    // с categories нужно было добавить в обоих местах по отдельности,
    // поэтому это отдельный тест именно на "трудный" путь - повторяющееся
    // событие, а не просто дубль eventById_includesCategoryNameAndColorViaJoin.
    Category category;
    category.name = QStringLiteral("Health");
    category.color = QStringLiteral("#4AD97A");
    const int categoryId = m_manager->addCategory(category);
    QVERIFY(categoryId > 0);

    Event weekly;
    weekly.title = QStringLiteral("Wake up + stretch");
    weekly.date = QDate(2026, 9, 21); // понедельник
    weekly.startTime = QTime(6, 20);
    weekly.endTime = QTime(6, 30);
    weekly.recurrenceType = RecurrenceType::Weekly;
    weekly.categoryId = categoryId;
    m_manager->addEvent(weekly);

    // Диапазон содержит два понедельника, см. eventsInRange_expandsRecurringOccurrencesAcrossRange.
    const QVector<Event> events = m_manager->eventsInRange(QDate(2026, 9, 21), QDate(2026, 10, 4));
    QCOMPARE(events.size(), 2);
    for (const Event &occurrence : events) {
        QCOMPARE(occurrence.categoryId, categoryId);
        QCOMPARE(occurrence.categoryName, QStringLiteral("Health"));
        QCOMPARE(occurrence.categoryColor, QStringLiteral("#4AD97A"));
    }
}

void TestEventManager::executionForOccurrence_returnsFalseWhenNoAction()
{
    Event event;
    event.title = QStringLiteral("Untouched");
    event.date = QDate(2026, 9, 21);
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    const int eventId = m_manager->addEvent(event);
    QVERIFY(eventId > 0);

    EventExecution execution;
    QVERIFY(!m_manager->executionForOccurrence(eventId, event.date, execution));
}

void TestEventManager::startOccurrence_createsRunningExecutionWithActualStart()
{
    Event event;
    event.title = QStringLiteral("Deep work");
    event.date = QDate(2026, 9, 21);
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    const int eventId = m_manager->addEvent(event);
    QVERIFY(eventId > 0);

    const QDateTime startedAt(event.date, QTime(9, 5));
    QVERIFY(m_manager->startOccurrence(eventId, event.date, startedAt));

    EventExecution execution;
    QVERIFY(m_manager->executionForOccurrence(eventId, event.date, execution));
    QCOMPARE(execution.status, ExecutionStatus::Running);
    QCOMPARE(execution.actualStart, startedAt);
    QVERIFY(!execution.actualEnd.isValid());
}

void TestEventManager::completeOccurrence_afterStart_setsCompletedWithActualEnd()
{
    Event event;
    event.title = QStringLiteral("Deep work");
    event.date = QDate(2026, 9, 21);
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    const int eventId = m_manager->addEvent(event);

    const QDateTime startedAt(event.date, QTime(9, 5));
    const QDateTime completedAt(event.date, QTime(9, 50));
    QVERIFY(m_manager->startOccurrence(eventId, event.date, startedAt));
    QVERIFY(m_manager->completeOccurrence(eventId, event.date, completedAt));

    EventExecution execution;
    QVERIFY(m_manager->executionForOccurrence(eventId, event.date, execution));
    QCOMPARE(execution.status, ExecutionStatus::Completed);
    QCOMPARE(execution.actualStart, startedAt);   // сохранённое ранее начало не потерялось
    QCOMPARE(execution.actualEnd, completedAt);

    qint64 durationSeconds = 0;
    QVERIFY(actualDurationSeconds(execution, durationSeconds));
    QCOMPARE(durationSeconds, static_cast<qint64>(45 * 60));
}

void TestEventManager::completeOccurrence_withoutStart_setsCompletedWithoutActualTimes()
{
    // "Manual completion" (раздел 4) - Complete нажали напрямую, без
    // предварительного Start.
    Event event;
    event.title = QStringLiteral("Quick task");
    event.date = QDate(2026, 9, 21);
    event.startTime = QTime(9, 0);
    event.endTime = QTime(9, 15);
    const int eventId = m_manager->addEvent(event);

    QVERIFY(m_manager->completeOccurrence(eventId, event.date, QDateTime(event.date, QTime(9, 10))));

    EventExecution execution;
    QVERIFY(m_manager->executionForOccurrence(eventId, event.date, execution));
    QCOMPARE(execution.status, ExecutionStatus::Completed);
    QVERIFY(!execution.actualStart.isValid());
    QVERIFY(!execution.actualEnd.isValid());
}

void TestEventManager::effectiveStatus_isMissedPastGracePeriodWithNoAction()
{
    Event event;
    event.title = QStringLiteral("Forgotten task");
    event.date = QDate(2026, 9, 21);
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    const int eventId = m_manager->addEvent(event);

    // "Сейчас" - на 2 часа позже planned end (значительно больше grace
    // по умолчанию в 60 минут). effectiveStatus() принимает now параметром,
    // так что тест не зависит от реального времени на машине, где он идёт.
    const QDateTime farInTheFuture(event.date, QTime(12, 0));

    QCOMPARE(
        m_manager->effectiveStatus(eventId, event.date, event.startTime, event.endTime, farInTheFuture),
        ExecutionStatus::Missed
    );
}

void TestEventManager::effectiveStatus_startedTaskDoesNotBecomeMissedPastPlannedEnd()
{
    // То же самое требование раздела 5, что и в test_execution.cpp, но
    // теперь сквозь весь стек EventManager (реальная запись/чтение из
    // SQLite), а не только через чистую функцию resolveExecutionStatus().
    Event event;
    event.title = QStringLiteral("Long meeting");
    event.date = QDate(2026, 9, 21);
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    const int eventId = m_manager->addEvent(event);

    QVERIFY(m_manager->startOccurrence(eventId, event.date, QDateTime(event.date, QTime(9, 5))));

    const QDateTime farInTheFuture(event.date.addDays(1), QTime(9, 0));
    QCOMPARE(
        m_manager->effectiveStatus(eventId, event.date, event.startTime, event.endTime, farInTheFuture),
        ExecutionStatus::Running
    );
}

void TestEventManager::recurringEvent_occurrencesTrackExecutionIndependently()
{
    // Смысл всей таблицы event_executions с ключом (event_id, occurrence_date) -
    // именно в этом: Start одного понедельника не должен затрагивать другой
    // понедельник ТОЙ ЖЕ серии.
    Event weekly;
    weekly.title = QStringLiteral("Weekly standup");
    weekly.date = QDate(2026, 9, 21); // первый понедельник (якорь серии)
    weekly.startTime = QTime(9, 0);
    weekly.endTime = QTime(9, 15);
    weekly.recurrenceType = RecurrenceType::Weekly;
    const int eventId = m_manager->addEvent(weekly);
    QVERIFY(eventId > 0);

    const QDate firstMonday(2026, 9, 21);
    const QDate secondMonday(2026, 9, 28);

    QVERIFY(m_manager->startOccurrence(eventId, firstMonday, QDateTime(firstMonday, QTime(9, 2))));

    // Первый понедельник - Running...
    EventExecution firstExecution;
    QVERIFY(m_manager->executionForOccurrence(eventId, firstMonday, firstExecution));
    QCOMPARE(firstExecution.status, ExecutionStatus::Running);

    // ...а второй, того же самого event_id - как будто ничего не произошло.
    EventExecution secondExecution;
    QVERIFY(!m_manager->executionForOccurrence(eventId, secondMonday, secondExecution));
    QCOMPARE(
        m_manager->effectiveStatus(eventId, secondMonday, weekly.startTime, weekly.endTime,
                                    QDateTime(secondMonday, QTime(9, 5))),
        ExecutionStatus::Planned
    );
}

void TestEventManager::removeEvent_alsoRemovesItsExecutionRecords()
{
    Event event;
    event.title = QStringLiteral("To be deleted");
    event.date = QDate(2026, 9, 21);
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    const int eventId = m_manager->addEvent(event);

    QVERIFY(m_manager->startOccurrence(eventId, event.date, QDateTime(event.date, QTime(9, 5))));

    EventExecution beforeDelete;
    QVERIFY(m_manager->executionForOccurrence(eventId, event.date, beforeDelete)); // убедились, что запись реально есть

    QVERIFY(m_manager->removeEvent(eventId));

    EventExecution afterDelete;
    QVERIFY(!m_manager->executionForOccurrence(eventId, event.date, afterDelete)); // не осталось сиротой
}

void TestEventManager::nowSnapshot_findsCurrentAndNextTask()
{
    Event current;
    current.title = QStringLiteral("Standup");
    current.date = QDate(2026, 9, 21);
    current.startTime = QTime(9, 0);
    current.endTime = QTime(9, 15);
    QVERIFY(m_manager->addEvent(current) > 0);

    Event next;
    next.title = QStringLiteral("Design review");
    next.date = QDate(2026, 9, 21);
    next.startTime = QTime(11, 0);
    next.endTime = QTime(12, 0);
    QVERIFY(m_manager->addEvent(next) > 0);

    // "Сейчас" - внутри окна Standup (9:00-9:15), задолго до Design review.
    const QDateTime now(current.date, QTime(9, 5));
    const NowSnapshot snapshot = m_manager->nowSnapshot(current.date, now);

    QVERIFY(snapshot.hasCurrent);
    QCOMPARE(snapshot.current.event.title, QStringLiteral("Standup"));
    QCOMPARE(snapshot.current.status, ExecutionStatus::Planned);

    QVERIFY(snapshot.hasNext);
    QCOMPARE(snapshot.next.event.title, QStringLiteral("Design review"));

    QCOMPARE(snapshot.allForDate.size(), 2); // сводка дня видит оба события
}

void TestEventManager::nowSnapshot_runningTaskIsCurrentEvenAfterItsWindowPassed()
{
    // То же самое требование раздела 5 (Started-задача не становится
    // Missed после planned end), но теперь ещё и проверяем, что она
    // остаётся "текущей" в nowSnapshot(), а не просто не-Missed.
    Event event;
    event.title = QStringLiteral("Long focus block");
    event.date = QDate(2026, 9, 21);
    event.startTime = QTime(9, 0);
    event.endTime = QTime(9, 30);
    const int eventId = m_manager->addEvent(event);
    QVERIFY(eventId > 0);

    QVERIFY(m_manager->startOccurrence(eventId, event.date, QDateTime(event.date, QTime(9, 5))));

    // "Сейчас" - на два часа позже planned end (окно формально давно кончилось).
    const QDateTime now(event.date, QTime(11, 30));
    const NowSnapshot snapshot = m_manager->nowSnapshot(event.date, now);

    QVERIFY(snapshot.hasCurrent);
    QCOMPARE(snapshot.current.event.id, eventId);
    QCOMPARE(snapshot.current.status, ExecutionStatus::Running);
}

void TestEventManager::pomodoroStartPattern_firstCallStartsOccurrence()
{
    // Воспроизводит MainWindow::startPomodoroForOccurrence(): effectiveStatus()
    // сначала, и только если он НЕ Running - startOccurrence().
    Event event;
    event.title = QStringLiteral("Focus block");
    event.date = QDate(2026, 9, 21);
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    const int eventId = m_manager->addEvent(event);
    QVERIFY(eventId > 0);

    const QDateTime now(event.date, QTime(9, 2));
    const ExecutionStatus statusBefore = m_manager->effectiveStatus(
        eventId, event.date, event.startTime, event.endTime, now);
    QCOMPARE(statusBefore, ExecutionStatus::Planned);

    if (statusBefore != ExecutionStatus::Running)
        QVERIFY(m_manager->startOccurrence(eventId, event.date, now));

    EventExecution execution;
    QVERIFY(m_manager->executionForOccurrence(eventId, event.date, execution));
    QCOMPARE(execution.status, ExecutionStatus::Running);
    QCOMPARE(execution.actualStart, now);
}

void TestEventManager::pomodoroStartPattern_secondCallDoesNotOverwriteActualStart()
{
    Event event;
    event.title = QStringLiteral("Focus block");
    event.date = QDate(2026, 9, 21);
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    const int eventId = m_manager->addEvent(event);

    const QDateTime firstStart(event.date, QTime(9, 2));
    QVERIFY(m_manager->startOccurrence(eventId, event.date, firstStart));

    // Пользователь нажимает "Start Pomodoro" ещё раз (например, после паузы) -
    // та же самая check-then-start логика: effectiveStatus() уже Running,
    // значит startOccurrence() на этот раз НЕ должен вызываться.
    const QDateTime secondAttempt(event.date, QTime(9, 20));
    const ExecutionStatus statusNow = m_manager->effectiveStatus(
        eventId, event.date, event.startTime, event.endTime, secondAttempt);
    QCOMPARE(statusNow, ExecutionStatus::Running);

    if (statusNow != ExecutionStatus::Running)
        m_manager->startOccurrence(eventId, event.date, secondAttempt); // не должно выполниться

    EventExecution execution;
    QVERIFY(m_manager->executionForOccurrence(eventId, event.date, execution));
    QCOMPARE(execution.status, ExecutionStatus::Running);
    QCOMPARE(execution.actualStart, firstStart); // не secondAttempt - не перезаписалось
}

void TestEventManager::nowSnapshot_currentTaskOccurrenceDateMatchesQueriedDay()
{
    // "Математика" сегодня - MainWindow::onStartCurrentTaskClicked() берёт
    // occurrenceDate из snapshot.current.event.date напрямую (не вычисляет
    // отдельно, в отличие от onStartPomodoroClicked()) - этот тест проверяет
    // именно то допущение, на которое он опирается.
    Event weekly;
    weekly.title = QStringLiteral("Math");
    weekly.date = QDate(2026, 9, 21); // понедельник - якорь серии
    weekly.startTime = QTime(13, 0);
    weekly.endTime = QTime(14, 0);
    weekly.recurrenceType = RecurrenceType::Weekly;
    const int eventId = m_manager->addEvent(weekly);
    QVERIFY(eventId > 0);

    const QDate today(2026, 9, 28); // следующий понедельник - НЕ дата якоря
    const QDateTime now(today, QTime(13, 10));
    const NowSnapshot snapshot = m_manager->nowSnapshot(today, now);

    QVERIFY(snapshot.hasCurrent);
    QCOMPARE(snapshot.current.event.id, eventId);
    QCOMPARE(snapshot.current.event.date, today); // НЕ дата якоря (21.09)
}

void TestEventManager::nowSnapshot_differentDaysYieldIndependentExecutionsForRecurringEvent()
{
    // Раздел 9 задания: "Математика" 2026-09-30 и 2026-10-01 должны
    // остаться разными execution-записями, без смешивания.
    Event weekly;
    weekly.title = QStringLiteral("Math");
    weekly.date = QDate(2026, 9, 30); // среда
    weekly.startTime = QTime(13, 0);
    weekly.endTime = QTime(14, 0);
    weekly.recurrenceType = RecurrenceType::Daily; // ежедневно, чтобы оба дня подряд попадали в серию
    const int eventId = m_manager->addEvent(weekly);
    QVERIFY(eventId > 0);

    const QDate day1(2026, 9, 30);
    const QDate day2(2026, 10, 1);

    const NowSnapshot snapshot1 = m_manager->nowSnapshot(day1, QDateTime(day1, QTime(13, 5)));
    QVERIFY(snapshot1.hasCurrent);
    QCOMPARE(snapshot1.current.event.date, day1);
    QVERIFY(m_manager->startOccurrence(eventId, snapshot1.current.event.date, QDateTime(day1, QTime(13, 5))));

    // День 2 - своя собственная, независимая от дня 1 запись.
    const NowSnapshot snapshot2 = m_manager->nowSnapshot(day2, QDateTime(day2, QTime(13, 5)));
    QVERIFY(snapshot2.hasCurrent);
    QCOMPARE(snapshot2.current.event.date, day2);
    QCOMPARE(snapshot2.current.status, ExecutionStatus::Planned); // НЕ Running - день 1 его не затронул

    EventExecution execDay1;
    QVERIFY(m_manager->executionForOccurrence(eventId, day1, execDay1));
    QCOMPARE(execDay1.status, ExecutionStatus::Running);

    EventExecution execDay2;
    QVERIFY(!m_manager->executionForOccurrence(eventId, day2, execDay2)); // по дню 2 действий не было
}

void TestEventManager::nowSnapshot_noCurrentTaskWhenNothingScheduled()
{
    // Пустой день - nowSnapshot().hasCurrent должен быть false, чтобы
    // MainWindow::onStartCurrentTaskClicked() корректно показал
    // "No current task" и не пытался ничего стартовать.
    const QDate emptyDay(2026, 9, 21);
    const NowSnapshot snapshot = m_manager->nowSnapshot(emptyDay, QDateTime(emptyDay, QTime(10, 0)));

    QVERIFY(!snapshot.hasCurrent);
    QVERIFY(snapshot.allForDate.isEmpty());
}

void TestEventManager::recurringOccurrence_missedTodayDoesNotAffectDifferentOccurrence()
{
    // Раздел "Recurring tasks" задания: "Математика" 1 октября пропущена -
    // это не должно менять статус "Математика" 2 октября. Используем один
    // и тот же event_id, два разных occurrenceDate, один и тот же "now".
    Event daily;
    daily.title = QStringLiteral("Math");
    daily.date = QDate(2026, 9, 21);
    daily.startTime = QTime(13, 0);
    daily.endTime = QTime(14, 0);
    daily.recurrenceType = RecurrenceType::Daily;
    const int eventId = m_manager->addEvent(daily);
    QVERIFY(eventId > 0);

    const QDate today(2026, 9, 21);
    const QDate tomorrow(2026, 9, 22);

    // "Сейчас" - вечером today (далеко за gracePeriod для today-вхождения),
    // но задолго ДО планового начала tomorrow-вхождения.
    const QDateTime now(today, QTime(20, 0));

    QCOMPARE(
        m_manager->effectiveStatus(eventId, today, daily.startTime, daily.endTime, now),
        ExecutionStatus::Missed
    );
    QCOMPARE(
        m_manager->effectiveStatus(eventId, tomorrow, daily.startTime, daily.endTime, now),
        ExecutionStatus::Planned // то же "now", но для tomorrow оно ещё задолго до окна - не затронуто
    );
}

void TestEventManager::nowSnapshot_missedCountReflectsMissedEvents()
{
    Event missedOne;
    missedOne.title = QStringLiteral("Missed A");
    missedOne.date = QDate(2026, 9, 21);
    missedOne.startTime = QTime(9, 0);
    missedOne.endTime = QTime(10, 0);
    QVERIFY(m_manager->addEvent(missedOne) > 0);

    Event missedTwo;
    missedTwo.title = QStringLiteral("Missed B");
    missedTwo.date = QDate(2026, 9, 21);
    missedTwo.startTime = QTime(11, 0);
    missedTwo.endTime = QTime(12, 0);
    QVERIFY(m_manager->addEvent(missedTwo) > 0);

    Event stillPlanned;
    stillPlanned.title = QStringLiteral("Not yet");
    stillPlanned.date = QDate(2026, 9, 21);
    stillPlanned.startTime = QTime(16, 0);
    stillPlanned.endTime = QTime(17, 0);
    QVERIFY(m_manager->addEvent(stillPlanned) > 0);

    // "Сейчас" - после grace period для первых двух, но задолго до третьего.
    const QDate day(2026, 9, 21);
    const QDateTime now(day, QTime(14, 0));
    const NowSnapshot snapshot = m_manager->nowSnapshot(day, now);

    QCOMPARE(snapshot.missedCount, 2);
    QCOMPARE(snapshot.allForDate.size(), 3);
}

void TestEventManager::rescheduleOccurrence_oneTimePlanned_updatesDateTimeKeepingSameId()
{
    Event event;
    event.title = QStringLiteral("Dentist");
    event.date = QDate(2026, 9, 21);
    event.startTime = QTime(17, 30);
    event.endTime = QTime(19, 0);
    const int eventId = m_manager->addEvent(event);
    QVERIFY(eventId > 0);

    const QDate newDate(2026, 9, 21);
    const QTime newStart(20, 0);
    const QTime newEnd(21, 0);
    const QDateTime now(event.date, QTime(10, 0)); // задолго до окна - Planned

    QVERIFY(m_manager->rescheduleOccurrence(eventId, event.date, newDate, newStart, newEnd, nullptr, now));

    Event stored;
    QVERIFY(m_manager->eventById(eventId, stored));
    QCOMPARE(stored.id, eventId); // id не изменился - та же строка
    QCOMPARE(stored.date, newDate);
    QCOMPARE(stored.startTime, newStart);
    QCOMPARE(stored.endTime, newEnd);
}

void TestEventManager::rescheduleOccurrence_oneTimeMissed_succeeds()
{
    Event event;
    event.title = QStringLiteral("Forgotten task");
    event.date = QDate(2026, 9, 21);
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    const int eventId = m_manager->addEvent(event);
    QVERIFY(eventId > 0);

    // "Сейчас" - на 2 часа позже plannedEnd, дальше default grace period (60 мин).
    const QDateTime now(event.date, QTime(12, 0));

    QVERIFY(m_manager->rescheduleOccurrence(
        eventId, event.date, QDate(2026, 9, 22), QTime(10, 0), QTime(11, 0), nullptr, now));

    Event stored;
    QVERIFY(m_manager->eventById(eventId, stored));
    QCOMPARE(stored.date, QDate(2026, 9, 22));
}

void TestEventManager::rescheduleOccurrence_oneTimeRunning_rejected()
{
    Event event;
    event.title = QStringLiteral("Deep work");
    event.date = QDate(2026, 9, 21);
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    const int eventId = m_manager->addEvent(event);
    QVERIFY(eventId > 0);

    QVERIFY(m_manager->startOccurrence(eventId, event.date, QDateTime(event.date, QTime(9, 5))));

    const QDateTime now(event.date, QTime(9, 30));
    QVERIFY(!m_manager->rescheduleOccurrence(
        eventId, event.date, QDate(2026, 9, 22), QTime(10, 0), QTime(11, 0), nullptr, now));

    Event stored;
    QVERIFY(m_manager->eventById(eventId, stored));
    QCOMPARE(stored.date, event.date); // ничего не изменилось
    QCOMPARE(stored.startTime, event.startTime);
}

void TestEventManager::rescheduleOccurrence_oneTimeCompleted_rejected()
{
    Event event;
    event.title = QStringLiteral("Quick task");
    event.date = QDate(2026, 9, 21);
    event.startTime = QTime(9, 0);
    event.endTime = QTime(9, 15);
    const int eventId = m_manager->addEvent(event);
    QVERIFY(eventId > 0);

    QVERIFY(m_manager->completeOccurrence(eventId, event.date, QDateTime(event.date, QTime(9, 10))));

    const QDateTime now(event.date, QTime(9, 30));
    QVERIFY(!m_manager->rescheduleOccurrence(
        eventId, event.date, QDate(2026, 9, 22), QTime(10, 0), QTime(11, 0), nullptr, now));

    Event stored;
    QVERIFY(m_manager->eventById(eventId, stored));
    QCOMPARE(stored.date, event.date);
}

void TestEventManager::rescheduleOccurrence_recurringPlanned_createsNewOneTimeEventWithCopiedFields()
{
    Category category;
    category.name = QStringLiteral("Study");
    category.color = QStringLiteral("#4A90D9");
    const int categoryId = m_manager->addCategory(category);
    QVERIFY(categoryId > 0);

    Event weekly;
    weekly.title = QStringLiteral("Math");
    weekly.description = QStringLiteral("Weekly math class");
    weekly.date = QDate(2026, 9, 21); // понедельник - якорь серии
    weekly.startTime = QTime(17, 30);
    weekly.endTime = QTime(19, 0);
    weekly.recurrenceType = RecurrenceType::Weekly;
    weekly.priority = 2;
    weekly.timezone = QStringLiteral("Europe/Moscow");
    weekly.categoryId = categoryId;
    const int templateId = m_manager->addEvent(weekly);
    QVERIFY(templateId > 0);

    const QDate occurrenceDate(2026, 9, 21);
    const QDate newDate(2026, 9, 22);
    const QTime newStart(20, 0);
    const QTime newEnd(21, 30);
    const QDateTime now(occurrenceDate, QTime(10, 0)); // задолго до окна - Planned

    int newEventId = -1;
    QVERIFY(m_manager->rescheduleOccurrence(
        templateId, occurrenceDate, newDate, newStart, newEnd, &newEventId, now));
    QVERIFY(newEventId > 0);
    QVERIFY(newEventId != templateId); // новый, самостоятельный id

    Event newEvent;
    QVERIFY(m_manager->eventById(newEventId, newEvent));
    QCOMPARE(newEvent.title, QStringLiteral("Math"));
    QCOMPARE(newEvent.description, QStringLiteral("Weekly math class"));
    QCOMPARE(newEvent.date, newDate);
    QCOMPARE(newEvent.startTime, newStart);
    QCOMPARE(newEvent.endTime, newEnd);
    QCOMPARE(newEvent.recurrenceType, RecurrenceType::None);
    QCOMPARE(newEvent.priority, 2);
    QCOMPARE(newEvent.timezone, QStringLiteral("Europe/Moscow"));
    QCOMPARE(newEvent.categoryId, categoryId);
    QCOMPARE(newEvent.pomodorosCompleted, 0); // не копия истории серии
}

void TestEventManager::rescheduleOccurrence_recurringPlanned_originalDisappearsOthersRemain()
{
    Event weekly;
    weekly.title = QStringLiteral("Math");
    weekly.date = QDate(2026, 9, 21); // понедельник
    weekly.startTime = QTime(17, 30);
    weekly.endTime = QTime(19, 0);
    weekly.recurrenceType = RecurrenceType::Weekly;
    const int templateId = m_manager->addEvent(weekly);
    QVERIFY(templateId > 0);

    const QDate occurrenceDate(2026, 9, 21);  // переносим этот понедельник
    const QDate otherOccurrence(2026, 9, 28); // следующий понедельник - должен остаться
    const QDate newDate(2026, 9, 22);
    const QDateTime now(occurrenceDate, QTime(10, 0));

    QVERIFY(m_manager->rescheduleOccurrence(
        templateId, occurrenceDate, newDate, QTime(20, 0), QTime(21, 30), nullptr, now));

    // Старая дата - для этого шаблона события больше нет вообще (NOW-related:
    // та же eventsForDate(), на которой строится nowSnapshot()).
    const QVector<Event> onOldDate = m_manager->eventsForDate(occurrenceDate);
    for (const Event &e : onOldDate)
        QVERIFY(e.id != templateId);

    // Новая дата - новое событие появилось.
    const QVector<Event> onNewDate = m_manager->eventsForDate(newDate);
    bool foundOnNewDate = false;
    for (const Event &e : onNewDate) {
        if (e.title == QStringLiteral("Math"))
            foundOnNewDate = true;
    }
    QVERIFY(foundOnNewDate);

    // Следующий понедельник серии - не затронут переносом.
    const QVector<Event> onOtherOccurrence = m_manager->eventsForDate(otherOccurrence);
    bool foundOther = false;
    for (const Event &e : onOtherOccurrence) {
        if (e.id == templateId)
            foundOther = true;
    }
    QVERIFY(foundOther);
}

void TestEventManager::rescheduleOccurrence_recurringTemplateUnchangedAfterReschedule()
{
    Event weekly;
    weekly.title = QStringLiteral("Math");
    weekly.date = QDate(2026, 9, 21);
    weekly.startTime = QTime(17, 30);
    weekly.endTime = QTime(19, 0);
    weekly.recurrenceType = RecurrenceType::Weekly;
    weekly.recurrenceInterval = 1;
    const int templateId = m_manager->addEvent(weekly);
    QVERIFY(templateId > 0);

    const QDateTime now(QDate(2026, 9, 21), QTime(10, 0));
    QVERIFY(m_manager->rescheduleOccurrence(
        templateId, QDate(2026, 9, 21), QDate(2026, 9, 22), QTime(20, 0), QTime(21, 30), nullptr, now));

    Event templateAfter;
    QVERIFY(m_manager->eventById(templateId, templateAfter));
    QCOMPARE(templateAfter.date, QDate(2026, 9, 21));                  // якорь не изменился
    QCOMPARE(templateAfter.startTime, QTime(17, 30));                  // время шаблона не изменилось
    QCOMPARE(templateAfter.endTime, QTime(19, 0));
    QCOMPARE(templateAfter.recurrenceType, RecurrenceType::Weekly);    // правило то же
    QCOMPARE(templateAfter.recurrenceInterval, 1);
}

void TestEventManager::rescheduleOccurrence_recurringMissed_succeeds()
{
    Event weekly;
    weekly.title = QStringLiteral("Math");
    weekly.date = QDate(2026, 9, 21);
    weekly.startTime = QTime(17, 30);
    weekly.endTime = QTime(19, 0);
    weekly.recurrenceType = RecurrenceType::Weekly;
    const int templateId = m_manager->addEvent(weekly);
    QVERIFY(templateId > 0);

    // "Сейчас" - на следующий день, далеко за gracePeriod для 21.09-вхождения.
    const QDateTime now(QDate(2026, 9, 22), QTime(9, 0));

    int newEventId = -1;
    QVERIFY(m_manager->rescheduleOccurrence(
        templateId, QDate(2026, 9, 21), QDate(2026, 9, 23), QTime(10, 0), QTime(11, 0), &newEventId, now));
    QVERIFY(newEventId > 0);
}

void TestEventManager::rescheduleOccurrence_recurringRunning_rejected()
{
    Event weekly;
    weekly.title = QStringLiteral("Math");
    weekly.date = QDate(2026, 9, 21);
    weekly.startTime = QTime(17, 30);
    weekly.endTime = QTime(19, 0);
    weekly.recurrenceType = RecurrenceType::Weekly;
    const int templateId = m_manager->addEvent(weekly);
    QVERIFY(templateId > 0);

    QVERIFY(m_manager->startOccurrence(
        templateId, QDate(2026, 9, 21), QDateTime(QDate(2026, 9, 21), QTime(17, 35))));

    const QDateTime now(QDate(2026, 9, 21), QTime(18, 0));
    int newEventId = -1;
    QVERIFY(!m_manager->rescheduleOccurrence(
        templateId, QDate(2026, 9, 21), QDate(2026, 9, 22), QTime(20, 0), QTime(21, 0), &newEventId, now));
    QCOMPARE(newEventId, -1); // не заполнен - ничего не создано

    // Шаблон не пострадал - другие вхождения серии по-прежнему генерируются.
    QVERIFY(!m_manager->eventsForDate(QDate(2026, 9, 28)).isEmpty());
}

void TestEventManager::rescheduleOccurrence_recurringCompleted_rejected()
{
    Event weekly;
    weekly.title = QStringLiteral("Math");
    weekly.date = QDate(2026, 9, 21);
    weekly.startTime = QTime(17, 30);
    weekly.endTime = QTime(19, 0);
    weekly.recurrenceType = RecurrenceType::Weekly;
    const int templateId = m_manager->addEvent(weekly);
    QVERIFY(templateId > 0);

    QVERIFY(m_manager->completeOccurrence(
        templateId, QDate(2026, 9, 21), QDateTime(QDate(2026, 9, 21), QTime(18, 30))));

    const QDateTime now(QDate(2026, 9, 21), QTime(19, 30));
    QVERIFY(!m_manager->rescheduleOccurrence(
        templateId, QDate(2026, 9, 21), QDate(2026, 9, 22), QTime(20, 0), QTime(21, 0), nullptr, now));
}

void TestEventManager::rescheduleOccurrence_duplicateRescheduleOfSameOccurrenceRejected()
{
    Event weekly;
    weekly.title = QStringLiteral("Math");
    weekly.date = QDate(2026, 9, 21);
    weekly.startTime = QTime(17, 30);
    weekly.endTime = QTime(19, 0);
    weekly.recurrenceType = RecurrenceType::Weekly;
    const int templateId = m_manager->addEvent(weekly);
    QVERIFY(templateId > 0);

    const QDateTime now(QDate(2026, 9, 21), QTime(10, 0));
    QVERIFY(m_manager->rescheduleOccurrence(
        templateId, QDate(2026, 9, 21), QDate(2026, 9, 22), QTime(20, 0), QTime(21, 0), nullptr, now));

    // Повторная попытка перенести ТО ЖЕ вхождение - отклоняется
    // (hasRecurrenceException() для этой пары уже true).
    int secondAttemptId = -1;
    QVERIFY(!m_manager->rescheduleOccurrence(
        templateId, QDate(2026, 9, 21), QDate(2026, 9, 25), QTime(9, 0), QTime(10, 0), &secondAttemptId, now));
    QCOMPARE(secondAttemptId, -1);
}

void TestEventManager::rescheduleOccurrence_exceptionForEventADoesNotAffectEventBSameDate()
{
    Event mathWeekly;
    mathWeekly.title = QStringLiteral("Math");
    mathWeekly.date = QDate(2026, 9, 21);
    mathWeekly.startTime = QTime(17, 30);
    mathWeekly.endTime = QTime(19, 0);
    mathWeekly.recurrenceType = RecurrenceType::Weekly;
    const int mathId = m_manager->addEvent(mathWeekly);
    QVERIFY(mathId > 0);

    Event historyWeekly;
    historyWeekly.title = QStringLiteral("History");
    historyWeekly.date = QDate(2026, 9, 21); // та же дата якоря
    historyWeekly.startTime = QTime(10, 0);
    historyWeekly.endTime = QTime(11, 0);
    historyWeekly.recurrenceType = RecurrenceType::Weekly;
    const int historyId = m_manager->addEvent(historyWeekly);
    QVERIFY(historyId > 0);

    const QDateTime now(QDate(2026, 9, 21), QTime(9, 0));
    QVERIFY(m_manager->rescheduleOccurrence(
        mathId, QDate(2026, 9, 21), QDate(2026, 9, 22), QTime(20, 0), QTime(21, 0), nullptr, now));

    // History на той же исходной дате не затронута переносом Math.
    const QVector<Event> onOriginalDate = m_manager->eventsForDate(QDate(2026, 9, 21));
    bool historyStillThere = false;
    bool mathStillThere = false;
    for (const Event &e : onOriginalDate) {
        if (e.id == historyId) historyStillThere = true;
        if (e.id == mathId) mathStillThere = true;
    }
    QVERIFY(historyStillThere);
    QVERIFY(!mathStillThere);
}

void TestEventManager::rescheduleOccurrence_doesNotCreateExecutionForPlannedOrMissed()
{
    Event event;
    event.title = QStringLiteral("One-off");
    event.date = QDate(2026, 9, 21);
    event.startTime = QTime(9, 0);
    event.endTime = QTime(10, 0);
    const int eventId = m_manager->addEvent(event);
    QVERIFY(eventId > 0);

    const QDateTime now(event.date, QTime(9, 30)); // Planned
    const QDate newDate(2026, 9, 22);
    QVERIFY(m_manager->rescheduleOccurrence(
        eventId, event.date, newDate, QTime(10, 0), QTime(11, 0), nullptr, now));

    EventExecution execOld;
    QVERIFY(!m_manager->executionForOccurrence(eventId, event.date, execOld));
    EventExecution execNew;
    QVERIFY(!m_manager->executionForOccurrence(eventId, newDate, execNew));
}

void TestEventManager::rescheduleOccurrence_newEventCanSubsequentlyStartAndComplete()
{
    Event weekly;
    weekly.title = QStringLiteral("Math");
    weekly.date = QDate(2026, 9, 21);
    weekly.startTime = QTime(17, 30);
    weekly.endTime = QTime(19, 0);
    weekly.recurrenceType = RecurrenceType::Weekly;
    const int templateId = m_manager->addEvent(weekly);
    QVERIFY(templateId > 0);

    const QDateTime now(QDate(2026, 9, 21), QTime(10, 0));
    int newEventId = -1;
    QVERIFY(m_manager->rescheduleOccurrence(
        templateId, QDate(2026, 9, 21), QDate(2026, 9, 22), QTime(20, 0), QTime(21, 0), &newEventId, now));
    QVERIFY(newEventId > 0);

    const QDate newDate(2026, 9, 22);
    const QDateTime startedAt(newDate, QTime(20, 5));
    QVERIFY(m_manager->startOccurrence(newEventId, newDate, startedAt));

    EventExecution running;
    QVERIFY(m_manager->executionForOccurrence(newEventId, newDate, running));
    QCOMPARE(running.status, ExecutionStatus::Running);

    const QDateTime completedAt(newDate, QTime(20, 50));
    QVERIFY(m_manager->completeOccurrence(newEventId, newDate, completedAt));

    EventExecution completed;
    QVERIFY(m_manager->executionForOccurrence(newEventId, newDate, completed));
    QCOMPARE(completed.status, ExecutionStatus::Completed);
    QCOMPARE(completed.actualStart, startedAt);
    QCOMPARE(completed.actualEnd, completedAt);
}

// --- Этап C ---

void TestEventManager::datesWithEvents_excludesRescheduledRecurringOccurrence()
{
    Event weekly;
    weekly.title = QStringLiteral("Weekly");
    weekly.date = QDate(2026, 9, 21); // понедельник - якорь серии
    weekly.startTime = QTime(17, 30);
    weekly.endTime = QTime(19, 0);
    weekly.recurrenceType = RecurrenceType::Weekly;
    const int templateId = m_manager->addEvent(weekly);
    QVERIFY(templateId > 0);

    const QDate rangeStart(2026, 9, 21);
    const QDate rangeEnd(2026, 10, 4);

    // До переноса оба понедельника отмечены.
    QSet<QDate> before = m_manager->datesWithEvents(rangeStart, rangeEnd);
    QVERIFY(before.contains(QDate(2026, 9, 21)));
    QVERIFY(before.contains(QDate(2026, 9, 28)));

    // Переносим только вхождение 21 сентября на вторник 22-го.
    const QDateTime now(QDate(2026, 9, 21), QTime(10, 0)); // задолго до окна - Planned
    QVERIFY(m_manager->rescheduleOccurrence(
        templateId, QDate(2026, 9, 21), QDate(2026, 9, 22), QTime(20, 0), QTime(21, 0), nullptr, now));

    const QSet<QDate> after = m_manager->datesWithEvents(rangeStart, rangeEnd);
    QVERIFY(!after.contains(QDate(2026, 9, 21))); // старый день больше не отмечен
    QVERIFY(after.contains(QDate(2026, 9, 22)));  // новый - отмечен
    QVERIFY(after.contains(QDate(2026, 9, 28)));  // остальная серия на месте
}

void TestEventManager::rescheduleOccurrence_persistsAcrossManagerRecreation()
{
    // Настоящий файл вместо :memory: - иначе "переживает перезапуск"
    // проверить нельзя. Первый менеджер уничтожается (деструктор закрывает
    // соединение) до создания второго - как при перезапуске приложения.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("calendar.db"));

    int templateId = -1;
    int newEventId = -1;
    {
        EventManager first(dbPath, QStringLiteral("test_persist_first"));

        Event weekly;
        weekly.title = QStringLiteral("Math");
        weekly.date = QDate(2026, 9, 21);
        weekly.startTime = QTime(17, 30);
        weekly.endTime = QTime(19, 0);
        weekly.recurrenceType = RecurrenceType::Weekly;
        templateId = first.addEvent(weekly);
        QVERIFY(templateId > 0);

        const QDateTime now(QDate(2026, 9, 21), QTime(10, 0));
        QVERIFY(first.rescheduleOccurrence(
            templateId, QDate(2026, 9, 21), QDate(2026, 9, 22), QTime(20, 0), QTime(21, 0), &newEventId, now));
        QVERIFY(newEventId > 0);
    }

    {
        EventManager second(dbPath, QStringLiteral("test_persist_second"));

        // Старое вхождение по-прежнему исключено из серии...
        QVERIFY(second.eventsForDate(QDate(2026, 9, 21)).isEmpty());

        // ...новое разовое событие на месте...
        const QVector<Event> moved = second.eventsForDate(QDate(2026, 9, 22));
        QCOMPARE(moved.size(), 1);
        QCOMPARE(moved[0].id, newEventId);
        QCOMPARE(moved[0].startTime, QTime(20, 0));
        QCOMPARE(moved[0].endTime, QTime(21, 0));

        // ...а следующее вхождение серии не тронуто.
        const QVector<Event> nextWeek = second.eventsForDate(QDate(2026, 9, 28));
        QCOMPARE(nextWeek.size(), 1);
        QCOMPARE(nextWeek[0].id, templateId);
    }
}

// ---------------------------------------------------------------------------
// Этап D
// ---------------------------------------------------------------------------

namespace {

Event makeOneTimeEvent(const QString &title, const QDate &date, const QTime &start, const QTime &end)
{
    Event event;
    event.title = title;
    event.date = date;
    event.startTime = start;
    event.endTime = end;
    return event;
}

Event makeSeries(const QString &title, const QDate &anchor, RecurrenceType type, int interval = 1)
{
    Event event = makeOneTimeEvent(title, anchor, QTime(9, 0), QTime(10, 0));
    event.recurrenceType = type;
    event.recurrenceInterval = interval;
    return event;
}

} // namespace

void TestEventManager::insertRawExecution(int eventId, const QDate &date, const QString &status)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(
        "INSERT OR REPLACE INTO event_executions (event_id, occurrence_date, status) "
        "VALUES (:event_id, :occurrence_date, :status)");
    query.bindValue(":event_id", eventId);
    query.bindValue(":occurrence_date", date.toString(Qt::ISODate));
    query.bindValue(":status", status);
    QVERIFY(query.exec());
}

int TestEventManager::countRows(const QString &table, int eventId) const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM %1 WHERE event_id = :id").arg(table));
    query.bindValue(":id", eventId);
    if (!query.exec() || !query.next())
        return -1;
    return query.value(0).toInt();
}

// --- D1: переходы ---

void TestEventManager::startOccurrence_fromNoRecordAndMissed_succeeds()
{
    const QDate date(2026, 9, 21);
    const int id = m_manager->addEvent(makeOneTimeEvent("Task", date, QTime(9, 0), QTime(10, 0)));
    QVERIFY(id > 0);

    // 14:00 - окно давно закончилось (grace 60 мин), эффективный статус Missed;
    // записи в БД нет. Start из Missed разрешён.
    const QDateTime startedAt(date, QTime(14, 0));
    QCOMPARE(m_manager->effectiveStatus(id, date, QTime(9, 0), QTime(10, 0), startedAt), ExecutionStatus::Missed);
    QVERIFY(m_manager->startOccurrence(id, date, startedAt));

    EventExecution execution;
    QVERIFY(m_manager->executionForOccurrence(id, date, execution));
    QCOMPARE(execution.status, ExecutionStatus::Running);
    QCOMPARE(execution.actualStart, startedAt);
}

void TestEventManager::startOccurrence_fromRunning_rejectedAndKeepsActualStart()
{
    const QDate date(2026, 9, 21);
    const int id = m_manager->addEvent(makeOneTimeEvent("Task", date, QTime(9, 0), QTime(10, 0)));
    const QDateTime firstStart(date, QTime(9, 5));
    QVERIFY(m_manager->startOccurrence(id, date, firstStart));

    QVERIFY(!m_manager->startOccurrence(id, date, QDateTime(date, QTime(9, 40))));

    EventExecution execution;
    QVERIFY(m_manager->executionForOccurrence(id, date, execution));
    QCOMPARE(execution.status, ExecutionStatus::Running);
    QCOMPARE(execution.actualStart, firstStart); // не сброшен
}

void TestEventManager::startOccurrence_fromCompleted_rejectedAndKeepsRecord()
{
    const QDate date(2026, 9, 21);
    const int id = m_manager->addEvent(makeOneTimeEvent("Task", date, QTime(9, 0), QTime(10, 0)));
    const QDateTime started(date, QTime(9, 5));
    const QDateTime completed(date, QTime(9, 50));
    QVERIFY(m_manager->startOccurrence(id, date, started));
    QVERIFY(m_manager->completeOccurrence(id, date, completed));

    QVERIFY(!m_manager->startOccurrence(id, date, QDateTime(date, QTime(11, 0))));

    EventExecution execution;
    QVERIFY(m_manager->executionForOccurrence(id, date, execution));
    QCOMPARE(execution.status, ExecutionStatus::Completed); // не вернулась в Running
    QCOMPARE(execution.actualStart, started);
    QCOMPARE(execution.actualEnd, completed);
}

void TestEventManager::startOccurrence_fromPostponedAndCancelled_rejected()
{
    const QDate date(2026, 9, 21);
    const int postponedId = m_manager->addEvent(makeOneTimeEvent("P", date, QTime(9, 0), QTime(10, 0)));
    const int cancelledId = m_manager->addEvent(makeOneTimeEvent("C", date, QTime(11, 0), QTime(12, 0)));
    insertRawExecution(postponedId, date, "postponed");
    insertRawExecution(cancelledId, date, "cancelled");

    const QDateTime now(date, QTime(8, 0));
    QVERIFY(!m_manager->startOccurrence(postponedId, date, now));
    QVERIFY(!m_manager->startOccurrence(cancelledId, date, now));

    EventExecution execution;
    QVERIFY(m_manager->executionForOccurrence(postponedId, date, execution));
    QCOMPARE(execution.status, ExecutionStatus::Postponed);
    QVERIFY(m_manager->executionForOccurrence(cancelledId, date, execution));
    QCOMPARE(execution.status, ExecutionStatus::Cancelled);
}

void TestEventManager::startOccurrence_rejectionAffectsOnlyThatOccurrenceOfSeries()
{
    const QDate monday(2026, 9, 21);
    const QDate nextMonday(2026, 9, 28);
    const int id = m_manager->addEvent(makeSeries("Weekly", monday, RecurrenceType::Weekly));
    QVERIFY(m_manager->completeOccurrence(id, monday, QDateTime(monday, QTime(10, 0))));

    QVERIFY(!m_manager->startOccurrence(id, monday, QDateTime(monday, QTime(11, 0))));
    QVERIFY(m_manager->startOccurrence(id, nextMonday, QDateTime(nextMonday, QTime(9, 0))));
}

void TestEventManager::completeOccurrence_fromNoRecordAndRunning_succeeds()
{
    const QDate date(2026, 9, 21);

    // Без записи ("Manual completion"): actualStart/actualEnd не заполняются.
    const int manualId = m_manager->addEvent(makeOneTimeEvent("Manual", date, QTime(9, 0), QTime(10, 0)));
    QVERIFY(m_manager->completeOccurrence(manualId, date, QDateTime(date, QTime(10, 0))));
    EventExecution execution;
    QVERIFY(m_manager->executionForOccurrence(manualId, date, execution));
    QCOMPARE(execution.status, ExecutionStatus::Completed);
    QVERIFY(!execution.actualStart.isValid());

    // Из Running ("Track actual time"): фиксируется actualEnd.
    const int trackedId = m_manager->addEvent(makeOneTimeEvent("Tracked", date, QTime(11, 0), QTime(12, 0)));
    const QDateTime end(date, QTime(11, 45));
    QVERIFY(m_manager->startOccurrence(trackedId, date, QDateTime(date, QTime(11, 5))));
    QVERIFY(m_manager->completeOccurrence(trackedId, date, end));
    QVERIFY(m_manager->executionForOccurrence(trackedId, date, execution));
    QCOMPARE(execution.status, ExecutionStatus::Completed);
    QCOMPARE(execution.actualEnd, end);
}

void TestEventManager::completeOccurrence_fromCompleted_rejectedAndKeepsActualEnd()
{
    const QDate date(2026, 9, 21);
    const int id = m_manager->addEvent(makeOneTimeEvent("Task", date, QTime(9, 0), QTime(10, 0)));
    const QDateTime end(date, QTime(9, 50));
    QVERIFY(m_manager->startOccurrence(id, date, QDateTime(date, QTime(9, 5))));
    QVERIFY(m_manager->completeOccurrence(id, date, end));

    QVERIFY(!m_manager->completeOccurrence(id, date, QDateTime(date, QTime(15, 0))));

    EventExecution execution;
    QVERIFY(m_manager->executionForOccurrence(id, date, execution));
    QCOMPARE(execution.status, ExecutionStatus::Completed);
    QCOMPARE(execution.actualEnd, end); // не сдвинут
}

void TestEventManager::completeOccurrence_fromPostponedAndCancelled_rejected()
{
    const QDate date(2026, 9, 21);
    const int postponedId = m_manager->addEvent(makeOneTimeEvent("P", date, QTime(9, 0), QTime(10, 0)));
    const int cancelledId = m_manager->addEvent(makeOneTimeEvent("C", date, QTime(11, 0), QTime(12, 0)));
    insertRawExecution(postponedId, date, "postponed");
    insertRawExecution(cancelledId, date, "cancelled");

    const QDateTime now(date, QTime(13, 0));
    QVERIFY(!m_manager->completeOccurrence(postponedId, date, now));
    QVERIFY(!m_manager->completeOccurrence(cancelledId, date, now));

    EventExecution execution;
    QVERIFY(m_manager->executionForOccurrence(postponedId, date, execution));
    QCOMPARE(execution.status, ExecutionStatus::Postponed);
    QVERIFY(m_manager->executionForOccurrence(cancelledId, date, execution));
    QCOMPARE(execution.status, ExecutionStatus::Cancelled);
}

// --- D1: редактирование ---

void TestEventManager::updateEvent_oneTimeWithHistory_rejectsDateAndStartChange()
{
    const QDate date(2026, 9, 21);

    for (int pass = 0; pass < 2; ++pass) {
        const bool completed = (pass == 0);
        const int id = m_manager->addEvent(
            makeOneTimeEvent(completed ? "Done" : "Doing", date, QTime(9, 0), QTime(10, 0)));
        QVERIFY(id > 0);
        QVERIFY(m_manager->startOccurrence(id, date, QDateTime(date, QTime(9, 0))));
        if (completed)
            QVERIFY(m_manager->completeOccurrence(id, date, QDateTime(date, QTime(9, 45))));

        Event stored;
        QVERIFY(m_manager->eventById(id, stored));

        Event moved = stored;
        moved.date = date.addDays(1);
        QVERIFY(!m_manager->updateEvent(moved));

        Event retimed = stored;
        retimed.startTime = QTime(9, 30);
        QVERIFY(!m_manager->updateEvent(retimed));

        Event after;
        QVERIFY(m_manager->eventById(id, after));
        QCOMPARE(after.date, date);
        QCOMPARE(after.startTime, QTime(9, 0));

        EventExecution execution;
        QVERIFY(m_manager->executionForOccurrence(id, date, execution));
        QCOMPARE(execution.status, completed ? ExecutionStatus::Completed : ExecutionStatus::Running);
    }
}

void TestEventManager::updateEvent_oneTimeCompleted_allowsEndTitleAndDescriptionChange()
{
    const QDate date(2026, 9, 21);
    const int id = m_manager->addEvent(makeOneTimeEvent("Done", date, QTime(9, 0), QTime(10, 0)));
    QVERIFY(m_manager->completeOccurrence(id, date, QDateTime(date, QTime(10, 0))));

    Event edited;
    QVERIFY(m_manager->eventById(id, edited));
    edited.title = QStringLiteral("Renamed");
    edited.description = QStringLiteral("Notes");
    edited.endTime = QTime(10, 30); // конец можно менять (начало и дата - нет)
    QVERIFY(m_manager->updateEvent(edited));

    Event after;
    QVERIFY(m_manager->eventById(id, after));
    QCOMPARE(after.title, QStringLiteral("Renamed"));
    QCOMPARE(after.description, QStringLiteral("Notes"));
    QCOMPARE(after.endTime, QTime(10, 30));

    EventExecution execution;
    QVERIFY(m_manager->executionForOccurrence(id, date, execution));
    QCOMPARE(execution.status, ExecutionStatus::Completed); // история на месте
}

void TestEventManager::updateEvent_oneTimePlanned_allowsAnyChange()
{
    const QDate date(2026, 9, 21);
    const int id = m_manager->addEvent(makeOneTimeEvent("Plan", date, QTime(9, 0), QTime(10, 0)));

    Event edited;
    QVERIFY(m_manager->eventById(id, edited));
    edited.date = date.addDays(3);
    edited.startTime = QTime(14, 0);
    edited.endTime = QTime(15, 0);
    QVERIFY(m_manager->updateEvent(edited));

    Event after;
    QVERIFY(m_manager->eventById(id, after));
    QCOMPARE(after.date, date.addDays(3));
    QCOMPARE(after.startTime, QTime(14, 0));
}

void TestEventManager::updateEvent_seriesWithCompletedOccurrence_allowsTitleAndTimeChange()
{
    const QDate monday(2026, 9, 21);
    const int id = m_manager->addEvent(makeSeries("Weekly", monday, RecurrenceType::Weekly));
    QVERIFY(m_manager->completeOccurrence(id, monday, QDateTime(monday, QTime(10, 0))));

    // Один выполненный день серию не замораживает.
    Event edited;
    QVERIFY(m_manager->eventById(id, edited));
    edited.title = QStringLiteral("Weekly renamed");
    edited.startTime = QTime(18, 0);
    edited.endTime = QTime(19, 30);
    QVERIFY(m_manager->updateEvent(edited));

    Event after;
    QVERIFY(m_manager->eventById(id, after));
    QCOMPARE(after.title, QStringLiteral("Weekly renamed"));
    QCOMPARE(after.startTime, QTime(18, 0));

    EventExecution execution;
    QVERIFY(m_manager->executionForOccurrence(id, monday, execution));
    QCOMPARE(execution.status, ExecutionStatus::Completed);
}

void TestEventManager::updateEvent_seriesWithCompletedOccurrence_anchorShift()
{
    const QDate monday(2026, 9, 21);
    const int id = m_manager->addEvent(makeSeries("Weekly", monday, RecurrenceType::Weekly));
    QVERIFY(m_manager->completeOccurrence(id, monday, QDateTime(monday, QTime(10, 0))));

    Event stored;
    QVERIFY(m_manager->eventById(id, stored));

    // Тот же день недели, раньше - 21.09 остаётся вхождением.
    Event earlier = stored;
    earlier.date = monday.addDays(-7);
    QVERIFY(m_manager->updateEvent(earlier));

    // Другой день недели - 21.09 перестало бы быть вхождением.
    Event otherWeekday = earlier;
    otherWeekday.date = monday.addDays(1); // вторник
    QVERIFY(!m_manager->updateEvent(otherWeekday));

    // Якорь позже выполненной даты - она оказалась бы до начала серии.
    Event later = earlier;
    later.date = monday.addDays(7);
    QVERIFY(!m_manager->updateEvent(later));

    Event after;
    QVERIFY(m_manager->eventById(id, after));
    QCOMPARE(after.date, monday.addDays(-7)); // остался результат первой, допустимой правки
}

void TestEventManager::updateEvent_seriesWithCompletedOccurrence_intervalChange()
{
    const QDate anchor(2026, 9, 21);

    // История на 22.09: Daily -> Custom(2) оставляет 21, 23, 25... - 22.09 выпало бы.
    const int droppedId = m_manager->addEvent(makeSeries("Daily A", anchor, RecurrenceType::Daily));
    QVERIFY(m_manager->completeOccurrence(droppedId, anchor.addDays(1), QDateTime(anchor.addDays(1), QTime(10, 0))));
    Event dropped;
    QVERIFY(m_manager->eventById(droppedId, dropped));
    dropped.recurrenceType = RecurrenceType::Custom;
    dropped.recurrenceInterval = 2;
    QVERIFY(!m_manager->updateEvent(dropped));

    // История на 23.09: та же правка оставляет её вхождением - разрешено.
    const int keptId = m_manager->addEvent(makeSeries("Daily B", anchor, RecurrenceType::Daily));
    QVERIFY(m_manager->completeOccurrence(keptId, anchor.addDays(2), QDateTime(anchor.addDays(2), QTime(10, 0))));
    Event kept;
    QVERIFY(m_manager->eventById(keptId, kept));
    kept.recurrenceType = RecurrenceType::Custom;
    kept.recurrenceInterval = 2;
    QVERIFY(m_manager->updateEvent(kept));
}

void TestEventManager::updateEvent_seriesWithCompletedOccurrence_recurrenceEndDate()
{
    const QDate monday(2026, 9, 21);
    const int id = m_manager->addEvent(makeSeries("Weekly", monday, RecurrenceType::Weekly));
    QVERIFY(m_manager->completeOccurrence(id, monday, QDateTime(monday, QTime(10, 0))));

    Event stored;
    QVERIFY(m_manager->eventById(id, stored));

    Event tooEarly = stored;
    tooEarly.recurrenceEndDate = QDate(2026, 9, 20); // до выполненной даты
    QVERIFY(!m_manager->updateEvent(tooEarly));

    Event ok = stored;
    ok.recurrenceEndDate = QDate(2026, 9, 28); // выполненная дата остаётся внутри
    QVERIFY(m_manager->updateEvent(ok));
}

void TestEventManager::updateEvent_seriesToOneTime_blockedOnlyWhenOtherDatesHaveHistory()
{
    const QDate monday(2026, 9, 21);
    const QDate nextMonday(2026, 9, 28);

    // История на двух датах: превращение в одноразовое на 21.09 осиротило бы 28.09.
    const int twoId = m_manager->addEvent(makeSeries("Two", monday, RecurrenceType::Weekly));
    QVERIFY(m_manager->completeOccurrence(twoId, monday, QDateTime(monday, QTime(10, 0))));
    QVERIFY(m_manager->completeOccurrence(twoId, nextMonday, QDateTime(nextMonday, QTime(10, 0))));
    Event two;
    QVERIFY(m_manager->eventById(twoId, two));
    two.recurrenceType = RecurrenceType::None;
    QVERIFY(!m_manager->updateEvent(two));

    // История только на якорной дате - превращение допустимо.
    const int oneId = m_manager->addEvent(makeSeries("One", monday, RecurrenceType::Weekly));
    QVERIFY(m_manager->completeOccurrence(oneId, monday, QDateTime(monday, QTime(10, 0))));
    Event one;
    QVERIFY(m_manager->eventById(oneId, one));
    one.recurrenceType = RecurrenceType::None;
    QVERIFY(m_manager->updateEvent(one));

    Event after;
    QVERIFY(m_manager->eventById(oneId, after));
    QCOMPARE(after.recurrenceType, RecurrenceType::None);
}

void TestEventManager::updateEvent_preExistingOrphanHistoryDoesNotBlockEdits()
{
    const QDate monday(2026, 9, 21);
    const int id = m_manager->addEvent(makeSeries("Weekly", monday, RecurrenceType::Weekly));

    // Старая "сирота": completed на вторнике, который и так не вхождение серии.
    insertRawExecution(id, monday.addDays(1), "completed");

    Event edited;
    QVERIFY(m_manager->eventById(id, edited));
    edited.title = QStringLiteral("Still editable");
    QVERIFY(m_manager->updateEvent(edited));

    edited.date = monday.addDays(-7); // и якорь тоже можно двигать
    QVERIFY(m_manager->updateEvent(edited));
}

void TestEventManager::updateEvent_rejectedEditChangesNothing()
{
    const QDate monday(2026, 9, 21);
    const int id = m_manager->addEvent(makeSeries("Original", monday, RecurrenceType::Weekly));
    QVERIFY(m_manager->completeOccurrence(id, monday, QDateTime(monday, QTime(10, 0))));

    Event edited;
    QVERIFY(m_manager->eventById(id, edited));
    edited.title = QStringLiteral("Should not be saved"); // безобидное поле в той же правке
    edited.date = monday.addDays(1);                      // ломающее поле
    QVERIFY(!m_manager->updateEvent(edited));

    Event after;
    QVERIFY(m_manager->eventById(id, after));
    QCOMPARE(after.title, QStringLiteral("Original")); // правка отклонена целиком
    QCOMPARE(after.date, monday);

    EventExecution execution;
    QVERIFY(m_manager->executionForOccurrence(id, monday, execution));
    QCOMPARE(execution.status, ExecutionStatus::Completed);
}

void TestEventManager::executionConflictsForEdit_reportsConflictingDates()
{
    const QDate date(2026, 9, 21);
    const int id = m_manager->addEvent(makeOneTimeEvent("Done", date, QTime(9, 0), QTime(10, 0)));
    QVERIFY(m_manager->completeOccurrence(id, date, QDateTime(date, QTime(10, 0))));

    Event stored;
    QVERIFY(m_manager->eventById(id, stored));

    QVERIFY(m_manager->executionConflictsForEdit(stored).isEmpty()); // без изменений

    Event moved = stored;
    moved.date = date.addDays(1);
    const QVector<QDate> conflicts = m_manager->executionConflictsForEdit(moved);
    QCOMPARE(conflicts.size(), 1);
    QCOMPARE(conflicts[0], date);

    Event endOnly = stored;
    endOnly.endTime = QTime(11, 0);
    QVERIFY(m_manager->executionConflictsForEdit(endOnly).isEmpty());

    Event missing = stored;
    missing.id = 999999; // несуществующее событие
    QVERIFY(m_manager->executionConflictsForEdit(missing).isEmpty());

    // Метод ничего не пишет - событие осталось прежним.
    Event after;
    QVERIFY(m_manager->eventById(id, after));
    QCOMPARE(after.date, date);
}

// --- D2: NOW ---

void TestEventManager::nowSnapshot_backToBackTasks_switchAtBoundary()
{
    const QDate date(2026, 9, 21);
    const int firstId = m_manager->addEvent(makeOneTimeEvent("First", date, QTime(9, 0), QTime(10, 0)));
    const int secondId = m_manager->addEvent(makeOneTimeEvent("Second", date, QTime(10, 0), QTime(11, 0)));
    QVERIFY(firstId > 0 && secondId > 0);

    // За секунду до стыка: текущая - первая, следующая - вторая.
    NowSnapshot before = m_manager->nowSnapshot(date, QDateTime(date, QTime(9, 59, 59)));
    QVERIFY(before.hasCurrent);
    QCOMPARE(before.current.event.id, firstId);
    QVERIFY(before.hasNext);
    QCOMPARE(before.next.event.id, secondId);

    // Ровно в момент стыка: текущая уже вторая; следующей нет.
    NowSnapshot atBoundary = m_manager->nowSnapshot(date, QDateTime(date, QTime(10, 0, 0)));
    QVERIFY(atBoundary.hasCurrent);
    QCOMPARE(atBoundary.current.event.id, secondId);
    QVERIFY(!atBoundary.hasNext);

    // Сразу после стыка - то же.
    NowSnapshot after = m_manager->nowSnapshot(date, QDateTime(date, QTime(10, 0, 1)));
    QVERIFY(after.hasCurrent);
    QCOMPARE(after.current.event.id, secondId);
}

void TestEventManager::nowSnapshot_rescheduledIntoToday_becomesCurrentThenNext()
{
    const QDate today(2026, 9, 24);

    // 1) Одноразовое, перенесённое на сегодня с другой даты (тот же id).
    const QDate oldDate(2026, 9, 20);
    const int oneTimeId = m_manager->addEvent(makeOneTimeEvent("Moved one-time", oldDate, QTime(9, 0), QTime(10, 0)));
    QVERIFY(m_manager->rescheduleOccurrence(
        oneTimeId, oldDate, today, QTime(14, 0), QTime(15, 0), nullptr, QDateTime(today, QTime(8, 0))));

    NowSnapshot atWindow = m_manager->nowSnapshot(today, QDateTime(today, QTime(14, 30)));
    QVERIFY(atWindow.hasCurrent);
    QCOMPARE(atWindow.current.event.id, oneTimeId);
    QCOMPARE(atWindow.current.event.date, today);

    NowSnapshot beforeWindow = m_manager->nowSnapshot(today, QDateTime(today, QTime(13, 0)));
    QVERIFY(!beforeWindow.hasCurrent);
    QVERIFY(beforeWindow.hasNext);
    QCOMPARE(beforeWindow.next.event.id, oneTimeId);

    // Со старой даты событие исчезло.
    NowSnapshot oldDay = m_manager->nowSnapshot(oldDate, QDateTime(oldDate, QTime(9, 30)));
    QVERIFY(!oldDay.hasCurrent);
    QCOMPARE(oldDay.allForDate.size(), 0);

    // 2) Вхождение серии, перенесённое на сегодня: появляется как новое
    // одноразовое событие с другим id.
    const QDate monday(2026, 9, 21);
    const QDate nextMonday(2026, 9, 28);
    const int seriesId = m_manager->addEvent(makeSeries("Series", monday, RecurrenceType::Weekly));
    int movedId = -1;
    QVERIFY(m_manager->rescheduleOccurrence(
        seriesId, nextMonday, today, QTime(16, 0), QTime(17, 0), &movedId, QDateTime(today, QTime(8, 0))));
    QVERIFY(movedId > 0 && movedId != seriesId);

    NowSnapshot seriesMoved = m_manager->nowSnapshot(today, QDateTime(today, QTime(16, 30)));
    QVERIFY(seriesMoved.hasCurrent);
    QCOMPARE(seriesMoved.current.event.id, movedId);
    QCOMPARE(seriesMoved.current.event.date, today);
}

// --- D4: удаление ---

void TestEventManager::removeEvent_oneTimeDeletesEventAndExecutions()
{
    const QDate date(2026, 9, 21);
    const int id = m_manager->addEvent(makeOneTimeEvent("Task", date, QTime(9, 0), QTime(10, 0)));
    QVERIFY(m_manager->startOccurrence(id, date, QDateTime(date, QTime(9, 0))));
    QVERIFY(m_manager->completeOccurrence(id, date, QDateTime(date, QTime(9, 50))));
    QCOMPARE(countRows("event_executions", id), 1);

    QVERIFY(m_manager->removeEvent(id));

    Event gone;
    QVERIFY(!m_manager->eventById(id, gone));
    QCOMPARE(countRows("event_executions", id), 0);
}

void TestEventManager::removeEvent_seriesClearsExceptionsAndExecutionsKeepsMovedCopy()
{
    const QDate monday(2026, 9, 21);
    const QDate nextMonday(2026, 9, 28);
    const int seriesId = m_manager->addEvent(makeSeries("Weekly", monday, RecurrenceType::Weekly));
    QVERIFY(m_manager->completeOccurrence(seriesId, monday, QDateTime(monday, QTime(10, 0))));

    int movedId = -1;
    QVERIFY(m_manager->rescheduleOccurrence(
        seriesId, nextMonday, QDate(2026, 9, 29), QTime(12, 0), QTime(13, 0), &movedId,
        QDateTime(monday, QTime(8, 0))));
    QVERIFY(movedId > 0);

    QCOMPARE(countRows("event_executions", seriesId), 1);
    QCOMPARE(countRows("recurrence_exceptions", seriesId), 1);

    QVERIFY(m_manager->removeEvent(seriesId));

    Event gone;
    QVERIFY(!m_manager->eventById(seriesId, gone));
    QCOMPARE(countRows("event_executions", seriesId), 0);
    QCOMPARE(countRows("recurrence_exceptions", seriesId), 0);

    // Перенесённая копия - независимое одноразовое событие, удаление серии её не трогает.
    Event moved;
    QVERIFY(m_manager->eventById(movedId, moved));
}

void TestEventManager::removeEvent_missingIdReturnsFalse()
{
    QVERIFY(!m_manager->removeEvent(987654));
}

void TestEventManager::removeEvent_failureRollsBackEverything()
{
    const QDate date(2026, 9, 21);
    const int id = m_manager->addEvent(makeOneTimeEvent("Task", date, QTime(9, 0), QTime(10, 0)));
    QVERIFY(m_manager->completeOccurrence(id, date, QDateTime(date, QTime(10, 0))));
    QCOMPARE(countRows("event_executions", id), 1);

    // Минимальная имитация сбоя без новой инфраструктуры: удаляем таблицу
    // исключений, и второй DELETE внутри removeEvent() падает. Первый DELETE
    // (execution-записи) к этому моменту уже выполнен - при откате он должен
    // быть отменён.
    {
        QSqlQuery drop(QSqlDatabase::database(m_connectionName));
        QVERIFY(drop.exec(QStringLiteral("DROP TABLE recurrence_exceptions")));
    }

    QTest::ignoreMessage(QtWarningMsg, QRegularExpression("Failed to clear recurrence exceptions before delete"));
    QVERIFY(!m_manager->removeEvent(id));

    Event stillThere;
    QVERIFY(m_manager->eventById(id, stillThere));        // событие на месте
    QCOMPARE(countRows("event_executions", id), 1);       // и execution-запись тоже - откат сработал
}

QTEST_MAIN(TestEventManager)
#include "test_event_manager.moc"
