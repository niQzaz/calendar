#include "EventManager.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <algorithm>

namespace {

// Все запросы этого класса используют одно и то же именованное
// соединение, чтобы не плодить лишние подключения к одному и тому же
// файлу базы (у Qt SQL соединения различаются по имени, а не по объекту).
const char *kConnectionName = "calendar_connection";

// SQLite не имеет типа enum - храним recurrence_type как текст.
QString recurrenceTypeToString(RecurrenceType type)
{
    switch (type) {
    case RecurrenceType::None: return QStringLiteral("none");
    case RecurrenceType::Daily: return QStringLiteral("daily");
    case RecurrenceType::Weekdays: return QStringLiteral("weekdays");
    case RecurrenceType::Weekly: return QStringLiteral("weekly");
    case RecurrenceType::Custom: return QStringLiteral("custom");
    }
    return QStringLiteral("none");
}

RecurrenceType recurrenceTypeFromString(const QString &value)
{
    if (value == QStringLiteral("daily")) return RecurrenceType::Daily;
    if (value == QStringLiteral("weekdays")) return RecurrenceType::Weekdays;
    if (value == QStringLiteral("weekly")) return RecurrenceType::Weekly;
    if (value == QStringLiteral("custom")) return RecurrenceType::Custom;
    return RecurrenceType::None;
}

// Путь к файлу БД для обычного (не тестового) использования - вынесен
// в отдельную функцию, чтобы конструктор по умолчанию мог просто передать
// его в EventManager(databasePath, connectionName), не дублируя логику.
QString defaultDatabaseFilePath()
{
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir); // на случай первого запуска - папки ещё нет
    return dataDir + "/calendar.db";
}

// QString() (default-constructed, "null"-строка) при bindValue() уходит
// в Qt SQL как SQL NULL, а не как пустая строка - причём NULL игнорирует
// "DEFAULT ''" колонки: DEFAULT применяется только когда колонка вообще
// не упомянута в запросе, а не когда в неё явно передан NULL. Колонка
// timezone объявлена NOT NULL, а поле Event::timezone не имеет явного
// инициализатора (значит остаётся null-строкой везде, где вызывающий код
// его не трогает - например, EventDialog и "простой" формат CSV никогда
// его не заполняют). Без этой нормализации addEvent()/updateEvent() для
// такого Event падали с "NOT NULL constraint failed: events.timezone".
QString nonNullTimezone(const QString &timezone)
{
    return timezone.isNull() ? QString(QLatin1String("")) : timezone;
}

// Собирает Event из текущей строки результата запроса.
// Работает и для SELECT *, и для SELECT с явным списком колонок -
// главное, чтобы в результате были колонки с этими именами.
Event eventFromQuery(const QSqlQuery &query)
{
    Event event;
    event.id = query.value("id").toInt();
    event.title = query.value("title").toString();
    event.date = QDate::fromString(query.value("date").toString(), Qt::ISODate);
    event.startTime = QTime::fromString(query.value("start_time").toString(), Qt::ISODate);
    event.endTime = QTime::fromString(query.value("end_time").toString(), Qt::ISODate);
    event.description = query.value("description").toString();
    event.pomodorosCompleted = query.value("pomodoros_completed").toInt();

    event.recurrenceType = recurrenceTypeFromString(query.value("recurrence_type").toString());
    event.recurrenceInterval = query.value("recurrence_interval").toInt();
    const QString endDateStr = query.value("recurrence_end_date").toString();
    if (!endDateStr.isEmpty())
        event.recurrenceEndDate = QDate::fromString(endDateStr, Qt::ISODate);

    event.priority = query.value("priority").toInt();
    event.timezone = query.value("timezone").toString();

    // category_id - настоящая колонка events, может быть SQL NULL ("без
    // категории"); category_name/category_color приходят только если
    // запрос делал LEFT JOIN с categories (см. eventsSelectWithCategoryJoin()
    // ниже) - если запрос JOIN не делал, оба поля просто останутся "".
    const QVariant categoryIdValue = query.value("category_id");
    event.categoryId = categoryIdValue.isNull() ? -1 : categoryIdValue.toInt();
    event.categoryName = query.value("category_name").toString();
    event.categoryColor = query.value("category_color").toString();

    return event;
}

