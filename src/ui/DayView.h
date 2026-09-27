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

// Day View (Phase F) - временная сетка на один день.
//
// Переиспользует тот же TimeGridView, что и WeekView (Phase E) - просто
// с одной колонкой вместо семи. Вся отрисовка/зум/клики/создание/выбор
// событий уже реализованы там один раз - здесь только навигация по дням
// (а не неделям) и заголовок вида "Wednesday, 16 September 2026".
class DayView : public QWidget, public ICalendarPage
{
    Q_OBJECT

public:
    explicit DayView(QWidget *parent = nullptr);

    // --- ICalendarPage ---
    void goToPrevious() override;
    void goToNext() override;
    void goToToday() override;
    QString headerTitle() const override;

    void setEventsForVisibleRange(const QMap<QDate, QVector<Event>> &eventsByDate);
    void setTheme(const Theme &theme);

    QDate currentDate() const { return m_date; }

signals:
    void createEventRequested(const QDate &date, const QTime &time);
    void editEventRequested(int eventId);

    // Событие перетащили мышью на новое время (MVP2, drag & drop) -
    // просто ретранслируется из TimeGridView, см. его комментарий к сигналу.
    void eventRescheduled(int eventId, const QDate &newDate, const QTime &newStartTime, const QTime &newEndTime);

    // Для Day View start == end == currentDate() - сигнал всё равно нужен,
    // чтобы MainWindow знал, когда подгружать события заново (навигация
    // по дням меняет currentDate()).
    void visibleRangeChanged(const QDate &start, const QDate &end);

private:
    void rebuildColumnDate();

    QDate m_date;

    QScrollArea *m_scrollArea = nullptr;
    TimeGridView *m_grid = nullptr;
};
