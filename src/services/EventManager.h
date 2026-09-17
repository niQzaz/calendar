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

    // Все события на дату, отсортированные по времени начала.
    QVector<Event> eventsForDate(const QDate &date) const;

    // Даты, на которые есть хотя бы одно событие
    // (используется CalendarWidget, чтобы пометить такие дни в сетке).
    QSet<QDate> datesWithEvents() const;

private:
    QString m_connectionName;
};
