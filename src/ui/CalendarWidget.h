#pragma once

#include <QWidget>
#include <QDate>
#include <QMap>
#include <QVector>

#include "models/Event.h"

class QLabel;
class QGridLayout;
class MonthDayCell;

// CalendarWidget - месячная сетка (Month View).
//
// Как устроена сетка:
// - 7 колонок (Mon..Sun или Sun..Sat, см. setFirstDayOfWeek) x 6 строк = 42
//   ячейки. 6 строк с запасом, т.к. при некоторых комбинациях "первый день
//   недели" + "число дней в месяце" месяц может занять 6 календарных строк.
// - Первый день месяца - QDate(year, month, 1).
// - QDate::dayOfWeek() для него (1=Пн..7=Вс) говорит, сколько "чужих"
//   ячеек нужно оставить в начале сетки для хвоста предыдущего месяца.
// - Дата любой ячейки вычисляется одной формулой:
//   firstOfMonth.addDays(dayNumber - 1), где dayNumber - номер дня
//   относительно 1-го числа месяца (может быть <=0 для предыдущего
//   месяца или больше daysInMonth() для следующего). QDate сам корректно
//   переносит дату через границу месяца/года.
//
// Каждая ячейка - отдельный виджет MonthDayCell (см. ui/MonthDayCell.h),
// который сам отрисовывает номер дня и мини-карточки событий; CalendarWidget
// отвечает только за раскладку 42 ячеек в сетку и пересчёт того, какая дата
// должна быть в какой ячейке.
class CalendarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CalendarWidget(QWidget *parent = nullptr);

    // Текущая выбранная дата.
    QDate selectedDate() const { return m_selectedDate; }

    // Первая и последняя дата, видимые сейчас в сетке (42 ячейки, могут
    // относиться к соседним месяцам). Нужны, чтобы запросить у EventManager
    // только события в этом диапазоне - см. EventManager::eventsInRange.
    QDate visibleRangeStart() const { return m_cellDates.isEmpty() ? QDate() : m_cellDates.first(); }
    QDate visibleRangeEnd() const { return m_cellDates.isEmpty() ? QDate() : m_cellDates.last(); }

    // Передаёт события для отрисовки внутри ячеек. Ключ - дата, значение -
    // события этой даты (уже отсортированные по времени начала). Ячейки вне
    // переданного диапазона просто не найдут в карте своей даты и покажутся
    // без событий - вызывающий код должен передавать события хотя бы для
    // всего видимого диапазона (visibleRangeStart()..visibleRangeEnd()).
    void setEventsForVisibleRange(const QMap<QDate, QVector<Event>> &eventsByDate);

public slots:
    void goToPreviousMonth();
    void goToNextMonth();
    void goToToday();
    void setSelectedDate(const QDate &date);

    // Этап 8: false - неделя с понедельника (по умолчанию), true - с воскресенья.
    // Пересобирает и заголовок недели, и сетку дней.
    void setFirstDayOfWeek(bool sundayFirst);

signals:
    // Испускается при выборе пользователем дня в сетке
    // (в т.ч. при программном вызове setSelectedDate/goToToday).
    void dateSelected(const QDate &date);

    // Испускается при любом перестроении сетки (смена месяца, выбор дня) -
    // видимый диапазон дат мог измениться.
    void visibleRangeChanged(const QDate &start, const QDate &end);

    // Двойной клик по пустому месту в ячейке дня - запрос создать
    // событие на эту дату.
    void createEventRequested(const QDate &date);

    // Двойной клик по мини-карточке события внутри ячейки - запрос
    // открыть это событие на редактирование.
    void editEventRequested(int eventId);

private:
    void rebuildGrid();
    void updateHeaderLabel();
    void updateWeekDayLabels(); // текст меток Mon..Sun / Sun..Sat в зависимости от m_sundayFirst
    void applyEventsToVisibleCells(); // раздаёт m_eventsByDate по видимым ячейкам

    int m_year;
    int m_month; // 1-12
    QDate m_selectedDate;
    QMap<QDate, QVector<Event>> m_eventsByDate;
    bool m_sundayFirst = false; // false = неделя с понедельника

    QLabel *m_monthLabel = nullptr;
    QGridLayout *m_gridLayout = nullptr;
    QVector<QLabel *> m_weekDayLabels;     // 7 меток дней недели, создаются один раз
    QVector<MonthDayCell *> m_dayCells;    // 42 ячейки сетки, создаются один раз
    QVector<QDate> m_cellDates;            // дата, соответствующая каждой ячейке
};
