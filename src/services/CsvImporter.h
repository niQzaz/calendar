#pragma once

#include "models/Event.h"

#include <QVector>
#include <QStringList>
#include <QString>

// Результат импорта CSV-файла.
struct CsvImportResult
{
    QVector<Event> validEvents;   // события, успешно распознанные и готовые к сохранению
    QStringList errors;           // человекочитаемые описания проблемных строк
    int totalDataRows = 0;        // строк с данными в файле (без заголовка и пустых строк)
    bool fileOpenFailed = false;  // true, если сам файл не удалось открыть
};

// CsvImporter - разбор CSV-файла расписания в список событий.
//
// Класс без состояния (все методы статические) - между вызовами ему
// нечего хранить, поэтому создавать объект незачем (в отличие от
// EventManager, который держит открытое соединение с БД).
//
// Ожидаемый формат файла:
//   date,start,end,subject,description
//   15.09.2026,09:00,10:30,Математика,Алгебра
// Первая строка (заголовок) всегда пропускается, без проверки на
// соответствие ожидаемым названиям столбцов - для формата из задания
// этого достаточно.
//
// Не является "настоящим" CSV-парсером - не поддерживает поля в кавычках
// с запятыми внутри и т.п. По заданию полноценный Excel-парсер не нужен,
// а для простого формата расписания разбор по запятой работает надёжно.
class CsvImporter
{
public:
    static CsvImportResult importFromFile(const QString &filePath);

private:
    // Разбирает одну строку данных. true и outEvent заполнен, если строка
    // корректна; иначе false и текст ошибки в outError.
    static bool parseRow(const QString &line, int lineNumber, Event &outEvent, QString &outError);
};