// SQL-запрос "SELECT ... FROM events" с LEFT JOIN на categories - нужен
// везде, где потом вызывается eventFromQuery() и важен цвет/имя категории
// (без JOIN category_name/category_color в eventFromQuery() просто
// останутся пустыми - событие отрисуется без цвета категории, не ошибка).
// К возвращаемой строке в местах использования дописывается WHERE/... .
QString eventsSelectWithCategoryJoin()
{
    return QStringLiteral(
        "SELECT events.*, categories.name AS category_name, categories.color AS category_color "
        "FROM events LEFT JOIN categories ON events.category_id = categories.id"
    );
}

} // namespace

EventManager::EventManager()
    : EventManager(defaultDatabaseFilePath(), QString::fromLatin1(kConnectionName))
{
}

EventManager::EventManager(const QString &databasePath, const QString &connectionName)
    : m_connectionName(connectionName)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    db.setDatabaseName(databasePath);

    if (!db.open()) {
        qWarning() << "Failed to open database:" << db.lastError().text();
        return;
    }

    QSqlQuery query(db);
    const bool ok = query.exec(
        "CREATE TABLE IF NOT EXISTS events ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  title TEXT NOT NULL,"
        "  date TEXT NOT NULL,"
        "  start_time TEXT NOT NULL,"
        "  end_time TEXT NOT NULL,"
        "  description TEXT,"
        "  pomodoros_completed INTEGER NOT NULL DEFAULT 0,"
        "  recurrence_type TEXT NOT NULL DEFAULT 'none',"
        "  recurrence_interval INTEGER NOT NULL DEFAULT 1,"
        "  recurrence_end_date TEXT,"
        "  priority INTEGER NOT NULL DEFAULT 0,"
        "  timezone TEXT NOT NULL DEFAULT '',"
        "  category_id INTEGER"
        ")"
    );

    if (!ok)
        qWarning() << "Failed to create events table:" << query.lastError().text();

    // Отдельная таблица категорий (MVP2) - без FK-констрейнта, т.к. нигде
    // в этой схеме FK и так не используются; ссылочная целостность
    // поддерживается вручную в removeCategory().
    if (!query.exec(
        "CREATE TABLE IF NOT EXISTS categories ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name TEXT NOT NULL,"
        "  color TEXT NOT NULL"
        ")"
    )) {
        qWarning() << "Failed to create categories table:" << query.lastError().text();
    }

    // Execution (MVP 3.0) - по одной строке максимум на (event_id,
    // occurrence_date), см. models/Execution.h за подробным объяснением,
    // почему это отдельная таблица, а не колонки в events. Без FK -
    // та же причина, что и у categories; ссылочная целостность
    // поддерживается вручную в removeEvent() (см. ниже).
    if (!query.exec(
        "CREATE TABLE IF NOT EXISTS event_executions ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  event_id INTEGER NOT NULL,"
        "  occurrence_date TEXT NOT NULL,"
        "  status TEXT NOT NULL DEFAULT 'planned',"
        "  actual_start TEXT,"
        "  actual_end TEXT,"
        "  UNIQUE(event_id, occurrence_date)"
        ")"
    )) {
        qWarning() << "Failed to create event_executions table:" << query.lastError().text();
    }

    // Миграция для баз, созданных в более ранних этапах: собираем список
    // уже существующих колонок и добавляем те, которых не хватает.
    // Для новой (только что созданной) базы этот цикл ничего не делает -
    // все колонки уже есть из CREATE TABLE выше.
    query.exec("PRAGMA table_info(events)");
    QSet<QString> existingColumns;
    while (query.next())
        existingColumns.insert(query.value("name").toString());

    struct ColumnMigration { QString name; QString alterSql; };
    const QVector<ColumnMigration> migrations = {
        {"pomodoros_completed", "ALTER TABLE events ADD COLUMN pomodoros_completed INTEGER NOT NULL DEFAULT 0"},
        {"recurrence_type", "ALTER TABLE events ADD COLUMN recurrence_type TEXT NOT NULL DEFAULT 'none'"},
        {"recurrence_interval", "ALTER TABLE events ADD COLUMN recurrence_interval INTEGER NOT NULL DEFAULT 1"},
        {"recurrence_end_date", "ALTER TABLE events ADD COLUMN recurrence_end_date TEXT"},
        {"priority", "ALTER TABLE events ADD COLUMN priority INTEGER NOT NULL DEFAULT 0"},
        {"timezone", "ALTER TABLE events ADD COLUMN timezone TEXT NOT NULL DEFAULT ''"},
        {"category_id", "ALTER TABLE events ADD COLUMN category_id INTEGER"},
    };

    for (const ColumnMigration &migration : migrations) {
        if (!existingColumns.contains(migration.name)) {
            if (!query.exec(migration.alterSql))
                qWarning() << "Migration failed for column" << migration.name << ":" << query.lastError().text();
        }
    }
}

