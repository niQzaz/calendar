#include "MainWindow.h"
#include "CalendarWidget.h"
#include "EventDialog.h"

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

    // --- Правая панель: события выбранного дня ---
    auto *rightPanel = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightPanel);

    m_selectedDateLabel = new QLabel(rightPanel);
    m_selectedDateLabel->setObjectName("selectedDateLabel");

    m_eventsList = new QListWidget(rightPanel);

    m_addEventButton = new QPushButton("+ Add event", rightPanel);
    m_deleteEventButton = new QPushButton("Delete event", rightPanel);
    m_deleteEventButton->setEnabled(false);

    rightLayout->addWidget(m_selectedDateLabel);
    rightLayout->addWidget(m_eventsList, 1);
    rightLayout->addWidget(m_addEventButton);
    rightLayout->addWidget(m_deleteEventButton);

    auto *splitter = new QSplitter(this);
    splitter->addWidget(m_calendar);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);

    setCentralWidget(splitter);

    connect(m_calendar, &CalendarWidget::dateSelected, this, &MainWindow::onDateSelected);
    connect(m_addEventButton, &QPushButton::clicked, this, &MainWindow::onAddEventClicked);
    connect(m_deleteEventButton, &QPushButton::clicked, this, &MainWindow::onDeleteEventClicked);
    connect(m_eventsList, &QListWidget::itemSelectionChanged, this, &MainWindow::onEventSelectionChanged);

    // Инициализируем правую панель для дня, который CalendarWidget
    // выбрал по умолчанию (сегодня).
    onDateSelected(m_calendar->selectedDate());
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
        const QString text = QString("%1 - %2   %3")
            .arg(event.startTime.toString("HH:mm"))
            .arg(event.endTime.toString("HH:mm"))
            .arg(event.title);

        auto *item = new QListWidgetItem(text, m_eventsList);
        item->setData(Qt::UserRole, event.id);
        if (!event.description.isEmpty())
            item->setToolTip(event.description);
    }

    m_deleteEventButton->setEnabled(false);
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
    m_deleteEventButton->setEnabled(!m_eventsList->selectedItems().isEmpty());
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
