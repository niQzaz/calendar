#include "CsvImporter.h"

#include <QFile>
#include <QTextStream>
#include <QDate>
#include <QTime>

namespace {
const char *kDateFormat = "dd.MM.yyyy";
const char *kTimeFormat = "HH:mm";
constexpr int kExpectedColumnCount = 5;
}

CsvImportResult CsvImporter::importFromFile(const QString &filePath)
{
    CsvImportResult result;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.fileOpenFailed = true;
        return result;
    }

    QTextStream stream(&file);

    bool headerSkipped = false;
    int lineNumber = 0;

    while (!stream.atEnd()) {
        const QString rawLine = stream.readLine();
        ++lineNumber;

        const QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue; // пустые строки пропускаем молча - это не ошибка

        if (!headerSkipped) {
            // Первая непустая строка - заголовок, не разбираем её как данные.
            headerSkipped = true;
            continue;
        }

        result.totalDataRows++;

        Event event;
        QString error;
        if (parseRow(line, lineNumber, event, error))
            result.validEvents.append(event);
        else
            result.errors.append(error);
    }

    return result;
}

bool CsvImporter::parseRow(const QString &line, int lineNumber, Event &outEvent, QString &outError)
{
    const QStringList fields = line.split(',');

    if (fields.size() != kExpectedColumnCount) {
        outError = QString("Строка %1: ожидалось %2 столбцов, найдено %3")
            .arg(lineNumber)
            .arg(kExpectedColumnCount)
            .arg(fields.size());
        return false;
    }

    const QString dateStr = fields[0].trimmed();
    const QString startStr = fields[1].trimmed();
    const QString endStr = fields[2].trimmed();
    const QString subject = fields[3].trimmed();
    const QString description = fields[4].trimmed();

    const QDate date = QDate::fromString(dateStr, kDateFormat);
    if (!date.isValid()) {
        outError = QString("Строка %1: некорректная дата \"%2\" (ожидается dd.MM.yyyy)")
            .arg(lineNumber)
            .arg(dateStr);
        return false;
    }

    const QTime startTime = QTime::fromString(startStr, kTimeFormat);
    if (!startTime.isValid()) {
        outError = QString("Строка %1: некорректное время начала \"%2\" (ожидается HH:mm)")
            .arg(lineNumber)
            .arg(startStr);
        return false;
    }

    const QTime endTime = QTime::fromString(endStr, kTimeFormat);
    if (!endTime.isValid()) {
        outError = QString("Строка %1: некорректное время окончания \"%2\" (ожидается HH:mm)")
            .arg(lineNumber)
            .arg(endStr);
        return false;
    }

    if (startTime >= endTime) {
        outError = QString("Строка %1: время окончания раньше (или равно) времени начала").arg(lineNumber);
        return false;
    }

    if (subject.isEmpty()) {
        outError = QString("Строка %1: не указано название события").arg(lineNumber);
        return false;
    }

    outEvent.title = subject;
    outEvent.date = date;
    outEvent.startTime = startTime;
    outEvent.endTime = endTime;
    outEvent.description = description;

    return true;
}