EventManager::~EventManager()
{
    // Закрываем соединение и убираем его из реестра Qt SQL явно,
    // иначе при завершении приложения Qt выводит предупреждение
    // "connection still in use" в консоль.
    QSqlDatabase::database(m_connectionName).close();
    QSqlDatabase::removeDatabase(m_connectionName);
}

int EventManager::addEvent(const Event &event)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(
        "INSERT INTO events (title, date, start_time, end_time, description, "
        "recurrence_type, recurrence_interval, recurrence_end_date, priority, timezone, category_id) "
        "VALUES (:title, :date, :start_time, :end_time, :description, "
        ":recurrence_type, :recurrence_interval, :recurrence_end_date, :priority, :timezone, :category_id)"
    );
    query.bindValue(":title", event.title);
    query.bindValue(":date", event.date.toString(Qt::ISODate));
    query.bindValue(":start_time", event.startTime.toString(Qt::ISODate));
    query.bindValue(":end_time", event.endTime.toString(Qt::ISODate));
    query.bindValue(":description", event.description);
    query.bindValue(":recurrence_type", recurrenceTypeToString(event.recurrenceType));
    query.bindValue(":recurrence_interval", event.recurrenceInterval);
    query.bindValue(":recurrence_end_date",
        event.recurrenceEndDate.isValid() ? QVariant(event.recurrenceEndDate.toString(Qt::ISODate)) : QVariant());
    query.bindValue(":priority", event.priority);
    query.bindValue(":timezone", nonNullTimezone(event.timezone));
    query.bindValue(":category_id", event.categoryId >= 0 ? QVariant(event.categoryId) : QVariant());

    if (!query.exec()) {
        qWarning() << "Failed to insert event:" << query.lastError().text();
        return -1;
    }

    return query.lastInsertId().toInt();
}

bool EventManager::updateEvent(const Event &event)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(
        "UPDATE events SET title = :title, date = :date, start_time = :start_time, "
        "end_time = :end_time, description = :description, "
        "recurrence_type = :recurrence_type, recurrence_interval = :recurrence_interval, "
        "recurrence_end_date = :recurrence_end_date, priority = :priority, timezone = :timezone, "
        "category_id = :category_id "
        "WHERE id = :id"
    );
    query.bindValue(":title", event.title);
    query.bindValue(":date", event.date.toString(Qt::ISODate));
    query.bindValue(":start_time", event.startTime.toString(Qt::ISODate));
    query.bindValue(":end_time", event.endTime.toString(Qt::ISODate));
    query.bindValue(":description", event.description);
    query.bindValue(":recurrence_type", recurrenceTypeToString(event.recurrenceType));
    query.bindValue(":recurrence_interval", event.recurrenceInterval);
    query.bindValue(":recurrence_end_date",
        event.recurrenceEndDate.isValid() ? QVariant(event.recurrenceEndDate.toString(Qt::ISODate)) : QVariant());
    query.bindValue(":priority", event.priority);
    query.bindValue(":timezone", nonNullTimezone(event.timezone));
    query.bindValue(":category_id", event.categoryId >= 0 ? QVariant(event.categoryId) : QVariant());
    query.bindValue(":id", event.id);

    if (!query.exec()) {
        qWarning() << "Failed to update event:" << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool EventManager::removeEvent(int id)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));

    // Сначала убираем execution-записи этого события (MVP3.0) - иначе
    // они останутся сиротами в event_executions после удаления события
    // (тот же принцип, что и очистка category_id в removeCategory()).
    query.prepare("DELETE FROM event_executions WHERE event_id = :id");
    query.bindValue(":id", id);
    if (!query.exec())
        qWarning() << "Failed to clear executions before delete:" << query.lastError().text();

    query.prepare("DELETE FROM events WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        qWarning() << "Failed to delete event:" << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool EventManager::eventById(int id, Event &outEvent) const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(eventsSelectWithCategoryJoin() + " WHERE events.id = :id");
    query.bindValue(":id", id);

    if (!query.exec() || !query.next())
        return false;

    outEvent = eventFromQuery(query);
    return true;
}

