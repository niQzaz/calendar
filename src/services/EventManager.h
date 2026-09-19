#pragma once

#include "models/Event.h"

#include <QVector>
#include <QSet>
#include <QDate>
#include <QString>

// EventManager отвечает за хранение событий в SQLite.
//
// В Этапе 1 (MVP) события хранились в QVector в оперативной памяти.
// Публичный интерфейс класса почти не изменился (addEvent/removeEvent/
// eventsForDate/datesWithEvents остались) - поменялась только реализация
// внутри .cpp. UI (MainWindow) от этого почти не пострадал - именно
// ради этого EventManager был с самого начала вынесен в отдельный
// класс, а не размазан по MainWindow.
//
// Добавились:
//  - updateEvent() - для редактирования существующего события;
//  - eventById()   - нужен диалогу редактирования, чтобы получить
//                     полные данные события по его id из списка.
class EventManager
{
public:
    // Открывает (или создаёт, если его ещё нет) файл базы данных
    // и создаёт таблицу events, если она отсутствует.
    EventManager();
    ~EventManager();

    // EventManager владеет соединением с БД (именованным подключением
    // Qt SQL) - это ресурс, копировать его не нужно и небезопасно,
    // поэтому явно запрещаем копирование (тот же принцип, что у RAII-обёрток
    // вроде std::unique_ptr).
    EventManager(const EventManager &) = delete;
    EventManager &operator=(const EventManager &) = delete;

    // Добавляет событие, возвращает присвоенный базой id.
    // Поле event.id при этом игнорируется. При ошибке возвращает -1.
    int addEvent(const Event &event);

    // Обновляет событие с идентификатором event.id. true при успехе.
    bool updateEvent(const Event &event);

    // Удаляет событие по id. true, если событие было найдено и удалено.
    bool removeEvent(int id);

    // Находит событие по id. Возвращает true и заполняет outEvent, если найдено.
    bool eventById(int id, Event &outEvent) const;

    // Увеличивает счётчик завершённых pomodoro для события на 1.
    bool incrementPomodoroCount(int id);

    // Все события на дату, отсортированные по времени начала.
    QVector<Event> eventsForDate(const QDate &date) const;

    // Даты в диапазоне [rangeStart, rangeEnd], на которые есть хотя бы одно
    // событие (обычное или повторяющееся). Диапазон обязателен: у повторяющегося
    // события без даты окончания "все даты, на которые оно есть" - бесконечное
    // множество, поэтому считаем только для конкретного видимого диапазона
    // (используется CalendarWidget, чтобы пометить такие дни в сетке месяца).
    QSet<QDate> datesWithEvents(const QDate &rangeStart, const QDate &rangeEnd) const;

    // Все события (обычные и повторяющиеся) в диапазоне [rangeStart, rangeEnd],
    // каждое с полем date, выставленным в конкретную дату вхождения -
    // используется MonthView для отрисовки мини-карточек событий внутри ячеек,
    // а в будущем и Week/Day View. Та же идея, что у eventsForDate(), только
    // сразу на диапазон дат, а не на один день.
    QVector<Event> eventsInRange(const QDate &rangeStart, const QDate &rangeEnd) const;

private:
    // Загружает все события с recurrence_type != 'none' - их всегда немного
    // (это шаблоны, а не отдельные повторения), поэтому дальше с ними
    // работаем в памяти через eventOccursOnDate(), а не через SQL.
    QVector<Event> allRecurringTemplates() const;

    QString m_connectionName;
};
