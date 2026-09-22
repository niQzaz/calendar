#pragma once

#include "models/Event.h"

#include <QVector>
#include <QStringList>
#include <QString>

// Результат импорта CSV-файла.
struct CsvImportResult
{
    QVector<Event> validEvents;    // события, успешно распознанные и готовые к сохранению
    QStringList errors;            // человекочитаемые описания проблемных строк
    int totalDataRows = 0;         // строк-задач в файле (без заголовка, пустых строк и section)
    bool fileOpenFailed = false;   // true, если сам файл не удалось открыть
    bool unsupportedFormat = false; // true, если заголовок не совпал ни с одним известным форматом
};

// CsvImporter - разбор CSV-файла расписания в список событий.
//
// Класс без состояния (все методы статические) - между вызовами ему
// нечего хранить, поэтому создавать объект незачем (в отличие от
// EventManager, который держит открытое соединение с БД).
//
// Поддерживает ДВА формата файла, определяет нужный по строке заголовка:
//
// 1) "Простой" формат (введён в MVP):
//      date,start,end,subject,description
//      15.09.2026,09:00,10:30,Математика,Алгебра
//    Каждая строка - одноразовое событие с явной датой/временем.
//
// 2) "Недельное расписание" (формат внешнего приложения):
//      TYPE,CONTENT,DESCRIPTION,PRIORITY,INDENT,DATE,DATE_LANG,TIMEZONE,DURATION,DURATION_UNIT
//      task,Подъём и разминка,Вода + разминка,3,2,every monday at 06:20,ru,Europe/Moscow,10,minute
//    Строки TYPE=section - это просто заголовки группировки (например,
//    "Понедельник"), не события - пропускаются молча, как пустые строки.
//    Строки TYPE=task с полем DATE вида "every <день недели> at HH:MM"
//    становятся ОДНИМ повторяющимся событием (RecurrenceType::Weekly) -
//    не разворачиваются заранее в конкретные даты, см. EventManager.
//
// Полноценный RFC4180 CSV-разбор (кавычки, экранированные кавычки, запятые
// внутри полей, CRLF/LF, BOM) - через parseCsvContent(), общий для обоих форматов.
class CsvImporter
{
public:
    static CsvImportResult importFromFile(const QString &filePath);

private:
    enum class Format
    {
        Unknown,
        Simple,          // date,start,end,subject,description
        WeeklySchedule    // TYPE,CONTENT,DESCRIPTION,PRIORITY,INDENT,DATE,DATE_LANG,TIMEZONE,DURATION,DURATION_UNIT
    };

    // Разбирает весь текст файла на записи (строки) и поля с учётом
    // CSV-правил про кавычки:
    //  - поле в двойных кавычках может содержать запятые и переводы строк;
    //  - "" внутри такого поля - это экранированная одна кавычка;
    //  - вне кавычек запятая - разделитель полей, перевод строки - разделитель записей.
    // Работает по всему тексту сразу (а не построчно), потому что запись
    // с кавычками может занимать больше одной физической строки файла.
    static QVector<QStringList> parseCsvContent(const QString &content);

    // Определяет формат файла по уже разобранной строке заголовка.
    static Format detectFormat(const QStringList &headerFields);

    // "Простой" формат - одноразовое событие с явной датой.
    static bool parseSimpleRow(const QStringList &fields, int recordNumber, Event &outEvent, QString &outError);

    // "Недельное расписание" - строка TYPE=task превращается в повторяющееся
    // (Weekly) событие. Строки TYPE=section обрабатываются отдельно в importFromFile
    // (они не события вообще, поэтому этот метод для них не вызывается).
    static bool parseWeeklyTaskRow(const QStringList &fields, int recordNumber, Event &outEvent, QString &outError);

    // "every monday at 06:20" -> день недели (1=Пн..7=Вс, как QDate::dayOfWeek())
    // и время. false, если строка не соответствует этому шаблону.
    static bool parseEveryWeekdayAt(const QString &raw, int &outIsoWeekday, QTime &outTime);

    // "10" + "minute" -> 10; "1,5" + "hour" -> 90 (округление до минуты).
    // false, если значение некорректно (не число, <= 0, неизвестная единица).
    static bool parseDurationMinutes(const QString &durationStr, const QString &unitStr, int &outMinutes);

    // Любая дата с нужным днём недели, начиная от сегодняшней - для Weekly
    // событий конкретная дата-якорь не имеет значения (eventOccursOnDate
    // сравнивает только день недели), поэтому просто берём ближайшую.
    static QDate anchorDateForWeekday(int isoWeekday);
};
