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

    return event;
}

} // namespace

EventManager::EventManager()
    : m_connectionName(kConnectionName)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);

    // Файл базы кладём в стандартную папку данных приложения
    // (на Linux это обычно ~/.local/share/<AppName>), а не рядом
    // с исполняемым файлом - так принято для пользовательских данных.
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir); // на случай первого запуска - папки ещё нет
    db.setDatabaseName(dataDir + "/calendar.db");

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
        "  recurrence_end_date TEXT"
        ")"
    );

    if (!ok)
        qWarning() << "Failed to create events table:" << query.lastError().text();

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
        "recurrence_type, recurrence_interval, recurrence_end_date) "
        "VALUES (:title, :date, :start_time, :end_time, :description, "
        ":recurrence_type, :recurrence_interval, :recurrence_end_date)"
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
        "recurrence_end_date = :recurrence_end_date WHERE id = :id"
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
    query.prepare("SELECT * FROM events WHERE id = :id");
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
    query.prepare("SELECT * FROM events WHERE date = :date AND recurrence_type = 'none'");
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
        "SELECT * FROM events WHERE recurrence_type = 'none' AND date BETWEEN :start AND :end"
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
    if (!query.exec("SELECT * FROM events WHERE recurrence_type <> 'none'")) {
        qWarning() << "Failed to load recurring events:" << query.lastError().text();
        return templates;
    }

    while (query.next())
        templates.append(eventFromQuery(query));

    return templates;
}