bool EventManager::incrementPomodoroCount(int id)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare("UPDATE events SET pomodoros_completed = pomodoros_completed + 1 WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        qWarning() << "Failed to increment pomodoro count:" << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

QVector<Event> EventManager::eventsForDate(const QDate &date) const
{
    QVector<Event> result;

    // 1. Обычные (неповторяющиеся) события, у которых date совпадает буквально.
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(eventsSelectWithCategoryJoin() + " WHERE events.date = :date AND events.recurrence_type = 'none'");
    query.bindValue(":date", date.toString(Qt::ISODate));

    if (!query.exec()) {
        qWarning() << "Failed to load events:" << query.lastError().text();
        return result;
    }

    while (query.next())
        result.append(eventFromQuery(query));

    // 2. Повторяющиеся события - проверяем каждый шаблон через eventOccursOnDate.
    // Шаблонов обычно немного, поэтому не грузим их каждый по отдельному запросу.
    for (const Event &templateEvent : allRecurringTemplates()) {
        if (eventOccursOnDate(templateEvent, date)) {
            Event occurrence = templateEvent;
            occurrence.date = date; // для отображения показываем именно запрошенную дату
            result.append(occurrence);
        }
    }

    std::sort(result.begin(), result.end(), [](const Event &a, const Event &b) {
        return a.startTime < b.startTime;
    });

    return result;
}

QSet<QDate> EventManager::datesWithEvents(const QDate &rangeStart, const QDate &rangeEnd) const
{
    QSet<QDate> dates;

    if (!rangeStart.isValid() || !rangeEnd.isValid() || rangeStart > rangeEnd)
        return dates;

    // 1. Обычные события внутри диапазона - один SQL-запрос.
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(
        "SELECT DISTINCT date FROM events "
        "WHERE recurrence_type = 'none' AND date BETWEEN :start AND :end"
    );
    query.bindValue(":start", rangeStart.toString(Qt::ISODate));
    query.bindValue(":end", rangeEnd.toString(Qt::ISODate));

    if (!query.exec()) {
        qWarning() << "Failed to load event dates:" << query.lastError().text();
        return dates;
    }

    while (query.next())
        dates.insert(QDate::fromString(query.value(0).toString(), Qt::ISODate));

    // 2. Повторяющиеся события - т.к. диапазон (обычно ~6 недель для сетки
    // календаря) невелик, просто перебираем каждый день диапазона против
    // каждого шаблона. Именно ограниченность диапазона и позволяет не
    // перечислять "все даты навсегда" для событий без даты окончания.
    const QVector<Event> templates = allRecurringTemplates();
    for (QDate day = rangeStart; day <= rangeEnd; day = day.addDays(1)) {
        for (const Event &templateEvent : templates) {
            if (eventOccursOnDate(templateEvent, day)) {
                dates.insert(day);
                break; // на этот день уже есть хотя бы одно событие, дальше не проверяем
            }
        }
    }

    return dates;
}

QVector<Event> EventManager::eventsInRange(const QDate &rangeStart, const QDate &rangeEnd) const
{
    QVector<Event> result;

    if (!rangeStart.isValid() || !rangeEnd.isValid() || rangeStart > rangeEnd)
        return result;

    // 1. Обычные события внутри диапазона - один SQL-запрос.
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(
        eventsSelectWithCategoryJoin() + " WHERE events.recurrence_type = 'none' AND events.date BETWEEN :start AND :end"
    );
    query.bindValue(":start", rangeStart.toString(Qt::ISODate));
    query.bindValue(":end", rangeEnd.toString(Qt::ISODate));

    if (!query.exec()) {
        qWarning() << "Failed to load events in range:" << query.lastError().text();
        return result;
    }

    while (query.next())
        result.append(eventFromQuery(query));

    // 2. Повторяющиеся события - та же идея, что в datesWithEvents(): диапазон
    // (обычно ~6 недель для сетки месяца) невелик, поэтому просто перебираем
    // каждый день против каждого шаблона.
    const QVector<Event> templates = allRecurringTemplates();
    for (QDate day = rangeStart; day <= rangeEnd; day = day.addDays(1)) {
        for (const Event &templateEvent : templates) {
            if (eventOccursOnDate(templateEvent, day)) {
                Event occurrence = templateEvent;
                occurrence.date = day;
                result.append(occurrence);
            }
        }
    }

    std::sort(result.begin(), result.end(), [](const Event &a, const Event &b) {
        if (a.date != b.date)
            return a.date < b.date;
        return a.startTime < b.startTime;
    });

    return result;
}

QVector<Event> EventManager::allRecurringTemplates() const
{
    QVector<Event> templates;

    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (!query.exec(eventsSelectWithCategoryJoin() + " WHERE events.recurrence_type <> 'none'")) {
        qWarning() << "Failed to load recurring events:" << query.lastError().text();
        return templates;
    }

    while (query.next())
        templates.append(eventFromQuery(query));

    return templates;
}

int EventManager::addCategory(const Category &category)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare("INSERT INTO categories (name, color) VALUES (:name, :color)");
    query.bindValue(":name", category.name);
    query.bindValue(":color", category.color);

    if (!query.exec()) {
        qWarning() << "Failed to insert category:" << query.lastError().text();
        return -1;
    }

    return query.lastInsertId().toInt();
}

