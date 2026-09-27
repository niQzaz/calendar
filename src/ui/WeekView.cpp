#include "WeekView.h"
#include "TimeGridView.h"

#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QTimer>

namespace {
QDate startOfWeek(const QDate &date, bool sundayFirst)
{
    const int isoWeekday = date.dayOfWeek(); // 1 = Пн ... 7 = Вс
    const int offset = sundayFirst ? (isoWeekday % 7) : (isoWeekday - 1);
    return date.addDays(-offset);
}
}

WeekView::WeekView(QWidget *parent)
    : QWidget(parent)
    , m_weekStart(startOfWeek(QDate::currentDate(), false))
{
    m_grid = new TimeGridView(this);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidget(m_grid);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_scrollArea);

    connect(m_grid, &TimeGridView::createEventRequested, this, &WeekView::createEventRequested);
    connect(m_grid, &TimeGridView::editEventRequested, this, &WeekView::editEventRequested);
    connect(m_grid, &TimeGridView::eventRescheduled, this, &WeekView::eventRescheduled);

    rebuildColumnDates();

    // Открываем неделю сразу прокрученной к разумному рабочему времени
    // (7:00), а не к полуночи - большинству событий там всё равно не бывает.
    // QTimer::singleShot(0, ...) - стандартный приём: на момент конструктора
    // виджет ещё не уложен окончательно (geometry могла не устаканиться),
    // поэтому откладываем скролл на "после того, как layout закончится".
    QTimer::singleShot(0, this, [this]() {
        m_scrollArea->verticalScrollBar()->setValue(m_grid->yForTime(QTime(7, 0)));
    });
}

void WeekView::rebuildColumnDates()
{
    QVector<QDate> dates;
    dates.reserve(7);
    for (int i = 0; i < 7; ++i)
        dates.append(m_weekStart.addDays(i));

    m_grid->setColumnDates(dates);
    emit visibleRangeChanged(m_weekStart, m_weekStart.addDays(6));
}

void WeekView::goToPrevious()
{
    m_weekStart = m_weekStart.addDays(-7);
    rebuildColumnDates();
}

void WeekView::goToNext()
{
    m_weekStart = m_weekStart.addDays(7);
    rebuildColumnDates();
}

void WeekView::goToToday()
{
    m_weekStart = startOfWeek(QDate::currentDate(), m_sundayFirst);
    rebuildColumnDates();
}

QString WeekView::headerTitle() const
{
    const QDate weekEnd = m_weekStart.addDays(6);

    if (m_weekStart.month() == weekEnd.month()) {
        return QString("%1 - %2 %3")
            .arg(m_weekStart.day())
            .arg(weekEnd.day())
            .arg(weekEnd.toString("MMMM yyyy"));
    }

    return QString("%1 - %2").arg(m_weekStart.toString("d MMMM")).arg(weekEnd.toString("d MMMM yyyy"));
}

void WeekView::setFirstDayOfWeek(bool sundayFirst)
{
    if (m_sundayFirst == sundayFirst)
        return;

    m_sundayFirst = sundayFirst;
    // Пересчитываем начало ТОЙ ЖЕ недели под новый порядок дней -
    // не "прыгаем" на другую неделю, просто меняем, с какого дня она
    // теперь официально начинается.
    m_weekStart = startOfWeek(m_weekStart, sundayFirst);
    rebuildColumnDates();
}

void WeekView::setEventsForVisibleRange(const QMap<QDate, QVector<Event>> &eventsByDate)
{
    m_grid->setEventsForColumns(eventsByDate);
}

void WeekView::setTheme(const Theme &theme)
{
    m_grid->setTheme(theme);
}
