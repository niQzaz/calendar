#include "EventManager.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

namespace {

// Все запросы этого класса используют одно и то же именованное
// соединение, чтобы не плодить лишние подключения к одному и тому же
// файлу базы (у Qt SQL соединения различаются по имени, а не по объекту).
const char *kConnectionName = "calendar_connection";

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
        "  description TEXT"
        ")"
    );

    if (!ok)
        qWarning() << "Failed to create events table:" << query.lastError().text();
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
        "INSERT INTO events (title, date, start_time, end_time, description) "
        "VALUES (:title, :date, :start_time, :end_time, :description)"
    );
    query.bindValue(":title", event.title);
    query.bindValue(":date", event.date.toString(Qt::ISODate));
    query.bindValue(":start_time", event.startTime.toString(Qt::ISODate));
    query.bindValue(":end_time", event.endTime.toString(Qt::ISODate));
    query.bindValue(":description", event.description);

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
        "end_time = :end_time, description = :description WHERE id = :id"
    );
    query.bindValue(":title", event.title);
    query.bindValue(":date", event.date.toString(Qt::ISODate));
    query.bindValue(":start_time", event.startTime.toString(Qt::ISODate));
    query.bindValue(":end_time", event.endTime.toString(Qt::ISODate));
    query.bindValue(":description", event.description);
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

QVector<Event> EventManager::eventsForDate(const QDate &date) const
{
    QVector<Event> result;

    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare("SELECT * FROM events WHERE date = :date ORDER BY start_time");
    query.bindValue(":date", date.toString(Qt::ISODate));

    if (!query.exec()) {
        qWarning() << "Failed to load events:" << query.lastError().text();
        return result;
    }

    while (query.next())
        result.append(eventFromQuery(query));

    return result;
}

QSet<QDate> EventManager::datesWithEvents() const
{
    QSet<QDate> dates;

    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (!query.exec("SELECT DISTINCT date FROM events")) {
        qWarning() << "Failed to load event dates:" << query.lastError().text();
        return dates;
    }

    while (query.next())
        dates.insert(QDate::fromString(query.value(0).toString(), Qt::ISODate));

    return dates;
}
