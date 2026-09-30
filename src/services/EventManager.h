#pragma once

#include "models/Event.h"
#include "models/Category.h"
#include "models/Execution.h"

#include <QVector>
#include <QSet>
#include <QDate>
#include <QString>

// EventManager отвечает за хранение событий в SQLite.
//
// В Этапе 1 (MVP) события хранились в QVector в оперативной памяти.
// Публичный интерфейс класса почти не изменился (addEvent/removeEvent/
// eventsForDate/datesWithEvents остались) - поменялась только реализация
// внутри .cpp. UI (MainWindow) от этого почти не пострадал - именно
// ради этого EventManager был с самого начала вынесен в отдельный
// класс, а не размазан по MainWindow.
//
// Добавились:
//  - updateEvent() - для редактирования существующего события;
//  - eventById()   - нужен диалогу редактирования, чтобы получить
//                     полные данные события по его id из списка.
class EventManager
{
public:
    // Открывает (или создаёт, если его ещё нет) файл базы данных
    // и создаёт таблицу events, если она отсутствует.
    EventManager();

    // То же самое, но с явным путём к файлу БД и именем подключения Qt SQL -
    // используется тестами (например, ":memory:" + уникальное имя на каждый
    // тест), чтобы не трогать реальную пользовательскую базу и не пересекаться
    // с другими открытыми соединениями. Обычный конструктор выше просто
    // вызывает этот с вычисленным путём по умолчанию и именем подключения
    // по умолчанию - поведение для существующих вызывающих не меняется.
    EventManager(const QString &databasePath, const QString &connectionName);

    ~EventManager();

    // EventManager владеет соединением с БД (именованным подключением
    // Qt SQL) - это ресурс, копировать его не нужно и небезопасно,
    // поэтому явно запрещаем копирование (тот же принцип, что у RAII-обёрток
    // вроде std::unique_ptr).
    EventManager(const EventManager &) = delete;
    EventManager &operator=(const EventManager &) = delete;

    // Добавляет событие, возвращает присвоенный базой id.
    // Поле event.id при этом игнорируется. При ошибке возвращает -1.
    int addEvent(const Event &event);

    // Обновляет событие с идентификатором event.id. true при успехе.
    bool updateEvent(const Event &event);

    // Удаляет событие по id. true, если событие было найдено и удалено.
    bool removeEvent(int id);

    // Находит событие по id. Возвращает true и заполняет outEvent, если найдено.
    bool eventById(int id, Event &outEvent) const;

    // Увеличивает счётчик завершённых pomodoro для события на 1.
    bool incrementPomodoroCount(int id);

    // Все события на дату, отсортированные по времени начала.
    QVector<Event> eventsForDate(const QDate &date) const;

    // Даты в диапазоне [rangeStart, rangeEnd], на которые есть хотя бы одно
    // событие (обычное или повторяющееся). Диапазон обязателен: у повторяющегося
    // события без даты окончания "все даты, на которые оно есть" - бесконечное
    // множество, поэтому считаем только для конкретного видимого диапазона
    // (используется MonthView, чтобы пометить такие дни в сетке месяца).
    QSet<QDate> datesWithEvents(const QDate &rangeStart, const QDate &rangeEnd) const;

    // Все события (обычные и повторяющиеся) в диапазоне [rangeStart, rangeEnd],
    // каждое с полем date, выставленным в конкретную дату вхождения -
    // используется MonthView для отрисовки мини-карточек событий внутри ячеек,
    // а в будущем и Week/Day View. Та же идея, что у eventsForDate(), только
    // сразу на диапазон дат, а не на один день.
    QVector<Event> eventsInRange(const QDate &rangeStart, const QDate &rangeEnd) const;

    // --- Категории (MVP2) ---
    // Отдельная таблица categories - см. models/Category.h. Event хранит
    // только category_id (внешний ключ, без принудительного FK-контроля
    // средствами SQLite - в остальной схеме их тоже нет); имя и цвет
    // Event получает через LEFT JOIN при чтении, а не хранит у себя.

    // Добавляет категорию, возвращает присвоенный базой id. -1 при ошибке.
    int addCategory(const Category &category);

    // Обновляет категорию с идентификатором category.id. true при успехе.
    bool updateCategory(const Category &category);

