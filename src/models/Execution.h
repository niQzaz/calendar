#pragma once

#include <QString>
#include <QDate>
#include <QTime>
#include <QDateTime>

// MVP 3.0 - Task Execution Foundation.
//
// Статус фактического выполнения ОДНОГО КОНКРЕТНОГО вхождения задачи -
// для разового события это само событие; для повторяющегося - один
// конкретный день из серии (см. EventExecution ниже, почему это разные
// вещи и почему статус нельзя хранить прямо в строке events).
//
// Missed - вычисляемый статус (см. resolveExecutionStatus), а не то, что
// явно выставляет пользователь или таймер - см. calendar_project_context_v1.md,
// раздел 6 ("не делать автоматическое изменение состояния только по таймеру").
// Postponed/Cancelled зарезервированы под будущий шаг (reschedule/postpone
// counter, раздел 8) - на этом этапе ничего в приложении их ещё не
// выставляет, но хранение и раунд-трип строк уже поддержаны, чтобы потом
// не переделывать schema.
enum class ExecutionStatus
{
    Planned,
    Running,
    Completed,
    Missed,
    Postponed,
    Cancelled
};

QString executionStatusToString(ExecutionStatus status);
ExecutionStatus executionStatusFromString(const QString &value); // неизвестная/пустая строка -> Planned

// Начальное значение grace period (раздел 5). Используется как значение
// по умолчанию везде, где grace period нужен параметром - на этом этапе
// нигде не персистится как настройка пользователя (см. отчёт по MVP3.0,
// "сознательные ограничения"), просто константа.
constexpr int kDefaultGracePeriodMinutes = 60;

// Сохранённая (в таблице event_executions) запись о выполнении ОДНОГО
// конкретного вхождения. occurrenceDate - для разового события совпадает
// с Event::date; для повторяющегося - конкретная дата конкретного
// вхождения серии (та же дата, что EventManager подставляет в Event::date
// при разворачивании через eventsForDate()/eventsInRange()).
//
// Почему не хранить это прямо в строке events: строка events для
// повторяющегося события - это ОДИН шаблон на всю серию (см. Event.h).
// Если бы status/actualStart были полями events, "Start" одного
// понедельника пометил бы Running всю еженедельную серию сразу.
// Отдельная таблица с ключом (event_id, occurrence_date) даёт каждому
// вхождению независимое состояние, создаваемое лениво - только когда
// пользователь реально нажал Start/Complete по конкретному вхождению,
// а не заранее на каждое будущее вхождение серии.
//
// actualStart/actualEnd невалидны (QDateTime::isValid() == false), если
// ещё не зафиксированы - в том числе когда это "Manual completion"
// (раздел 4: Complete нажали без предварительного Start).
struct EventExecution
{
    int id = -1;
    int eventId = -1;
    QDate occurrenceDate;
    ExecutionStatus status = ExecutionStatus::Planned;
    QDateTime actualStart;
    QDateTime actualEnd;
};

// Чистая функция без обращения к БД: по плановому времени вхождения,
// сохранённому execution-состоянию (nullptr, если по вхождению ещё не
// было никаких действий) и текущему моменту определяет ЭФФЕКТИВНЫЙ статус.
//
// Явно сохранённое состояние (Running/Completed/Missed/Postponed/Cancelled)
// всегда побеждает и возвращается как есть - в частности, Running никогда
// не "затирается" в Missed только из-за того, что plannedEnd прошёл
// (раздел 5 - обязательное требование). Только когда сохранённого
// состояния нет вовсе, либо оно равно Planned (по сути "ничего не
// произошло"), применяется правило grace period: plannedEnd + grace < now.
ExecutionStatus resolveExecutionStatus(
    const QDate &occurrenceDate,
    const QTime &plannedStart,
    const QTime &plannedEnd,
    const EventExecution *savedExecution,
    const QDateTime &now,
    int gracePeriodMinutes = kDefaultGracePeriodMinutes
);

// Ниже - вычисление actual duration / deviation "по требованию" из
// timestamps (раздел 3: "не сохраняй вычисляемые значения без
// необходимости, если их можно надёжно вычислить"). Ничего из этого не
// хранится в БД. Все функции возвращают false, если нужных данных ещё
// нет (actualStart/actualEnd не зафиксированы) - в этом случае outSeconds
// не трогается; вызывающий код сам решает, как показать отсутствие данных
// (тот же принцип bool+out-параметр, что у eventById()/categoryById()).

// Фактическая длительность выполнения (actualEnd - actualStart), в секундах.
bool actualDurationSeconds(const EventExecution &execution, qint64 &outSeconds);

// Насколько фактическое начало отличается от планового, в секундах
// (положительное значение - начал позже плана, отрицательное - раньше).
bool startDeviationSeconds(const QDate &occurrenceDate, const QTime &plannedStart,
                            const EventExecution &execution, qint64 &outSeconds);

// Насколько фактическая длительность отличается от плановой, в секундах
// (положительное - выполнял дольше, чем планировал).
bool durationDeviationSeconds(const QTime &plannedStart, const QTime &plannedEnd,
                               const EventExecution &execution, qint64 &outSeconds);
