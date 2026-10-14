#include "ReschedulePresets.h"

namespace {

constexpr int kMinutesPerDay = 24 * 60;
constexpr int kLastMinuteOfDay = kMinutesPerDay - 1; // 23:59

int minutesOfDay(const QTime &time)
{
    return time.hour() * 60 + time.minute();
}

int ceilToMultiple(int value, int step)
{
    return ((value + step - 1) / step) * step;
}

// Общая часть "через N минут / позже сегодня": base = now + offset,
// начало = base, округлённое вверх до stepMinutes, внутри ТОГО ЖЕ дня, что и now.
RescheduleTarget relativeTarget(const QDateTime &now, int offsetMinutes, int stepMinutes, int durationMinutes)
{
    RescheduleTarget result; // valid = false

    const QDateTime base = now.addSecs(static_cast<qint64>(offsetMinutes) * 60);
    if (base.date() != now.date())
        return result; // уже перевалило за полночь

    int secondsOfDay = QTime(0, 0).secsTo(base.time());
    if (base.time().msec() > 0)
        ++secondsOfDay; // любые доли секунды считаем превышением границы

    const int stepSeconds = stepMinutes * 60;
    const int startMinutes = ceilToMultiple(secondsOfDay, stepSeconds) / 60;
    if (startMinutes > kLastMinuteOfDay)
        return result; // округление вверх вытолкнуло за полночь

    const int endMinutes = startMinutes + durationMinutes;
    if (endMinutes > kLastMinuteOfDay)
        return result; // задача не помещается в сегодняшний день

    result.valid = true;
    result.date = now.date();
    result.start = QTime(startMinutes / 60, startMinutes % 60);
    result.end = QTime(endMinutes / 60, endMinutes % 60);
    return result;
}

} // namespace

RescheduleTarget computePresetTarget(ReschedulePreset preset,
                                     const QTime &origStart,
                                     const QTime &origEnd,
                                     const QDateTime &now)
{
    RescheduleTarget invalid;

    if (!origStart.isValid() || !origEnd.isValid() || !now.isValid())
        return invalid;

    const int durationMinutes = minutesOfDay(origEnd) - minutesOfDay(origStart);
    if (durationMinutes <= 0)
        return invalid;

    switch (preset) {
    case ReschedulePreset::In15Min:
        return relativeTarget(now, 15, 5, durationMinutes);
    case ReschedulePreset::In30Min:
        return relativeTarget(now, 30, 5, durationMinutes);
    case ReschedulePreset::InOneHour:
        return relativeTarget(now, 60, 5, durationMinutes);
    case ReschedulePreset::LaterToday:
        return relativeTarget(now, 120, 60, durationMinutes);
    case ReschedulePreset::Tomorrow: {
        RescheduleTarget result;
        result.valid = true;
        result.date = now.date().addDays(1);
        result.start = origStart;
        result.end = origEnd;
        return result;
    }
    }

    return invalid;
}
