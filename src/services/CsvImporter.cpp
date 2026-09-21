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

QVector<QStringList> CsvImporter::parseCsvContent(const QString &content)
{
    QVector<QStringList> rows;
    QStringList currentRow;
    QString currentField;
    bool inQuotes = false;

    const int n = content.size();
    int i = 0;

    while (i < n) {
        const QChar ch = content.at(i);

        if (inQuotes) {
            if (ch == QLatin1Char('"')) {
                if (i + 1 < n && content.at(i + 1) == QLatin1Char('"')) {
                    // Экранированная кавычка внутри поля: "" -> одна буквальная кавычка.
                    currentField.append(QLatin1Char('"'));
                    i += 2;
                    continue;
                }
                inQuotes = false; // закрывающая кавычка
                ++i;
                continue;
            }
            // Внутри кавычек буквальны любые символы, включая запятые и переводы строк.
            currentField.append(ch);
            ++i;
            continue;
        }

        if (ch == QLatin1Char('"')) {
            inQuotes = true;
            ++i;
            continue;
        }

        if (ch == QLatin1Char(',')) {
            currentRow.append(currentField);
            currentField.clear();
            ++i;
            continue;
        }

        if (ch == QLatin1Char('\r')) {
            // \r\n считаем одним разделителем записи; одиночный \r (старый
            // Mac-стиль переноса строк) - тоже разделителем.
            if (i + 1 < n && content.at(i + 1) == QLatin1Char('\n'))
                ++i;
            currentRow.append(currentField);
            currentField.clear();
            rows.append(currentRow);
            currentRow.clear();
            ++i;
            continue;
        }

        if (ch == QLatin1Char('\n')) {
            currentRow.append(currentField);
            currentField.clear();
            rows.append(currentRow);
            currentRow.clear();
            ++i;
            continue;
        }

        currentField.append(ch);
        ++i;
    }

    // Последняя запись, если файл не заканчивается переводом строки.
    if (!currentField.isEmpty() || !currentRow.isEmpty()) {
        currentRow.append(currentField);
        rows.append(currentRow);
    }

    return rows;
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
    QString content = stream.readAll();

    // Снимаем UTF-8 BOM (U+FEFF), если он есть в начале файла - некоторые
    // приложения (например, Excel) добавляют его при экспорте в UTF-8,
    // и без снятия он "прилип" бы к первому полю заголовка.
    if (!content.isEmpty() && content.at(0) == QChar(0xFEFF))
        content.remove(0, 1);

    const QVector<QStringList> rows = parseCsvContent(content);

    bool headerSkipped = false;
    int recordNumber = 0;

    for (const QStringList &fields : rows) {
        ++recordNumber;

        // Полностью пустая запись (одно пустое поле - т.е. просто пустая
        // строка в файле) пропускается молча, это не ошибка.
        const bool isBlankRow = (fields.size() == 1 && fields.first().trimmed().isEmpty());
        if (isBlankRow)
            continue;

        if (!headerSkipped) {
            // Первая непустая запись - заголовок, не разбираем её как данные.
            headerSkipped = true;
            continue;
        }

        result.totalDataRows++;

        Event event;
        QString error;
        if (parseRow(fields, recordNumber, event, error))
            result.validEvents.append(event);
        else
            result.errors.append(error);
    }

    return result;
}

bool CsvImporter::parseRow(const QStringList &fields, int recordNumber, Event &outEvent, QString &outError)
{
    if (fields.size() != kExpectedColumnCount) {
        outError = QString("Строка %1: ожидалось %2 столбцов, найдено %3")
            .arg(recordNumber)
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
            .arg(recordNumber)
            .arg(dateStr);
        return false;
    }

    const QTime startTime = QTime::fromString(startStr, kTimeFormat);
    if (!startTime.isValid()) {
        outError = QString("Строка %1: некорректное время начала \"%2\" (ожидается HH:mm)")
            .arg(recordNumber)
            .arg(startStr);
        return false;
    }

    const QTime endTime = QTime::fromString(endStr, kTimeFormat);
    if (!endTime.isValid()) {
        outError = QString("Строка %1: некорректное время окончания \"%2\" (ожидается HH:mm)")
            .arg(recordNumber)
            .arg(endStr);
        return false;
    }

    if (startTime >= endTime) {
        outError = QString("Строка %1: время окончания раньше (или равно) времени начала").arg(recordNumber);
        return false;
    }

    if (subject.isEmpty()) {
        outError = QString("Строка %1: не указано название события").arg(recordNumber);
        return false;
    }

    outEvent.title = subject;
    outEvent.date = date;
    outEvent.startTime = startTime;
    outEvent.endTime = endTime;
    outEvent.description = description;

    return true;
}
