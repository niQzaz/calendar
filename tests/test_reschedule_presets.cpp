// Тесты для services/ReschedulePresets.h (Этап C) - быстрые варианты
// переноса задачи.
//
// Чистая логика без БД и UI, поэтому здесь проверяются именно границы:
// округление вверх, полночь, влезание задачи в день и переход через
// границу месяца/года для "Tomorrow". Сам диалог автоматическими тестами
// не покрыт - только ручная проверка.

#include <QtTest>

#include "services/ReschedulePresets.h"

class TestReschedulePresets : public QObject
{
    Q_OBJECT

private:
    static QDateTime at(int hour, int minute, int second = 0)
    {
        return QDateTime(QDate(2026, 10, 14), QTime(hour, minute, second));
    }

    static RescheduleTarget compute(ReschedulePreset preset,
                                    const QTime &start, const QTime &end,
                                    const QDateTime &now)
    {
        return computePresetTarget(preset, start, end, now);
    }

private slots:
    // --- In15Min / In30Min / InOneHour ---
    void in15_roundsStartUpToFiveMinutes();
    void in15_exactFiveMinuteBoundaryStays();
    void in15_secondsPushToNextBoundary();
    void in30_and_in60_addOffset();
    void relative_keepsDuration();
    void relative_fitsUpToLastMinuteOfDay();
    void relative_notFittingIsInvalid();
    void relative_crossingMidnightIsInvalid();

    // --- LaterToday ---
    void laterToday_addsTwoHoursAndRoundsUpToFullHour();
    void laterToday_exactHourStays();
    void laterToday_oneSecondPastHourRoundsUp();
    void laterToday_fitsWhenTaskEndsByLastMinute();
    void laterToday_disabledWhenTaskDoesNotFitInDay();
    void laterToday_disabledWhenRoundingCrossesMidnight();

    // --- Tomorrow ---
    void tomorrow_keepsTimesAndMovesDate();
    void tomorrow_crossesMonthAndYearBoundary();
    void tomorrow_isRelativeToNowNotToOriginalDate();

    // --- Общее ---
    void invalidOriginalSlotIsInvalidForAllPresets();
};

// ---------------------------------------------------------------------------
// In15Min / In30Min / InOneHour
// ---------------------------------------------------------------------------

void TestReschedulePresets::in15_roundsStartUpToFiveMinutes()
{
    // 10:03 + 15 = 10:18 -> вверх до 10:20
    const RescheduleTarget target = compute(ReschedulePreset::In15Min, QTime(9, 0), QTime(10, 0), at(10, 3));
    QVERIFY(target.valid);
    QCOMPARE(target.date, QDate(2026, 10, 14));
    QCOMPARE(target.start, QTime(10, 20));
    QCOMPARE(target.end, QTime(11, 20));
}

void TestReschedulePresets::in15_exactFiveMinuteBoundaryStays()
{
    // 10:00:00 + 15 = 10:15:00 - ровно на границе, не уходит на 10:20
    const RescheduleTarget target = compute(ReschedulePreset::In15Min, QTime(9, 0), QTime(10, 0), at(10, 0));
    QVERIFY(target.valid);
    QCOMPARE(target.start, QTime(10, 15));
}

void TestReschedulePresets::in15_secondsPushToNextBoundary()
{
    // 10:02:30 + 15 = 10:17:30 -> 10:20
    const RescheduleTarget target = compute(ReschedulePreset::In15Min, QTime(9, 0), QTime(10, 0), at(10, 2, 30));
    QVERIFY(target.valid);
    QCOMPARE(target.start, QTime(10, 20));

    // 10:00:30 + 15 = 10:15:30 - превышение границы на 30 секунд -> 10:20
    const RescheduleTarget target2 = compute(ReschedulePreset::In15Min, QTime(9, 0), QTime(10, 0), at(10, 0, 30));
    QVERIFY(target2.valid);
    QCOMPARE(target2.start, QTime(10, 20));
}

void TestReschedulePresets::in30_and_in60_addOffset()
{
    const RescheduleTarget in30 = compute(ReschedulePreset::In30Min, QTime(9, 0), QTime(10, 0), at(14, 0));
    QVERIFY(in30.valid);
    QCOMPARE(in30.start, QTime(14, 30));

    const RescheduleTarget in60 = compute(ReschedulePreset::InOneHour, QTime(9, 0), QTime(10, 0), at(14, 0));
    QVERIFY(in60.valid);
    QCOMPARE(in60.start, QTime(15, 0));
}

