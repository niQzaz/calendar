#include "MainWindow.h"
#include "CalendarWidget.h"
#include "EventDialog.h"
#include "PomodoroWidget.h"

#include <QListWidget>
#include <QListWidgetItem>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QSplitter>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Calendar");
    resize(900, 600);

    m_calendar = new CalendarWidget(this);
    m_pomodoro = new PomodoroWidget(this);

    // --- Правая панель: события выбранного дня ---
    auto *rightPanel = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightPanel);

    m_selectedDateLabel = new QLabel(rightPanel);
    m_selectedDateLabel->setObjectName("selectedDateLabel");

    m_eventsList = new QListWidget(rightPanel);

    auto *hintLabel = new QLabel("Double-click an event to edit it", rightPanel);
    hintLabel->setObjectName("hintLabel");

    m_addEventButton = new QPushButton("+ Add event", rightPanel);
    m_deleteEventButton = new QPushButton("Delete event", rightPanel);
    m_deleteEventButton->setEnabled(false);
    m_startPomodoroButton = new QPushButton("Start Pomodoro", rightPanel);
    m_startPomodoroButton->setEnabled(false);

    rightLayout->addWidget(m_selectedDateLabel);
    rightLayout->addWidget(m_eventsList, 1);
    rightLayout->addWidget(hintLabel);
    rightLayout->addWidget(m_addEventButton);
    rightLayout->addWidget(m_deleteEventButton);
    rightLayout->addWidget(m_startPomodoroButton);

    auto *splitter = new QSplitter(this);
    splitter->addWidget(m_calendar);
    splitter->addWidget(rightPanel);
    splitter->addWidget(m_pomodoro);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 1);

    setCentralWidget(splitter);

    connect(m_calendar, &CalendarWidget::dateSelected, this, &MainWindow::onDateSelected);
    connect(m_addEventButton, &QPushButton::clicked, this, &MainWindow::onAddEventClicked);
    connect(m_deleteEventButton, &QPushButton::clicked, this, &MainWindow::onDeleteEventClicked);
    connect(m_eventsList, &QListWidget::itemSelectionChanged, this, &MainWindow::onEventSelectionChanged);
    connect(m_eventsList, &QListWidget::itemDoubleClicked, this, &MainWindow::onEventDoubleClicked);
    connect(m_startPomodoroButton, &QPushButton::clicked, this, &MainWindow::onStartPomodoroClicked);
    connect(m_pomodoro, &PomodoroWidget::pomodoroCompletedForEvent, this, &MainWindow::onPomodoroCompletedForEvent);

    // Инициализируем правую панель для дня, который CalendarWidget
    // выбрал по умолчанию (сегодня), и сразу подсвечиваем в сетке дни,
    // на которые уже есть события, загруженные из БД с прошлого запуска.
    onDateSelected(m_calendar->selectedDate());
    refreshCalendarMarkers();
}

void MainWindow::onDateSelected(const QDate &date)
{
    m_selectedDateLabel->setText(date.toString("dddd, d MMMM yyyy"));
    refreshEventsList();
}

void MainWindow::refreshEventsList()
{
    m_eventsList->clear();

    const QDate date = m_calendar->selectedDate();
    const QVector<Event> events = m_eventManager.eventsForDate(date);

    for (const Event &event : events) {
        QString text = QString("%1 - %2   %3")
            .arg(event.startTime.toString("HH:mm"))
            .arg(event.endTime.toString("HH:mm"))
            .arg(event.title);

        if (event.pomodorosCompleted > 0)
            text += QString("   \xF0\x9F\x8D\x85\xC3\x97%1").arg(event.pomodorosCompleted);

        auto *item = new QListWidgetItem(text, m_eventsList);
        item->setData(Qt::UserRole, event.id);
        if (!event.description.isEmpty())
            item->setToolTip(event.description);
    }

    m_deleteEventButton->setEnabled(false);
    m_startPomodoroButton->setEnabled(false);
}

void MainWindow::refreshCalendarMarkers()
{
    m_calendar->setDatesWithEvents(m_eventManager.datesWithEvents());
}

void MainWindow::onAddEventClicked()
{
    EventDialog dialog(m_calendar->selectedDate(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_eventManager.addEvent(dialog.toEvent());
    refreshEventsList();
    refreshCalendarMarkers();
}

void MainWindow::onEventSelectionChanged()
{
    const bool hasSelection = !m_eventsList->selectedItems().isEmpty();
    m_deleteEventButton->setEnabled(hasSelection);
    m_startPomodoroButton->setEnabled(hasSelection);
}

void MainWindow::onEventDoubleClicked(QListWidgetItem *item)
{
    const int eventId = item->data(Qt::UserRole).toInt();

    Event existingEvent;
    if (!m_eventManager.eventById(eventId, existingEvent))
        return;

    EventDialog dialog(existingEvent, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_eventManager.updateEvent(dialog.toEvent());
    refreshEventsList();
    refreshCalendarMarkers();
}

void MainWindow::onStartPomodoroClicked()
{
    QListWidgetItem *item = m_eventsList->currentItem();
    if (!item)
        return;

    const int eventId = item->data(Qt::UserRole).toInt();

    Event event;
    if (!m_eventManager.eventById(eventId, event))
        return;

    const QString taskLabel = QString("%1 (%2\xE2\x80\x93%3)")
        .arg(event.title)
        .arg(event.startTime.toString("HH:mm"))
        .arg(event.endTime.toString("HH:mm"));

    m_pomodoro->startForTask(event.id, taskLabel);
}

void MainWindow::onPomodoroCompletedForEvent(int eventId)
{
    m_eventManager.incrementPomodoroCount(eventId);
    refreshEventsList();
}

void MainWindow::onDeleteEventClicked()
{
    QListWidgetItem *item = m_eventsList->currentItem();
    if (!item)
        return;

    const int eventId = item->data(Qt::UserRole).toInt();
    m_eventManager.removeEvent(eventId);

    refreshEventsList();
    refreshCalendarMarkers();
}
