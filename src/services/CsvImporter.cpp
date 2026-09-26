#include "CsvImporter.h"

#include <QFile>
#include <QStringDecoder>
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

// Таблица Windows-1251 -> Unicode для байтов 0x80-0xFF (0x00-0x7F в CP1251
// совпадают с ASCII, отдельная таблица для них не нужна). Байт 0x98 в самой
// CP1251 не занят ни одним символом - сюда положен U+FFFD (replacement
// character), чтобы не читать неопределённое поведение.
const char16_t kCp1251HighTable[128] = {
    0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021,
    0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
    0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0xFFFD, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
    0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7,
    0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
    0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7,
    0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457,
    0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417,
    0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F,
    0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427,
    0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F,
    0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437,
    0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F,
    0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447,
    0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F,
};

QString decodeWindows1251(const QByteArray &bytes)
{
    QString result;
    result.reserve(bytes.size());
    for (unsigned char byte : bytes) {
        if (byte < 0x80)
            result.append(QChar(byte));
        else
            result.append(QChar(kCp1251HighTable[byte - 0x80]));
    }
    return result;
}

// Определяет кодировку и декодирует содержимое файла в QString:
//  1) есть UTF-8 BOM -> однозначно UTF-8, BOM снимается;
//  2) весь файл - валидный UTF-8 -> UTF-8;
//  3) иначе -> Windows-1251 (частый случай для CSV, сохранённых на Windows
//     не через "UTF-8", например старыми версиями Excel/Google Таблиц).
// Qt6 больше не тянет QTextCodec "из коробки" (это отдельный модуль
// Core5Compat) - для одной конкретной кодовой страницы проще и легче
// декодировать вручную по таблице, чем тянуть весь модуль ради него одного.
QString decodeFileContent(const QByteArray &rawBytes)
{
    if (rawBytes.startsWith("\xEF\xBB\xBF"))
        return QString::fromUtf8(rawBytes.constData() + 3, rawBytes.size() - 3);

    QStringDecoder utf8Decoder(QStringDecoder::Utf8);
    const QString asUtf8 = utf8Decoder(rawBytes);
    if (!utf8Decoder.hasError())
        return asUtf8;

    return decodeWindows1251(rawBytes);
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
    if (!file.open(QIODevice::ReadOnly)) { // без QIODevice::Text - нужны сырые байты для определения кодировки
        result.fileOpenFailed = true;
        return result;
    }

    const QString content = decodeFileContent(file.readAll());

    const QVector<QStringList> rows = parseCsvContent(content);
    if (rows.isEmpty())
        return result;

    const Format format = detectFormat(rows.first());
    if (format == Format::Unknown) {
        result.unsupportedFormat = true;
        return result;
    }

    for (int i = 1; i < rows.size(); ++i) { // с 1 - заголовок (rows[0]) уже использован для detectFormat
        QStringList fields = rows[i];
        const int recordNumber = i + 1; // нумерация "как в файле": заголовок - строка 1

        // Fallback: некоторые генераторы CSV (замечено на выгрузках из
        // сторонних приложений на Windows) оборачивают ЦЕЛУЮ запись ещё
        // в одну пару внешних двойных кавычек поверх обычного CSV-экранирования:
        //   "task,""Душ, завтрак, сборы"",,4,2,..."
        // Корректный CSV-разбор (parseCsvContent) в этом случае видит всю
        // строку как ОДНО поле - но, поскольку экранированные "" внутри
        // него он уже раскрыл в обычные " по пути, это поле само по себе
        // оказывается уже вполне нормальной CSV-строкой:
        //   task,"Душ, завтрак, сборы",,4,2,...
        // Поэтому если строка распалась в одно поле, и это поле само
        // похоже на CSV-запись (содержит запятые), пробуем разобрать
        // его ещё раз тем же parseCsvContent() - без ручного снятия
        // кавычек и без отдельной логики экранирования.
        if (fields.size() == 1 && fields.first().contains(QLatin1Char(','))) {
            const QVector<QStringList> reparsed = parseCsvContent(fields.first());
            if (reparsed.size() == 1 && reparsed.first().size() > 1)
                fields = reparsed.first();
        }

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
