#pragma once

#include <QWidget>
#include <QDate>
#include <QMap>
#include <QVector>

#include "ICalendarPage.h"
#include "models/Event.h"
#include "services/Theme.h"

class QScrollArea;
class TimeGridView;

// Week View (Phase E) - временная сетка на 7 дней.
//
// Вся отрисовка/интерактивность сетки - в TimeGridView (общий движок,
// который в Phase F будет переиспользован и в DayView). WeekView отвечает
// только за то, какие 7 дат сейчас показывать, навигацию по неделям
// и скролл (через QScrollArea, в который помещён TimeGridView).
class WeekView : public QWidget, public ICalendarPage
{
    Q_OBJECT

public:
    explicit WeekView(QWidget *parent = nullptr);

    // --- ICalendarPage ---
    void goToPrevious() override;
    void goToNext() override;
    void goToToday() override;
    QString headerTitle() const override;

    // false - неделя с понедельника (по умолчанию), true - с воскресенья.
    void setFirstDayOfWeek(bool sundayFirst);

    void setEventsForVisibleRange(const QMap<QDate, QVector<Event>> &eventsByDate);
    void setTheme(const Theme &theme);

    // Stage B.1 - программно снять подсветку выбранного события в сетке.
    void clearSelection();

    QDate visibleRangeStart() const { return m_weekStart; }
    QDate visibleRangeEnd() const { return m_weekStart.addDays(6); }

signals:
    // Клик по пустому месту сетки - запрос создать событие на конкретные
    // дату и время (уже округлённое до 15 минут).
    void createEventRequested(const QDate &date, const QTime &time);

    // Двойной клик по событию - запрос открыть его на редактирование.
    void editEventRequested(int eventId);

    // Событие перетащили мышью на новое время/день (MVP2, drag & drop) -
    // просто ретранслируется из TimeGridView, см. его комментарий к сигналу.
    void eventRescheduled(int eventId, const QDate &newDate, const QTime &newStartTime, const QTime &newEndTime);

    // Выбор события кликом (Stage B) - просто ретранслируется из
    // TimeGridView, см. его комментарий к сигналу.
    void eventSelected(int eventId, const QDate &occurrenceDate);

    // Stage B.1 - клик по пустому месту сетки снял выделение события.
    void eventSelectionCleared();

    // Видимый диапазон дат сменился (навигация по неделям) -
    // MainWindow должен подгрузить события для нового диапазона.
    void visibleRangeChanged(const QDate &start, const QDate &end);

private:
    void rebuildColumnDates();

    QDate m_weekStart; // первый день отображаемой недели
    bool m_sundayFirst = false;

    QScrollArea *m_scrollArea = nullptr;
    TimeGridView *m_grid = nullptr;
};