void TestReschedulePresets::relative_keepsDuration()
{
    // исходная длительность 1 ч 30 мин
    const RescheduleTarget target = compute(ReschedulePreset::In30Min, QTime(17, 30), QTime(19, 0), at(12, 0));
    QVERIFY(target.valid);
    QCOMPARE(target.start, QTime(12, 30));
    QCOMPARE(target.end, QTime(14, 0));
}

void TestReschedulePresets::relative_fitsUpToLastMinuteOfDay()
{
    // 23:15 + 15 = 23:30, длительность 29 мин -> конец ровно 23:59 - допустимо
    const RescheduleTarget fits = compute(ReschedulePreset::In15Min, QTime(10, 0), QTime(10, 29), at(23, 15));
    QVERIFY(fits.valid);
    QCOMPARE(fits.start, QTime(23, 30));
    QCOMPARE(fits.end, QTime(23, 59));
}

void TestReschedulePresets::relative_notFittingIsInvalid()
{
    // то же начало 23:30, но длительность 30 мин -> конец 00:00 следующего дня
    const RescheduleTarget target = compute(ReschedulePreset::In15Min, QTime(10, 0), QTime(10, 30), at(23, 15));
    QVERIFY(!target.valid);
}

void TestReschedulePresets::relative_crossingMidnightIsInvalid()
{
    // 23:50 + 15 = 00:05 следующего дня
    QVERIFY(!compute(ReschedulePreset::In15Min, QTime(9, 0), QTime(9, 10), at(23, 50)).valid);
    // 23:50 + 30 и + 60 - тем более
    QVERIFY(!compute(ReschedulePreset::In30Min, QTime(9, 0), QTime(9, 10), at(23, 50)).valid);
    QVERIFY(!compute(ReschedulePreset::InOneHour, QTime(9, 0), QTime(9, 10), at(23, 50)).valid);

    // 23:46 + 15 = 00:01 следующего дня
    QVERIFY(!compute(ReschedulePreset::In15Min, QTime(9, 0), QTime(9, 10), at(23, 46)).valid);

    // Округление вверх вытолкнуло за полночь: 23:43 + 15 = 23:58 -> 00:00
    QVERIFY(!compute(ReschedulePreset::In15Min, QTime(9, 0), QTime(9, 10), at(23, 43)).valid);
}

// ---------------------------------------------------------------------------
// LaterToday
// ---------------------------------------------------------------------------

void TestReschedulePresets::laterToday_addsTwoHoursAndRoundsUpToFullHour()
{
    // 14:20 + 2 ч = 16:20 -> 17:00
    const RescheduleTarget target = compute(ReschedulePreset::LaterToday, QTime(9, 0), QTime(10, 0), at(14, 20));
    QVERIFY(target.valid);
    QCOMPARE(target.date, QDate(2026, 10, 14));
    QCOMPARE(target.start, QTime(17, 0));
    QCOMPARE(target.end, QTime(18, 0));
}

void TestReschedulePresets::laterToday_exactHourStays()
{
    // 14:00:00 + 2 ч = 16:00:00 - уже полный час
    const RescheduleTarget target = compute(ReschedulePreset::LaterToday, QTime(9, 0), QTime(10, 0), at(14, 0));
    QVERIFY(target.valid);
    QCOMPARE(target.start, QTime(16, 0));
}

void TestReschedulePresets::laterToday_oneSecondPastHourRoundsUp()
{
    // 14:00:01 + 2 ч = 16:00:01 -> 17:00
    const RescheduleTarget target = compute(ReschedulePreset::LaterToday, QTime(9, 0), QTime(10, 0), at(14, 0, 1));
    QVERIFY(target.valid);
    QCOMPARE(target.start, QTime(17, 0));
}

void TestReschedulePresets::laterToday_fitsWhenTaskEndsByLastMinute()
{
    // 21:00 + 2 ч = 23:00, длительность 59 мин -> конец ровно 23:59
    const RescheduleTarget fits = compute(ReschedulePreset::LaterToday, QTime(9, 0), QTime(9, 59), at(21, 0));
    QVERIFY(fits.valid);
    QCOMPARE(fits.start, QTime(23, 0));
    QCOMPARE(fits.end, QTime(23, 59));
}

