#pragma once

#include <QWidget>
#include <QDate>
#include <QTime>
#include <QDateTime>
#include <QMap>
#include <QVector>

#include "models/Event.h"
#include "models/Execution.h"
#include "services/Theme.h"

class QLabel;
class QPushButton;
class QStackedWidget;
class MonthView;
class WeekView;
class DayView;
class NowView;
class ICalendarPage;

// Какое из четырёх представлений сейчас показано.
enum class CalendarViewMode
{
    Now,
    Month,
    Week,
    Day
};

// CalendarView - контейнер календаря: общая шапка (навигация назад/вперёд,
// заголовок текущего периода, кнопка Today, переключатель Month/Week/Day)
// и сами представления (MonthView/WeekView/DayView) внутри QStackedWidget.
//
// До Phase D навигация (стрелки, заголовок, Today) жила прямо внутри
// MonthView (тогда ещё называвшегося CalendarWidget) - теперь она здесь,
// потому что у Week и Day будет своя навигация ("предыдущая неделя"/
// "предыдущий день"), а кнопки и заголовок должны быть общими для всех
// трёх представлений, а не дублироваться в каждом из них.
//
// CalendarView не знает подробностей устройства Month/Week/Day - для
// навигации и заголовка все три представления реализуют общий интерфейс
// ICalendarPage (см. ui/ICalendarPage.h). Методы вроде setEventsForVisibleRange()
// ниже - исключение: они "пробрасываются" в то представление, которое
// сейчас активно.
class CalendarView : public QWidget
{
    Q_OBJECT

public:
    explicit CalendarView(QWidget *parent = nullptr);

    CalendarViewMode viewMode() const { return m_viewMode; }
    void setViewMode(CalendarViewMode mode);

    // --- Проброс к активному представлению (Month/Week) ---
    QDate selectedDate() const;
    QDate monthVisibleRangeStart() const;
    QDate monthVisibleRangeEnd() const;
    void setEventsForVisibleRange(const QMap<QDate, QVector<Event>> &eventsByDate);
    void setTheme(const Theme &theme);
    void setFirstDayOfWeek(bool sundayFirst);
    void setSelectedDate(const QDate &date);
    void goToToday();

    // NOW screen (продолжение MVP3) - прокидывается в NowView независимо
    // от того, какой режим сейчас активен (см. .cpp), чтобы данные уже
    // были свежими к моменту, когда пользователь переключится на Now.
    void setNowSnapshot(const NowSnapshot &snapshot, const QDateTime &asOf);

signals:
    void viewModeChanged(CalendarViewMode mode);

    // Эти сигналы пока приходят либо от MonthView, либо от WeekView -
    // CalendarView их просто ретранслирует под общим именем, чтобы
    // MainWindow не знал про внутреннее устройство контейнера.
    void dateSelected(const QDate &date);                              // только Month
    void visibleRangeChanged(const QDate &start, const QDate &end);    // любое представление
    void createEventRequested(const QDate &date);                       // Month: двойной клик по пустой ячейке
    void createEventRequestedWithTime(const QDate &date, const QTime &time); // Week/Day: клик по пустому месту сетки
    void editEventRequested(int eventId);                               // любое представление

    // Событие перетащили мышью на новое время/день (MVP2, drag & drop) -
    // приходит от WeekView или DayView (в MonthView drag пока не реализован).
    void eventRescheduled(int eventId, const QDate &newDate, const QTime &newStartTime, const QTime &newEndTime);

private slots:
    void onPreviousClicked();
    void onNextClicked();
    void onTodayClicked();
    void updateHeaderTitle();

private:
    void updateSwitcherButtons();
    void emitCurrentRange(); // сообщает актуальный диапазон активного представления
    ICalendarPage *currentPage() const;

    CalendarViewMode m_viewMode = CalendarViewMode::Month;

    QLabel *m_titleLabel = nullptr;
    QPushButton *m_nowButton = nullptr;
    QPushButton *m_monthButton = nullptr;
    QPushButton *m_weekButton = nullptr;
    QPushButton *m_dayButton = nullptr;
    QStackedWidget *m_stack = nullptr;

    NowView *m_nowView = nullptr;
    MonthView *m_monthView = nullptr;
    WeekView *m_weekView = nullptr;
    DayView *m_dayView = nullptr;
};
