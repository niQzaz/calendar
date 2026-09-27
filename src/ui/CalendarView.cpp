#include "CalendarView.h"
#include "MonthView.h"
#include "WeekView.h"
#include "DayView.h"

#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>

CalendarView::CalendarView(QWidget *parent)
    : QWidget(parent)
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(8);

    // --- Строка 1: [<]  заголовок периода  [>]      [Today] ---
    auto *navLayout = new QHBoxLayout();
    auto *prevButton = new QPushButton("<", this);
    auto *nextButton = new QPushButton(">", this);
    auto *todayButton = new QPushButton("Today", this);
    prevButton->setFixedWidth(32);
    nextButton->setFixedWidth(32);
    prevButton->setObjectName("navButton");
    nextButton->setObjectName("navButton");
    todayButton->setObjectName("todayButton");

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName("monthLabel"); // переиспользуем стиль заголовка периода
    m_titleLabel->setAlignment(Qt::AlignCenter);

    navLayout->addWidget(prevButton);
    navLayout->addWidget(m_titleLabel, 1);
    navLayout->addWidget(nextButton);
    navLayout->addSpacing(16);
    navLayout->addWidget(todayButton);
    rootLayout->addLayout(navLayout);

    connect(prevButton, &QPushButton::clicked, this, &CalendarView::onPreviousClicked);
    connect(nextButton, &QPushButton::clicked, this, &CalendarView::onNextClicked);
    connect(todayButton, &QPushButton::clicked, this, &CalendarView::onTodayClicked);

    // --- Строка 2: переключатель Month | Week | Day ---
    auto *switcherLayout = new QHBoxLayout();
    switcherLayout->addStretch();

    m_monthButton = new QPushButton("Month", this);
    m_weekButton = new QPushButton("Week", this);
    m_dayButton = new QPushButton("Day", this);
    for (QPushButton *button : {m_monthButton, m_weekButton, m_dayButton}) {
        button->setObjectName("viewSwitcherButton");
        button->setCheckable(true);
        switcherLayout->addWidget(button);
    }
    switcherLayout->addStretch();
    rootLayout->addLayout(switcherLayout);

    connect(m_monthButton, &QPushButton::clicked, this, [this]() { setViewMode(CalendarViewMode::Month); });
    connect(m_weekButton, &QPushButton::clicked, this, [this]() { setViewMode(CalendarViewMode::Week); });
    connect(m_dayButton, &QPushButton::clicked, this, [this]() { setViewMode(CalendarViewMode::Day); });

    // --- Сами представления ---
    m_monthView = new MonthView(this);
    m_weekView = new WeekView(this);
    m_dayView = new DayView(this);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_monthView);
    m_stack->addWidget(m_weekView);
    m_stack->addWidget(m_dayView);
    rootLayout->addWidget(m_stack, 1);

    // MonthView - пока единственное представление, которое реально работает
    // с событиями/датами полностью (панель справа и т.п.), поэтому его
    // сигналы ретранслируются наружу под теми же именами.
    connect(m_monthView, &MonthView::dateSelected, this, &CalendarView::dateSelected);
    connect(m_monthView, &MonthView::visibleRangeChanged, this, &CalendarView::visibleRangeChanged);
    connect(m_monthView, &MonthView::visibleRangeChanged, this, &CalendarView::updateHeaderTitle);
    connect(m_monthView, &MonthView::createEventRequested, this, &CalendarView::createEventRequested);
    connect(m_monthView, &MonthView::editEventRequested, this, &CalendarView::editEventRequested);

    // WeekView (Phase E) - клик по сетке несёт ещё и время, поэтому его
    // createEventRequested ретранслируется как createEventRequestedWithTime,
    // а не как обычный createEventRequested.
    connect(m_weekView, &WeekView::visibleRangeChanged, this, &CalendarView::visibleRangeChanged);
    connect(m_weekView, &WeekView::visibleRangeChanged, this, &CalendarView::updateHeaderTitle);
    connect(m_weekView, &WeekView::createEventRequested, this, &CalendarView::createEventRequestedWithTime);
    connect(m_weekView, &WeekView::editEventRequested, this, &CalendarView::editEventRequested);
    connect(m_weekView, &WeekView::eventRescheduled, this, &CalendarView::eventRescheduled);

    // DayView (Phase F) - переиспользует тот же TimeGridView, что и WeekView,
    // поэтому сигналы того же вида и подключаются точно так же.
    connect(m_dayView, &DayView::visibleRangeChanged, this, &CalendarView::visibleRangeChanged);
    connect(m_dayView, &DayView::visibleRangeChanged, this, &CalendarView::updateHeaderTitle);
    connect(m_dayView, &DayView::createEventRequested, this, &CalendarView::createEventRequestedWithTime);
    connect(m_dayView, &DayView::editEventRequested, this, &CalendarView::editEventRequested);
    connect(m_dayView, &DayView::eventRescheduled, this, &CalendarView::eventRescheduled);

    setViewMode(CalendarViewMode::Month);
}

