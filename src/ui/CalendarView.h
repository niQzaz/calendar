#pragma once

#include <QWidget>
#include <QDate>
#include <QMap>
#include <QVector>

#include "models/Event.h"
#include "services/Theme.h"

class QLabel;
class QPushButton;
class QStackedWidget;
class MonthView;
class WeekView;
class DayView;
class ICalendarPage;

// Какое из трёх представлений сейчас показано.
enum class CalendarViewMode
{
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
// ниже - исключение: пока полноценно работает только Month View, поэтому
// такие методы пока просто "пробрасываются" в MonthView напрямую;
// по мере реализации Week/Day (Phase E/F) список будет расти симметрично.
class CalendarView : public QWidget
{
    Q_OBJECT

public:
    explicit CalendarView(QWidget *parent = nullptr);

    CalendarViewMode viewMode() const { return m_viewMode; }
    void setViewMode(CalendarViewMode mode);

    // --- Проброс к MonthView ---
    QDate selectedDate() const;
    QDate monthVisibleRangeStart() const;
    QDate monthVisibleRangeEnd() const;
    void setEventsForVisibleRange(const QMap<QDate, QVector<Event>> &eventsByDate);
    void setTheme(const Theme &theme);
    void setFirstDayOfWeek(bool sundayFirst);
    void setSelectedDate(const QDate &date);
    void goToToday();

signals:
    void viewModeChanged(CalendarViewMode mode);

    // Эти три сигнала пока целиком приходят от MonthView (единственного
    // полнофункционального представления) - CalendarView их просто
    // ретранслирует под тем же именем, чтобы MainWindow не знал про
    // внутреннее устройство контейнера.
    void dateSelected(const QDate &date);
    void visibleRangeChanged(const QDate &start, const QDate &end);
    void createEventRequested(const QDate &date);
    void editEventRequested(int eventId);

private slots:
    void onPreviousClicked();
    void onNextClicked();
    void onTodayClicked();
    void updateHeaderTitle();

private:
    void updateSwitcherButtons();
    ICalendarPage *currentPage() const;

    CalendarViewMode m_viewMode = CalendarViewMode::Month;

    QLabel *m_titleLabel = nullptr;
    QPushButton *m_monthButton = nullptr;
    QPushButton *m_weekButton = nullptr;
    QPushButton *m_dayButton = nullptr;
    QStackedWidget *m_stack = nullptr;

    MonthView *m_monthView = nullptr;
    WeekView *m_weekView = nullptr;
    DayView *m_dayView = nullptr;
};
