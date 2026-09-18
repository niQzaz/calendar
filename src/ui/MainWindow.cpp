#include "MainWindow.h"
#include "CalendarWidget.h"
#include "EventDialog.h"
#include "PomodoroWidget.h"
#include "services/CsvImporter.h"
#include "services/NotificationService.h"
#include "services/EventReminder.h"

#include <QListWidget>
#include <QListWidgetItem>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QSplitter>
#include <QWidget>
#include <QMenuBar>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Calendar");
    resize(900, 600);

    auto *fileMenu = menuBar()->addMenu("File");
    auto *importAction = fileMenu->addAction("Import CSV...");
    connect(importAction, &QAction::triggered, this, &MainWindow::onImportCsvClicked);

    m_calendar = new CalendarWidget(this);
    m_notificationService = new NotificationService(this);
    m_pomodoro = new PomodoroWidget(m_notificationService, this);
    m_eventReminder = new EventReminder(&m_eventManager, m_notificationService, this);

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
    connect(m_calendar, &CalendarWidget::visibleRangeChanged, this, &MainWindow::onVisibleRangeChanged);

    // Инициализируем правую панель для дня, который CalendarWidget
    // выбрал по умолчанию (сегодня), и сразу подсвечиваем в сетке дни,
    // на которые уже есть события, загруженные из БД с прошлого запуска.
    m_visibleRangeStart = m_calendar->visibleRangeStart();
    m_visibleRangeEnd = m_calendar->visibleRangeEnd();
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

        const QString recurrence = recurrenceSummary(event);
        if (!recurrence.isEmpty())
            text += QString("   \xE2\x86\xBB %1").arg(recurrence); // ↻ значок повтора

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
    m_calendar->setDatesWithEvents(m_eventManager.datesWithEvents(m_visibleRangeStart, m_visibleRangeEnd));
}

void MainWindow::onVisibleRangeChanged(const QDate &start, const QDate &end)
{
    m_visibleRangeStart = start;
    m_visibleRangeEnd = end;
    refreshCalendarMarkers();
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

void MainWindow::onImportCsvClicked()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this, "Import schedule from CSV", QString(), "CSV files (*.csv);;All files (*)"
    );

    if (filePath.isEmpty())
        return; // пользователь отменил выбор файла

    const CsvImportResult result = CsvImporter::importFromFile(filePath);

    if (result.fileOpenFailed) {
        QMessageBox::warning(this, "Import failed", "Could not open the selected file.");
        return;
    }

    if (result.totalDataRows == 0) {
        QMessageBox::information(this, "Import CSV", "No data rows found in the file.");
        return;
    }

    QString summary = QString("Found %1 valid event(s) out of %2 data row(s).")
        .arg(result.validEvents.size())
        .arg(result.totalDataRows);

    if (!result.errors.isEmpty()) {
        const int maxErrorsShown = 10;
        summary += QString("\n\n%1 row(s) skipped due to errors:\n").arg(result.errors.size());
        for (int i = 0; i < qMin(maxErrorsShown, result.errors.size()); ++i)
            summary += "\n- " + result.errors[i];
        if (result.errors.size() > maxErrorsShown)
            summary += QString("\n... and %1 more").arg(result.errors.size() - maxErrorsShown);
    }

    if (result.validEvents.isEmpty()) {
        QMessageBox::warning(this, "Import CSV", summary);
        return;
    }

    summary += "\n\nImport these events?";
    const auto answer = QMessageBox::question(
        this, "Import CSV", summary, QMessageBox::Yes | QMessageBox::No
    );

    if (answer != QMessageBox::Yes)
        return;

    for (const Event &event : result.validEvents)
        m_eventManager.addEvent(event);

    refreshEventsList();
    refreshCalendarMarkers();

    QMessageBox::information(
        this, "Import CSV", QString("Imported %1 event(s).").arg(result.validEvents.size())
    );
}

void MainWindow::onDeleteEventClicked()
{
    QListWidgetItem *item = m_eventsList->currentItem();
    if (!item)
        return;

    const int eventId = item->data(Qt::UserRole).toInt();

    // Повторяющееся событие в БД - одна строка-шаблон (см. Этап 6),
    // поэтому удаление затрагивает всю серию, а не только показанное
    // повторение. Предупреждаем об этом явно, прежде чем удалять.
    Event event;
    if (m_eventManager.eventById(eventId, event) && event.recurrenceType != RecurrenceType::None) {
        const auto answer = QMessageBox::question(
            this, "Delete repeating event",
            "This is a repeating event. Deleting it removes the whole series "
            "(all past and future occurrences), not just this one. Continue?",
            QMessageBox::Yes | QMessageBox::No
        );
        if (answer != QMessageBox::Yes)
            return;
    }

    m_eventManager.removeEvent(eventId);

    refreshEventsList();
    refreshCalendarMarkers();
}
