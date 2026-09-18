#pragma once

#include <QWidget>
#include <QDate>
#include <QSet>
#include <QVector>

class QLabel;
class QPushButton;
class QGridLayout;

// CalendarWidget - самодельная сетка месяца (не встроенный QCalendarWidget).
//
// Почему не QCalendarWidget?
// QCalendarWidget - готовый виджет "всё в одном", но плохо поддаётся
// кастомизации: сложно перекрасить конкретные ячейки (пометить дни
// с событиями) и сложно сделать свой заголовок под макет (стрелки + Today
// в одну строку). Для учебного проекта проще сделать сетку из QPushButton
// самим - логика "какой сегодня первый день недели" и "сколько дней
// в месяце" станет наглядной, а не спрятанной внутри библиотечного класса.
//
// Как устроена сетка:
// - 7 колонок (Mon..Sun) x 6 строк = 42 ячейки. 6 строк с запасом,
//   т.к. при некоторых комбинациях "первый день недели" + "число дней
//   в месяце" месяц может занять 6 календарных строк (например, Пн 1 марта
//   2027 + 31 день).
// - Первый день месяца - QDate(year, month, 1).
// - QDate::dayOfWeek() для него (1=Пн..7=Вс) говорит, сколько "чужих"
//   ячеек нужно оставить в начале сетки для хвоста предыдущего месяца.
// - Дата любой ячейки вычисляется одной формулой:
//   firstOfMonth.addDays(dayNumber - 1), где dayNumber - номер дня
//   относительно 1-го числа месяца (может быть <=0 для предыдущего
//   месяца или больше daysInMonth() для следующего). QDate сам корректно
//   переносит дату через границу месяца/года - отдельных ветвлений
//   для "прошлый/следующий месяц" не требуется.
class CalendarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CalendarWidget(QWidget *parent = nullptr);

    // Текущая выбранная дата.
    QDate selectedDate() const { return m_selectedDate; }

    // Первая и последняя дата, видимые сейчас в сетке (42 ячейки, могут
    // относиться к соседним месяцам). Нужны, чтобы запросить у EventManager
    // только даты в этом диапазоне - см. комментарий у EventManager::datesWithEvents.
    QDate visibleRangeStart() const { return m_cellDates.isEmpty() ? QDate() : m_cellDates.first(); }
    QDate visibleRangeEnd() const { return m_cellDates.isEmpty() ? QDate() : m_cellDates.last(); }

    // Обновляет набор дат, для которых показывается маркер "есть события".
    void setDatesWithEvents(const QSet<QDate> &dates);

public slots:
    void goToPreviousMonth();
    void goToNextMonth();
    void goToToday();
    void setSelectedDate(const QDate &date);

signals:
    // Испускается при выборе пользователем дня в сетке
    // (в т.ч. при программном вызове setSelectedDate/goToToday).
    void dateSelected(const QDate &date);

    // Испускается при любом перестроении сетки (смена месяца, выбор дня) -
    // видимый диапазон дат мог измениться.
    void visibleRangeChanged(const QDate &start, const QDate &end);

private:
    void rebuildGrid();
    void updateHeaderLabel();
    void onDayButtonClicked(int cellIndex);

    int m_year;
    int m_month; // 1-12
    QDate m_selectedDate;
    QSet<QDate> m_datesWithEvents;

    QLabel *m_monthLabel = nullptr;
    QGridLayout *m_gridLayout = nullptr;
    QVector<QPushButton *> m_dayButtons; // 42 кнопки сетки, создаются один раз
    QVector<QDate> m_cellDates;          // дата, соответствующая каждой кнопке
};
