#pragma once

#include <QString>
#include <QDate>
#include <QTime>

// Событие расписания: название, дата, время начала/окончания, описание.
//
// Это просто структура данных (POD-like), без какой-либо логики внутри.
// Вся логика работы с коллекцией событий (добавление, удаление, поиск)
// находится в EventManager, а не здесь - так каждый класс отвечает
// только за одну вещь: Event хранит данные одного события,
// EventManager управляет множеством событий.
struct Event
{
    int id = -1;   // уникальный идентификатор, назначается EventManager'ом
                   // (-1 означает "id ещё не присвоен")
    QString title;
    QDate date;
    QTime startTime;
    QTime endTime;
    QString description;
};