bool EventManager::updateCategory(const Category &category)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare("UPDATE categories SET name = :name, color = :color WHERE id = :id");
    query.bindValue(":name", category.name);
    query.bindValue(":color", category.color);
    query.bindValue(":id", category.id);

    if (!query.exec()) {
        qWarning() << "Failed to update category:" << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool EventManager::removeCategory(int id)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));

    // Сначала "отвязываем" от категории все события, которые на неё
    // ссылались - без этого шага у них остался бы category_id, ведущий
    // в никуда (строка в categories уже удалена). Раз в этой схеме нет
    // настоящих FK-констрейнтов, целостность поддерживаем вручную, здесь.
    query.prepare("UPDATE events SET category_id = NULL WHERE category_id = :id");
    query.bindValue(":id", id);
    if (!query.exec())
        qWarning() << "Failed to clear category_id before delete:" << query.lastError().text();

    query.prepare("DELETE FROM categories WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec()) {
        qWarning() << "Failed to delete category:" << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool EventManager::categoryById(int id, Category &outCategory) const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare("SELECT * FROM categories WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec() || !query.next())
        return false;

    outCategory.id = query.value("id").toInt();
    outCategory.name = query.value("name").toString();
    outCategory.color = query.value("color").toString();
    return true;
}

QVector<Category> EventManager::allCategories() const
{
    QVector<Category> result;

    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (!query.exec("SELECT * FROM categories ORDER BY name COLLATE NOCASE")) {
        qWarning() << "Failed to load categories:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        Category category;
        category.id = query.value("id").toInt();
        category.name = query.value("name").toString();
        category.color = query.value("color").toString();
        result.append(category);
    }

    return result;
}

bool EventManager::executionForOccurrence(int eventId, const QDate &occurrenceDate, EventExecution &outExecution) const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare("SELECT * FROM event_executions WHERE event_id = :event_id AND occurrence_date = :occurrence_date");
    query.bindValue(":event_id", eventId);
    query.bindValue(":occurrence_date", occurrenceDate.toString(Qt::ISODate));

    if (!query.exec() || !query.next())
        return false;

    outExecution.id = query.value("id").toInt();
    outExecution.eventId = query.value("event_id").toInt();
    outExecution.occurrenceDate = QDate::fromString(query.value("occurrence_date").toString(), Qt::ISODate);
    outExecution.status = executionStatusFromString(query.value("status").toString());

    const QString actualStartStr = query.value("actual_start").toString();
    outExecution.actualStart = actualStartStr.isEmpty()
        ? QDateTime() : QDateTime::fromString(actualStartStr, Qt::ISODate);

    const QString actualEndStr = query.value("actual_end").toString();
    outExecution.actualEnd = actualEndStr.isEmpty()
        ? QDateTime() : QDateTime::fromString(actualEndStr, Qt::ISODate);

    return true;
}

