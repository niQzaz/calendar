#pragma once

#include "models/Event.h"

#include <QVector>
#include <QSet>
#include <QDate>

// EventManager хранит все события приложения и предоставляет операции
// добавления / удаления / поиска.
//
// На этапе MVP хранение полностью в оперативной памяти (QVector) - все
// события пропадают при закрытии приложения, это ожидаемо для MVP.
//
// В Этапе 2 внутренняя реализация будет заменена на SQLite, но публичный
// интерфейс этого класса должен остаться прежним. Именно поэтому UI-код
// (MainWindow) работает только с EventManager и никогда не трогает
// хранилище напрямую - при замене хранилища на SQLite менять UI не придётся.
class EventManager
{
public:
    // Добавляет событие и возвращает присвоенный ему id.
    int addEvent(const Event &event);

    // Удаляет событие по id. Возвращает true, если событие было найдено и удалено.
    bool removeEvent(int id);

    // Возвращает все события на указанную дату, отсортированные по времени начала.
    QVector<Event> eventsForDate(const QDate &date) const;

    // Возвращает множество дат, на которые есть хотя бы одно событие
    // (используется CalendarWidget, чтобы пометить такие дни в сетке).
    QSet<QDate> datesWithEvents() const;

private:
    QVector<Event> m_events;
    int m_nextId = 1;
};