ICalendarPage *CalendarView::currentPage() const
{
    switch (m_viewMode) {
    case CalendarViewMode::Month: return m_monthView;
    case CalendarViewMode::Week: return m_weekView;
    case CalendarViewMode::Day: return m_dayView;
    }
    return nullptr;
}

void CalendarView::setViewMode(CalendarViewMode mode)
{
    m_viewMode = mode;

    switch (mode) {
    case CalendarViewMode::Month: m_stack->setCurrentWidget(m_monthView); break;
    case CalendarViewMode::Week: m_stack->setCurrentWidget(m_weekView); break;
    case CalendarViewMode::Day: m_stack->setCurrentWidget(m_dayView); break;
    }

    updateSwitcherButtons();
    updateHeaderTitle();
    emitCurrentRange();
    emit viewModeChanged(mode);
}

void CalendarView::emitCurrentRange()
{
    // При переключении представления (а не навигации внутри него) само
    // представление никакого сигнала не шлёт - нужно попросить у него
    // актуальный диапазон и сообщить наружу явно, иначе MainWindow не
    // узнает, что нужно подгрузить события для новых дат.
    switch (m_viewMode) {
    case CalendarViewMode::Month:
        emit visibleRangeChanged(m_monthView->visibleRangeStart(), m_monthView->visibleRangeEnd());
        break;
    case CalendarViewMode::Week:
        emit visibleRangeChanged(m_weekView->visibleRangeStart(), m_weekView->visibleRangeEnd());
        break;
    case CalendarViewMode::Day:
        emit visibleRangeChanged(m_dayView->currentDate(), m_dayView->currentDate());
        break;
    }
}

void CalendarView::updateSwitcherButtons()
{
    m_monthButton->setChecked(m_viewMode == CalendarViewMode::Month);
    m_weekButton->setChecked(m_viewMode == CalendarViewMode::Week);
    m_dayButton->setChecked(m_viewMode == CalendarViewMode::Day);
}

void CalendarView::updateHeaderTitle()
{
    ICalendarPage *page = currentPage();
    m_titleLabel->setText(page ? page->headerTitle() : QString());
}

void CalendarView::onPreviousClicked()
{
    if (ICalendarPage *page = currentPage())
        page->goToPrevious();
    updateHeaderTitle();
}

void CalendarView::onNextClicked()
{
    if (ICalendarPage *page = currentPage())
        page->goToNext();
    updateHeaderTitle();
}

void CalendarView::onTodayClicked()
{
    if (ICalendarPage *page = currentPage())
        page->goToToday();
    updateHeaderTitle();
}

QDate CalendarView::selectedDate() const
{
    return m_monthView->selectedDate();
}

QDate CalendarView::monthVisibleRangeStart() const
{
    return m_monthView->visibleRangeStart();
}

QDate CalendarView::monthVisibleRangeEnd() const
{
    return m_monthView->visibleRangeEnd();
}

void CalendarView::setEventsForVisibleRange(const QMap<QDate, QVector<Event>> &eventsByDate)
{
    // Данные относятся к диапазону ИМЕННО активного представления (MainWindow
    // запрашивает их в ответ на visibleRangeChanged, который всегда несёт
    // диапазон текущего режима) - поэтому раздаём только активному, а не
    // обоим сразу: иначе, например, узкий недельный диапазон "затёр" бы
    // месячную сетку почти пустой на короткое время между переключениями.
    switch (m_viewMode) {
    case CalendarViewMode::Month:
        m_monthView->setEventsForVisibleRange(eventsByDate);
        break;
    case CalendarViewMode::Week:
        m_weekView->setEventsForVisibleRange(eventsByDate);
        break;
    case CalendarViewMode::Day:
        m_dayView->setEventsForVisibleRange(eventsByDate);
        break;
    }
}

void CalendarView::setTheme(const Theme &theme)
{
    m_monthView->setTheme(theme);
    m_weekView->setTheme(theme);
    m_dayView->setTheme(theme);
}

void CalendarView::setFirstDayOfWeek(bool sundayFirst)
{
    m_monthView->setFirstDayOfWeek(sundayFirst);
    m_weekView->setFirstDayOfWeek(sundayFirst);
}

void CalendarView::setSelectedDate(const QDate &date)
{
    m_monthView->setSelectedDate(date);
}

void CalendarView::goToToday()
{
    if (ICalendarPage *page = currentPage())
        page->goToToday();
    updateHeaderTitle();
}
