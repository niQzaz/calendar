#include "CalendarWidget.h"
#include "MonthDayCell.h"

#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStringList>

namespace {
const QStringList kWeekDayNamesMondayFirst = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
const QStringList kWeekDayNamesSundayFirst = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const QStringList kMonthNames = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};
}

CalendarWidget::CalendarWidget(QWidget *parent)
    : QWidget(parent)
{
    const QDate today = QDate::currentDate();
    m_year = today.year();
    m_month = today.month();
    m_selectedDate = today;

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(8);

    // --- Заголовок: [<]  September 2026  [>]      [Today] ---
    auto *headerLayout = new QHBoxLayout();
    auto *prevButton = new QPushButton("<", this);
    auto *nextButton = new QPushButton(">", this);
    auto *todayButton = new QPushButton("Today", this);
    prevButton->setFixedWidth(32);
    nextButton->setFixedWidth(32);
    prevButton->setObjectName("navButton");
    nextButton->setObjectName("navButton");
    todayButton->setObjectName("todayButton");

    m_monthLabel = new QLabel(this);
    m_monthLabel->setAlignment(Qt::AlignCenter);
    m_monthLabel->setObjectName("monthLabel");

    headerLayout->addWidget(prevButton);
    headerLayout->addWidget(m_monthLabel, 1);
    headerLayout->addWidget(nextButton);
    headerLayout->addSpacing(16);
    headerLayout->addWidget(todayButton);
    rootLayout->addLayout(headerLayout);

    connect(prevButton, &QPushButton::clicked, this, &CalendarWidget::goToPreviousMonth);
    connect(nextButton, &QPushButton::clicked, this, &CalendarWidget::goToNextMonth);
    connect(todayButton, &QPushButton::clicked, this, &CalendarWidget::goToToday);

    // --- Названия дней недели ---
    auto *weekDaysLayout = new QHBoxLayout();
    m_weekDayLabels.reserve(7);
    for (int i = 0; i < 7; ++i) {
        auto *label = new QLabel(this);
        label->setAlignment(Qt::AlignCenter);
        label->setObjectName("weekDayLabel");
        weekDaysLayout->addWidget(label);
        m_weekDayLabels.append(label);
    }
    rootLayout->addLayout(weekDaysLayout);

    // --- Сетка дней: создаём 42 ячейки один раз, дальше только обновляем их данные ---
    m_gridLayout = new QGridLayout();
    m_gridLayout->setSpacing(4);
    rootLayout->addLayout(m_gridLayout);

    m_dayCells.reserve(42);
    m_cellDates.resize(42);

    for (int i = 0; i < 42; ++i) {
        auto *cell = new MonthDayCell(this);
        m_gridLayout->addWidget(cell, i / 7, i % 7);

        connect(cell, &MonthDayCell::clicked, this, &CalendarWidget::setSelectedDate);
        connect(cell, &MonthDayCell::createEventRequested, this, [this](const QDate &date) {
            setSelectedDate(date);
            emit createEventRequested(date);
        });
        connect(cell, &MonthDayCell::editEventRequested, this, &CalendarWidget::editEventRequested);

        m_dayCells.append(cell);
    }

    updateWeekDayLabels();
    rebuildGrid();
}

void CalendarWidget::updateHeaderLabel()
{
    m_monthLabel->setText(QString("%1 %2").arg(kMonthNames[m_month - 1]).arg(m_year));
}

void CalendarWidget::updateWeekDayLabels()
{
    const QStringList &names = m_sundayFirst ? kWeekDayNamesSundayFirst : kWeekDayNamesMondayFirst;
    for (int i = 0; i < 7; ++i)
        m_weekDayLabels[i]->setText(names[i]);
}

void CalendarWidget::rebuildGrid()
{
    updateHeaderLabel();

    const QDate firstOfMonth(m_year, m_month, 1);

    // dayOfWeek(): 1 = понедельник ... 7 = воскресенье.
    // Столько пустых ячеек нужно оставить перед 1-м числом месяца -
    // зависит от того, с какого дня недели начинается сетка (Этап 8):
    // при неделе с понедельника колонка 0 - это Пн (dayOfWeek()==1 → 0 отступа),
    // при неделе с воскресенья колонка 0 - это Вс (dayOfWeek()==7 → 0 отступа).
    const int leadingEmptyCells = m_sundayFirst
        ? (firstOfMonth.dayOfWeek() % 7)
        : (firstOfMonth.dayOfWeek() - 1);
    const QDate today = QDate::currentDate();

    for (int i = 0; i < 42; ++i) {
        MonthDayCell *cell = m_dayCells[i];
        const int dayNumber = i - leadingEmptyCells + 1;

        // QDate сам корректно "переносит" дату через границу месяца:
        // для dayNumber <= 0 получаем дни предыдущего месяца,
        // для dayNumber > daysInMonth() - дни следующего.
        const QDate cellDate = firstOfMonth.addDays(dayNumber - 1);
        const bool belongsToCurrentMonth = (cellDate.month() == m_month && cellDate.year() == m_year);

        m_cellDates[i] = cellDate;
        cell->setDate(cellDate);
        cell->setOtherMonth(!belongsToCurrentMonth);
        cell->setToday(cellDate == today);
        cell->setSelected(cellDate == m_selectedDate);
    }

    // Данные о событиях могли устареть относительно нового набора дат -
    // MainWindow пришлёт актуальные по сигналу visibleRangeChanged чуть
    // позже, но применяем то, что уже есть, чтобы не мигать пустой сеткой.
    applyEventsToVisibleCells();
}

void CalendarWidget::applyEventsToVisibleCells()
{
    for (int i = 0; i < m_dayCells.size(); ++i)
        m_dayCells[i]->setEvents(m_eventsByDate.value(m_cellDates[i]));
}

void CalendarWidget::setEventsForVisibleRange(const QMap<QDate, QVector<Event>> &eventsByDate)
{
    m_eventsByDate = eventsByDate;
    applyEventsToVisibleCells();
}

void CalendarWidget::setTheme(const Theme &theme)
{
    for (MonthDayCell *cell : m_dayCells)
        cell->setTheme(theme);
}

void CalendarWidget::setFirstDayOfWeek(bool sundayFirst)
{
    if (m_sundayFirst == sundayFirst)
        return;

    m_sundayFirst = sundayFirst;
    updateWeekDayLabels();
    rebuildGrid();
    emit visibleRangeChanged(m_cellDates.first(), m_cellDates.last());
}

void CalendarWidget::goToPreviousMonth()
{
    m_month -= 1;
    if (m_month < 1) {
        m_month = 12;
        m_year -= 1;
    }
    rebuildGrid();
    emit visibleRangeChanged(m_cellDates.first(), m_cellDates.last());
}

void CalendarWidget::goToNextMonth()
{
    m_month += 1;
    if (m_month > 12) {
        m_month = 1;
        m_year += 1;
    }
    rebuildGrid();
    emit visibleRangeChanged(m_cellDates.first(), m_cellDates.last());
}

void CalendarWidget::goToToday()
{
    setSelectedDate(QDate::currentDate());
}

void CalendarWidget::setSelectedDate(const QDate &date)
{
    m_year = date.year();
    m_month = date.month();
    m_selectedDate = date;
    rebuildGrid();
    emit visibleRangeChanged(m_cellDates.first(), m_cellDates.last());
    emit dateSelected(m_selectedDate);
}
