#include "DayView.h"
#include "TimeGridView.h"

#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QTimer>

DayView::DayView(QWidget *parent)
    : QWidget(parent)
    , m_date(QDate::currentDate())
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

    connect(m_grid, &TimeGridView::createEventRequested, this, &DayView::createEventRequested);
    connect(m_grid, &TimeGridView::editEventRequested, this, &DayView::editEventRequested);

    rebuildColumnDate();

    // Как и в WeekView - открываем сразу прокрученным к 7:00, а не к полуночи.
    QTimer::singleShot(0, this, [this]() {
        m_scrollArea->verticalScrollBar()->setValue(m_grid->yForTime(QTime(7, 0)));
    });
}

void DayView::rebuildColumnDate()
{
    m_grid->setColumnDates({m_date});
    emit visibleRangeChanged(m_date, m_date);
}

void DayView::goToPrevious()
{
    m_date = m_date.addDays(-1);
    rebuildColumnDate();
}

void DayView::goToNext()
{
    m_date = m_date.addDays(1);
    rebuildColumnDate();
}

void DayView::goToToday()
{
    m_date = QDate::currentDate();
    rebuildColumnDate();
}

QString DayView::headerTitle() const
{
    return m_date.toString("dddd, d MMMM yyyy");
}

void DayView::setEventsForVisibleRange(const QMap<QDate, QVector<Event>> &eventsByDate)
{
    m_grid->setEventsForColumns(eventsByDate);
}

void DayView::setTheme(const Theme &theme)
{
    m_grid->setTheme(theme);
}