    // Удаляет категорию по id. У всех событий, ссылавшихся на неё,
    // category_id сбрасывается в "без категории" (а не событие целиком) -
    // потеря цвета категории не должна тихо удалять чужие события.
    bool removeCategory(int id);

    // Находит категорию по id. Возвращает true и заполняет outCategory, если найдена.
    bool categoryById(int id, Category &outCategory) const;

    // Все категории, отсортированные по имени - для выпадающих списков в UI.
    QVector<Category> allCategories() const;

    // --- Execution (MVP 3.0, Task Execution Foundation) ---
    // Учёт фактического выполнения ОДНОГО КОНКРЕТНОГО вхождения задачи -
    // см. models/Execution.h за ExecutionStatus/EventExecution и чистыми
    // функциями расчёта (resolveExecutionStatus, *DeviationSeconds).
    // Хранится в отдельной таблице event_executions, максимум по одной
    // строке на (event_id, occurrence_date) - для разового события это
    // всегда одна и та же дата; для повторяющегося - у каждого конкретного
    // вхождения своя независимая запись, создаётся лениво (только когда
    // пользователь реально нажал Start/Complete по этому вхождению),
    // НЕ заранее на каждое будущее вхождение серии.

    // Сохранённая execution-запись для вхождения, если она есть.
    // false, если по этому вхождению ещё не было никаких действий -
    // это НЕ ошибка, эффективный статус в таком случае - Planned/Missed,
    // см. effectiveStatus() ниже.
    bool executionForOccurrence(int eventId, const QDate &occurrenceDate, EventExecution &outExecution) const;

    // Отмечает вхождение начатым: status=Running, actualStart=startedAt.
    bool startOccurrence(int eventId, const QDate &occurrenceDate, const QDateTime &startedAt);

    // Отмечает вхождение завершённым: status=Completed. Если до этого было
    // startOccurrence() (actualStart уже сохранён) - фиксирует actualEnd
    // тоже ("Track actual time", раздел 4); если Complete нажали без
    // предварительного Start - actualStart/actualEnd остаются невалидными
    // ("Manual completion", раздел 4). Это не отдельная настройка - просто
    // то, нажимал ли пользователь Start до этого или нет.
    bool completeOccurrence(int eventId, const QDate &occurrenceDate, const QDateTime &completedAt);

    // Эффективный статус вхождения ПРЯМО СЕЙЧАС - объединяет сохранённое
    // состояние (если есть) с grace period и текущим моментом через
    // resolveExecutionStatus(). НЕ пишет ничего в БД - см. calendar_project_
    // context_v1.md, раздел 6 ("не делать автоматическое изменение
    // состояния только по таймеру UI"): вызывающий код может дёргать этот
    // метод сколько угодно раз для обновления отображения, база при этом
    // не трогается.
    ExecutionStatus effectiveStatus(int eventId, const QDate &occurrenceDate,
                                     const QTime &plannedStart, const QTime &plannedEnd,
                                     const QDateTime &now,
                                     int gracePeriodMinutes = kDefaultGracePeriodMinutes) const;

    // NOW screen - собирает события дня (обычно - сегодня) вместе с
    // резолвнутым статусом каждого и самой execution-записью (та же логика,
    // что в effectiveStatus(), но выполняется инлайн, а не через него -
    // чтобы заодно вернуть и EventExecution для расчёта deviation в UI,
    // не делая по ней ещё один отдельный запрос), и определяет текущую/
    // следующую задачу через findCurrentTask()/findNextTask() (models/
    // Execution.h - там же вся логика выбора, здесь только сборка данных).
    // НЕ пишет ничего в БД - то же чтение+вычисление, что и effectiveStatus().
    NowSnapshot nowSnapshot(const QDate &date, const QDateTime &now,
                             int gracePeriodMinutes = kDefaultGracePeriodMinutes) const;

private:
    // Загружает все события с recurrence_type != 'none' - их всегда немного
    // (это шаблоны, а не отдельные повторения), поэтому дальше с ними
    // работаем в памяти через eventOccursOnDate(), а не через SQL.
    QVector<Event> allRecurringTemplates() const;

    // INSERT OR REPLACE в event_executions по (event_id, occurrence_date) -
    // общая часть startOccurrence()/completeOccurrence(), у обоих одна
    // и та же операция "записать/перезаписать execution-запись целиком".
    bool upsertExecution(const EventExecution &execution);

    QString m_connectionName;
};
