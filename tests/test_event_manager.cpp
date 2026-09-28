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

private:
    EventManager *m_manager = nullptr;
};

void TestEventManager::init()
{
    // Уникальное имя подключения на каждый тест - иначе повторное
    // addDatabase() с тем же именем в рамках одного процесса будет либо
    // переиспользовать старое соединение, либо ругаться в консоль.
    static int counter = 0;
    const QString connectionName = QStringLiteral("test_event_manager_%1").arg(++counter);
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

QTEST_MAIN(TestEventManager)
#include "test_event_manager.moc"