void TestReschedulePresets::laterToday_disabledWhenTaskDoesNotFitInDay()
{
    // 21:00 + 2 ч = 23:00, длительность 60 мин -> конец 00:00 следующего дня
    QVERIFY(!compute(ReschedulePreset::LaterToday, QTime(9, 0), QTime(10, 0), at(21, 0)).valid);

    // 20:30 + 2 ч = 22:30 -> 23:00, длительность 60 мин - тоже не помещается
    QVERIFY(!compute(ReschedulePreset::LaterToday, QTime(9, 0), QTime(10, 0), at(20, 30)).valid);

    // а вот 20:00 + 2 ч = 22:00, длительность 60 мин -> 23:00 - помещается
    const RescheduleTarget fits = compute(ReschedulePreset::LaterToday, QTime(9, 0), QTime(10, 0), at(20, 0));
    QVERIFY(fits.valid);
    QCOMPARE(fits.start, QTime(22, 0));
    QCOMPARE(fits.end, QTime(23, 0));

    // длительность 2 ч от 22:00 -> конец 00:00 - не помещается
    QVERIFY(!compute(ReschedulePreset::LaterToday, QTime(9, 0), QTime(11, 0), at(20, 0)).valid);
}

void TestReschedulePresets::laterToday_disabledWhenRoundingCrossesMidnight()
{
    // 21:50 + 2 ч = 23:50 -> округление вверх даёт 00:00 следующего дня
    QVERIFY(!compute(ReschedulePreset::LaterToday, QTime(9, 0), QTime(9, 10), at(21, 50)).valid);

    // 22:30 + 2 ч = 00:30 следующего дня - уже за полночью до округления
    QVERIFY(!compute(ReschedulePreset::LaterToday, QTime(9, 0), QTime(9, 10), at(22, 30)).valid);
}

// ---------------------------------------------------------------------------
// Tomorrow
// ---------------------------------------------------------------------------

void TestReschedulePresets::tomorrow_keepsTimesAndMovesDate()
{
    const RescheduleTarget target = compute(ReschedulePreset::Tomorrow, QTime(17, 30), QTime(19, 0), at(23, 59));
    QVERIFY(target.valid);
    QCOMPARE(target.date, QDate(2026, 10, 15));
    QCOMPARE(target.start, QTime(17, 30));
    QCOMPARE(target.end, QTime(19, 0));
}

void TestReschedulePresets::tomorrow_crossesMonthAndYearBoundary()
{
    const QDateTime newYearsEve(QDate(2026, 12, 31), QTime(18, 0));
    const RescheduleTarget year = compute(ReschedulePreset::Tomorrow, QTime(9, 0), QTime(10, 0), newYearsEve);
    QVERIFY(year.valid);
    QCOMPARE(year.date, QDate(2027, 1, 1));

    const QDateTime monthEnd(QDate(2026, 9, 30), QTime(8, 0));
    const RescheduleTarget month = compute(ReschedulePreset::Tomorrow, QTime(9, 0), QTime(10, 0), monthEnd);
    QVERIFY(month.valid);
    QCOMPARE(month.date, QDate(2026, 10, 1));
}

void TestReschedulePresets::tomorrow_isRelativeToNowNotToOriginalDate()
{
    // Пресет не знает дату вхождения вообще - только now. Для пропущенной
    // вчерашней задачи "Tomorrow" - это завтра от СЕГОДНЯ.
    const RescheduleTarget target = compute(ReschedulePreset::Tomorrow, QTime(9, 0), QTime(10, 0), at(12, 0));
    QCOMPARE(target.date, at(12, 0).date().addDays(1));
}

// ---------------------------------------------------------------------------
// Общее
// ---------------------------------------------------------------------------

void TestReschedulePresets::invalidOriginalSlotIsInvalidForAllPresets()
{
    const QDateTime now = at(10, 0);
    const ReschedulePreset presets[] = {
        ReschedulePreset::In15Min, ReschedulePreset::In30Min, ReschedulePreset::InOneHour,
        ReschedulePreset::LaterToday, ReschedulePreset::Tomorrow
    };

    for (ReschedulePreset preset : presets) {
        // конец равен началу
        QVERIFY(!compute(preset, QTime(9, 0), QTime(9, 0), now).valid);
        // конец раньше начала
        QVERIFY(!compute(preset, QTime(10, 0), QTime(9, 0), now).valid);
        // невалидное время
        QVERIFY(!compute(preset, QTime(), QTime(9, 0), now).valid);
    }
}

QTEST_MAIN(TestReschedulePresets)
#include "test_reschedule_presets.moc"