bool EventManager::upsertExecution(const EventExecution &execution)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));

    // INSERT OR REPLACE полагается на UNIQUE(event_id, occurrence_date) -
    // существующая строка (если была) удаляется и вставляется заново
    // целиком; execution.id при этом не сохраняется (у replaced-строки
    // будет новый id), но нигде за пределами этого класса id execution-
    // записи не используется, так что это не проблема.
    query.prepare(
        "INSERT OR REPLACE INTO event_executions "
        "(event_id, occurrence_date, status, actual_start, actual_end) "
        "VALUES (:event_id, :occurrence_date, :status, :actual_start, :actual_end)"
    );
    query.bindValue(":event_id", execution.eventId);
    query.bindValue(":occurrence_date", execution.occurrenceDate.toString(Qt::ISODate));
    query.bindValue(":status", executionStatusToString(execution.status));
    query.bindValue(":actual_start",
        execution.actualStart.isValid() ? QVariant(execution.actualStart.toString(Qt::ISODate)) : QVariant());
    query.bindValue(":actual_end",
        execution.actualEnd.isValid() ? QVariant(execution.actualEnd.toString(Qt::ISODate)) : QVariant());

    if (!query.exec()) {
        qWarning() << "Failed to save execution state:" << query.lastError().text();
        return false;
    }

    return true;
}

bool EventManager::startOccurrence(int eventId, const QDate &occurrenceDate, const QDateTime &startedAt)
{
    EventExecution execution;
    execution.eventId = eventId;
    execution.occurrenceDate = occurrenceDate;
    execution.status = ExecutionStatus::Running;
    execution.actualStart = startedAt;
    // actualEnd остаётся невалидным - вхождение ещё не завершено.
    return upsertExecution(execution);
}

bool EventManager::completeOccurrence(int eventId, const QDate &occurrenceDate, const QDateTime &completedAt)
{
    EventExecution existing;
    const bool hasExisting = executionForOccurrence(eventId, occurrenceDate, existing);

    EventExecution execution;
    execution.eventId = eventId;
    execution.occurrenceDate = occurrenceDate;
    execution.status = ExecutionStatus::Completed;

    if (hasExisting && existing.actualStart.isValid()) {
        // Было startOccurrence() до этого - "Track actual time" (раздел 4):
        // actualStart уже зафиксирован, теперь фиксируем и actualEnd.
        execution.actualStart = existing.actualStart;
        execution.actualEnd = completedAt;
    }
    // Иначе - "Manual completion" (раздел 4): Complete нажали без
    // предварительного Start, actualStart/actualEnd остаются невалидными.

    return upsertExecution(execution);
}

ExecutionStatus EventManager::effectiveStatus(int eventId, const QDate &occurrenceDate,
                                               const QTime &plannedStart, const QTime &plannedEnd,
                                               const QDateTime &now, int gracePeriodMinutes) const
{
    EventExecution execution;
    const bool hasExecution = executionForOccurrence(eventId, occurrenceDate, execution);
    return resolveExecutionStatus(
        occurrenceDate, plannedStart, plannedEnd,
        hasExecution ? &execution : nullptr,
        now, gracePeriodMinutes
    );
}

NowSnapshot EventManager::nowSnapshot(const QDate &date, const QDateTime &now, int gracePeriodMinutes) const
{
    NowSnapshot snapshot;

    const QVector<Event> events = eventsForDate(date);
    snapshot.allForDate.reserve(events.size());
    for (const Event &event : events) {
        EventExecution execution;
        const bool hasExecution = executionForOccurrence(event.id, event.date, execution);

        EventWithStatus item;
        item.event = event;
        item.execution = hasExecution ? execution : EventExecution();
        item.status = resolveExecutionStatus(
            event.date, event.startTime, event.endTime,
            hasExecution ? &execution : nullptr, now, gracePeriodMinutes
        );

        snapshot.allForDate.append(item);
    }

    const QTime nowTime = now.time();

    EventWithStatus current;
    if (findCurrentTask(snapshot.allForDate, nowTime, current)) {
        snapshot.hasCurrent = true;
        snapshot.current = current;
    }

    EventWithStatus next;
    if (findNextTask(snapshot.allForDate, nowTime, snapshot.hasCurrent ? &snapshot.current : nullptr, next)) {
        snapshot.hasNext = true;
        snapshot.next = next;
    }

    return snapshot;
}
