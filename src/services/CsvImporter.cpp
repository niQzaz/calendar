#include "CsvImporter.h"

#include <QFile>
#include <QTextStream>
#include <QDate>
#include <QTime>
#include <QRegularExpression>
#include <QMap>

namespace {
const char *kDateFormat = "dd.MM.yyyy";
const char *kTimeFormat = "HH:mm";
constexpr int kSimpleColumnCount = 5;
constexpr int kWeeklyColumnCount = 10;

// Индексы столбцов "недельного расписания" - по порядку в заголовке
// TYPE,CONTENT,DESCRIPTION,PRIORITY,INDENT,DATE,DATE_LANG,TIMEZONE,DURATION,DURATION_UNIT
enum WeeklyColumn
{
    ColType = 0,
    ColContent = 1,
    ColDescription = 2,
    ColPriority = 3,
    ColIndent = 4,       // сохраняется в модели не будет - см. заголовок CsvImporter.h
    ColDate = 5,
    ColDateLang = 6,     // не используется при разборе - "every ... at ..." всегда на этом языке-шаблоне
    ColTimezone = 7,
    ColDuration = 8,
    ColDurationUnit = 9
};

const QMap<QString, int> &weekdayNameToIso()
{
    // 1=Пн..7=Вс - как QDate::dayOfWeek(), чтобы напрямую совпадать
    // с тем, что ожидает eventOccursOnDate() для RecurrenceType::Weekly.
    static const QMap<QString, int> names = {
        {"monday", 1}, {"tuesday", 2}, {"wednesday", 3}, {"thursday", 4},
        {"friday", 5}, {"saturday", 6}, {"sunday", 7}
    };
    return names;
}
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

CsvImporter::Format CsvImporter::detectFormat(const QStringList &headerFields)
{
    QStringList normalized;
    normalized.reserve(headerFields.size());
    for (const QString &field : headerFields)
        normalized.append(field.trimmed().toLower());

    static const QStringList kSimpleHeader = {"date", "start", "end", "subject", "description"};
    static const QStringList kWeeklyHeader = {
        "type", "content", "description", "priority", "indent",
        "date", "date_lang", "timezone", "duration", "duration_unit"
    };

    if (normalized == kSimpleHeader)
        return Format::Simple;
    if (normalized == kWeeklyHeader)
        return Format::WeeklySchedule;
    return Format::Unknown;
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
    if (rows.isEmpty())
        return result;

    const Format format = detectFormat(rows.first());
    if (format == Format::Unknown) {
        result.unsupportedFormat = true;
        return result;
    }

    for (int i = 1; i < rows.size(); ++i) { // с 1 - заголовок (rows[0]) уже использован для detectFormat
        const QStringList &fields = rows[i];
        const int recordNumber = i + 1; // нумерация "как в файле": заголовок - строка 1

        const bool isBlankRow = (fields.size() == 1 && fields.first().trimmed().isEmpty());
        if (isBlankRow)
            continue;

        if (format == Format::Simple) {
            result.totalDataRows++;
            Event event;
            QString error;
            if (parseSimpleRow(fields, recordNumber, event, error))
                result.validEvents.append(event);
            else
                result.errors.append(error);
            continue;
        }

        // Format::WeeklySchedule
        const QString type = fields.value(ColType).trimmed().toLower();
        if (type == QLatin1String("section"))
            continue; // группировка ("Понедельник" и т.п.) - не событие, пропускаем молча

        result.totalDataRows++;
        Event event;
        QString error;
        if (parseWeeklyTaskRow(fields, recordNumber, event, error))
            result.validEvents.append(event);
        else
            result.errors.append(error);
    }

    return result;
}

bool CsvImporter::parseSimpleRow(const QStringList &fields, int recordNumber, Event &outEvent, QString &outError)
{
    if (fields.size() != kSimpleColumnCount) {
        outError = QString("Строка %1: ожидалось %2 столбцов, найдено %3")
            .arg(recordNumber)
            .arg(kSimpleColumnCount)
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

bool CsvImporter::parseEveryWeekdayAt(const QString &raw, int &outIsoWeekday, QTime &outTime)
{
    static const QRegularExpression re(
        QStringLiteral(R"(^\s*every\s+([a-zA-Z]+)\s+at\s+(\d{1,2}):(\d{2})\s*$)"),
        QRegularExpression::CaseInsensitiveOption
    );

    const QRegularExpressionMatch match = re.match(raw);
    if (!match.hasMatch())
        return false;

    const QString weekdayName = match.captured(1).toLower();
    const QMap<QString, int> &names = weekdayNameToIso();
    if (!names.contains(weekdayName))
        return false;
    outIsoWeekday = names.value(weekdayName);

    bool hourOk = false;
    bool minuteOk = false;
    const int hour = match.captured(2).toInt(&hourOk);
    const int minute = match.captured(3).toInt(&minuteOk);
    if (!hourOk || !minuteOk || hour < 0 || hour > 23 || minute < 0 || minute > 59)
        return false;

    outTime = QTime(hour, minute);
    return true;
}

bool CsvImporter::parseDurationMinutes(const QString &durationStr, const QString &unitStr, int &outMinutes)
{
    bool ok = false;
    // toDouble(), а не toInt(): в исходном файле есть значения вида "1,5"
    // (протестовано на примере "Профмат,1,5 ч" - но эта запятая внутри
    // DURATION в реальном файле пользователя не встречается, DURATION там
    // всегда целое число минут; toDouble() просто на всякий случай не упадёт
    // и на дробном значении часов, если оно встретится).
    const double value = durationStr.trimmed().toDouble(&ok);
    if (!ok || value <= 0)
        return false;

    const QString unit = unitStr.trimmed().toLower();
    double minutes = value;
    if (unit.startsWith(QLatin1String("hour")))
        minutes = value * 60.0;
    else if (unit.startsWith(QLatin1String("minute")))
        minutes = value;
    else if (!unit.isEmpty())
        return false; // неизвестная единица измерения - лучше явная ошибка, чем угадывать

    outMinutes = qRound(minutes);
    return outMinutes > 0;
}

QDate CsvImporter::anchorDateForWeekday(int isoWeekday)
{
    const QDate today = QDate::currentDate();
    int diff = isoWeekday - today.dayOfWeek();
    if (diff < 0)
        diff += 7;
    return today.addDays(diff);
}

bool CsvImporter::parseWeeklyTaskRow(const QStringList &fields, int recordNumber, Event &outEvent, QString &outError)
{
    QStringList normalizedFields = fields;

    if (normalizedFields.size() == kWeeklyColumnCount + 1) {
        // Частый случай в реальных экспортах: DESCRIPTION вида "1,5 ч"
        // (русская десятичная запятая, например "1,5 часа") записан в
        // исходном файле БЕЗ кавычек - формально это невалидный CSV
        // (запятая внутри поля обязана быть в кавычках), и честный разбор
        // режет такое поле на два. Это не ошибка парсера - строка на входе
        // сама по себе неоднозначна без кавычек.
        //
        // Восстанавливаем только тогда, когда это действительно похоже
        // именно на этот случай: если склеить DESCRIPTION (индекс 2) со
        // следующим полем через запятую, PRIORITY и DATE должны встать
        // на свои места и быть корректными (число и распознаваемое
        // "every ... at ..."). Если это не так - не гадаем, а сообщаем
        // обычную ошибку "неверное число столбцов" ниже.
        QStringList recovered = normalizedFields;
        recovered[ColDescription] = recovered[ColDescription] + "," + recovered[ColDescription + 1];
        recovered.removeAt(ColDescription + 1);

        bool priorityLooksValid = false;
        recovered.value(ColPriority).trimmed().toInt(&priorityLooksValid);

        int dummyWeekday = 0;
        QTime dummyTime;
        const bool dateLooksValid = parseEveryWeekdayAt(recovered.value(ColDate).trimmed(), dummyWeekday, dummyTime);

        if (recovered.size() == kWeeklyColumnCount && priorityLooksValid && dateLooksValid)
            normalizedFields = recovered;
    }

    if (normalizedFields.size() != kWeeklyColumnCount) {
        outError = QString("Строка %1: ожидалось %2 столбцов, найдено %3")
            .arg(recordNumber)
            .arg(kWeeklyColumnCount)
            .arg(normalizedFields.size());
        return false;
    }

    const QString content = normalizedFields.value(ColContent).trimmed();
    if (content.isEmpty()) {
        outError = QString("Строка %1: не указано название задачи (CONTENT)").arg(recordNumber);
        return false;
    }

    const QString dateRule = normalizedFields.value(ColDate).trimmed();
    int isoWeekday = 0;
    QTime startTime;
    if (!parseEveryWeekdayAt(dateRule, isoWeekday, startTime)) {
        outError = QString("Строка %1: не удалось разобрать правило даты \"%2\" "
                            "(ожидается \"every <день недели> at HH:MM\")")
            .arg(recordNumber)
            .arg(dateRule);
        return false;
    }

    const QString durationStr = normalizedFields.value(ColDuration).trimmed();
    const QString durationUnitStr = normalizedFields.value(ColDurationUnit).trimmed();
    int durationMinutes = 0;
    if (!parseDurationMinutes(durationStr, durationUnitStr, durationMinutes)) {
        outError = QString("Строка %1: некорректная продолжительность \"%2 %3\"")
            .arg(recordNumber)
            .arg(durationStr, durationUnitStr);
        return false;
    }

    const int startTotalMinutes = startTime.hour() * 60 + startTime.minute();
    const int endTotalMinutes = startTotalMinutes + durationMinutes;
    if (endTotalMinutes > 24 * 60) {
        outError = QString("Строка %1: событие переходит через полночь - пока не поддерживается")
            .arg(recordNumber);
        return false;
    }
    const QTime endTime = (endTotalMinutes == 24 * 60)
        ? QTime(23, 59, 59) // ровно полночь как endTime не выразить QTime - прижимаем к концу суток
        : QTime(endTotalMinutes / 60, endTotalMinutes % 60);

    bool priorityOk = false;
    const int priority = normalizedFields.value(ColPriority).trimmed().toInt(&priorityOk);

    outEvent.title = content;
    outEvent.description = normalizedFields.value(ColDescription).trimmed();
    outEvent.date = anchorDateForWeekday(isoWeekday);
    outEvent.startTime = startTime;
    outEvent.endTime = endTime;
    outEvent.recurrenceType = RecurrenceType::Weekly;
    outEvent.recurrenceInterval = 1;
    outEvent.recurrenceEndDate = QDate(); // без окончания - повторяется всегда
    outEvent.priority = priorityOk ? priority : 0;
    outEvent.timezone = normalizedFields.value(ColTimezone).trimmed();

    return true;
}
