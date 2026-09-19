#include "MainWindow.h"
#include "CalendarWidget.h"
#include "EventDialog.h"
#include "PomodoroWidget.h"
#include "SettingsDialog.h"
#include "services/CsvImporter.h"
#include "services/NotificationService.h"
#include "services/EventReminder.h"
#include "services/ThemeManager.h"

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
#include <QMap>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Calendar");
    resize(900, 600);

    // ThemeManager создаётся первым - его конструктор сразу применяет
    // сохранённую тему ко всему приложению (палитра + QSS), прежде чем
    // остальные виджеты этого окна будут созданы и показаны.
    m_themeManager = new ThemeManager(&m_settings, this);

    auto *fileMenu = menuBar()->addMenu("File");

    m_calendar = new CalendarWidget(this);
    m_notificationService = new NotificationService(&m_settings, this);
    m_pomodoro = new PomodoroWidget(&m_settings, m_notificationService, this);
    m_eventReminder = new EventReminder(&m_eventManager, m_notificationService, this);

    m_calendar->setFirstDayOfWeek(m_settings.sundayFirst());
    m_calendar->setTheme(m_themeManager->currentTheme());
    connect(m_themeManager, &ThemeManager::themeChanged, m_calendar, &CalendarWidget::setTheme);

    // Действия меню создаём только теперь - m_togglePomodoroAction
    // подключается напрямую к m_pomodoro, а m_goToTodayAction - к m_calendar
    // (через лямбду), оба должны уже существовать к этому моменту.
    m_newEventAction = new QAction("New event", this);
    connect(m_newEventAction, &QAction::triggered, this, &MainWindow::onAddEventClicked);
    fileMenu->addAction(m_newEventAction);

    m_importCsvAction = new QAction("Import CSV...", this);
    connect(m_importCsvAction, &QAction::triggered, this, &MainWindow::onImportCsvClicked);
    fileMenu->addAction(m_importCsvAction);

    fileMenu->addSeparator();

    auto *settingsAction = fileMenu->addAction("Settings...");
    connect(settingsAction, &QAction::triggered, this, &MainWindow::onSettingsClicked);

    // Today и Pomodoro в меню не показываем - они нужны только как
    // горячие клавиши, поэтому addAction(this), а не в меню.
    m_goToTodayAction = new QAction(this);
    connect(m_goToTodayAction, &QAction::triggered, this, [this]() { m_calendar->goToToday(); });
    addAction(m_goToTodayAction);

    m_togglePomodoroAction = new QAction(this);
    connect(m_togglePomodoroAction, &QAction::triggered, m_pomodoro, &PomodoroWidget::toggleStartPause);
    addAction(m_togglePomodoroAction);

    applyShortcuts();

    // --- Правая панель: события выбранного дня ---
    auto *rightPanel = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightPanel);

    m_selectedDateLabel = new QLabel(rightPanel);
    m_selectedDateLabel->setObjectName("selectedDateLabel");

    m_eventsList = new QListWidget(rightPanel);
    m_eventsList->setObjectName("eventsList");

    auto *hintLabel = new QLabel("Double-click an event to edit it", rightPanel);
    hintLabel->setObjectName("hintLabel");

    m_addEventButton = new QPushButton("+ Add event", rightPanel);
    m_addEventButton->setObjectName("panelButton");
    m_deleteEventButton = new QPushButton("Delete event", rightPanel);
    m_deleteEventButton->setObjectName("panelButton");
    m_deleteEventButton->setEnabled(false);
    m_startPomodoroButton = new QPushButton("Start Pomodoro", rightPanel);
    m_startPomodoroButton->setObjectName("panelButton");
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
    splitter->setHandleWidth(1);

    // Небольшие отступы вокруг всего контента вместо того, чтобы сплиттер
    // упирался прямо в края окна - чуть более "продуманный" вид, чем голый
    // central widget без полей.
    auto *centralContainer = new QWidget(this);
    auto *centralLayout = new QVBoxLayout(centralContainer);
    centralLayout->setContentsMargins(12, 12, 12, 12);
    centralLayout->addWidget(splitter);
    setCentralWidget(centralContainer);

    connect(m_calendar, &CalendarWidget::dateSelected, this, &MainWindow::onDateSelected);
    connect(m_addEventButton, &QPushButton::clicked, this, &MainWindow::onAddEventClicked);
    connect(m_deleteEventButton, &QPushButton::clicked, this, &MainWindow::onDeleteEventClicked);
    connect(m_eventsList, &QListWidget::itemSelectionChanged, this, &MainWindow::onEventSelectionChanged);
    connect(m_eventsList, &QListWidget::itemDoubleClicked, this, &MainWindow::onEventDoubleClicked);
    connect(m_startPomodoroButton, &QPushButton::clicked, this, &MainWindow::onStartPomodoroClicked);
    connect(m_pomodoro, &PomodoroWidget::pomodoroCompletedForEvent, this, &MainWindow::onPomodoroCompletedForEvent);
    connect(m_calendar, &CalendarWidget::visibleRangeChanged, this, &MainWindow::onVisibleRangeChanged);
    connect(m_calendar, &CalendarWidget::createEventRequested, this, &MainWindow::onCreateEventRequested);
    connect(m_calendar, &CalendarWidget::editEventRequested, this, &MainWindow::onEditEventRequested);

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
    if (!m_visibleRangeStart.isValid() || !m_visibleRangeEnd.isValid())
        return;

    const QVector<Event> events = m_eventManager.eventsInRange(m_visibleRangeStart, m_visibleRangeEnd);

    QMap<QDate, QVector<Event>> eventsByDate;
    for (const Event &event : events)
        eventsByDate[event.date].append(event);

    m_calendar->setEventsForVisibleRange(eventsByDate);
}

void MainWindow::onVisibleRangeChanged(const QDate &start, const QDate &end)
{
    m_visibleRangeStart = start;
    m_visibleRangeEnd = end;
    refreshCalendarMarkers();
}

void MainWindow::onAddEventClicked()
{
    openNewEventDialog(m_calendar->selectedDate());
}

void MainWindow::openNewEventDialog(const QDate &defaultDate)
{
    EventDialog dialog(defaultDate, this);
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
    openEditEventDialog(item->data(Qt::UserRole).toInt());
}

void MainWindow::openEditEventDialog(int eventId)
{
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

void MainWindow::onCreateEventRequested(const QDate &date)
{
    openNewEventDialog(date);
}

void MainWindow::onEditEventRequested(int eventId)
{
    openEditEventDialog(eventId);
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

void MainWindow::applyShortcuts()
{
    m_newEventAction->setShortcut(m_settings.shortcut(ShortcutAction::NewEvent));
    m_importCsvAction->setShortcut(m_settings.shortcut(ShortcutAction::ImportCsv));
    m_goToTodayAction->setShortcut(m_settings.shortcut(ShortcutAction::GoToToday));
    m_togglePomodoroAction->setShortcut(m_settings.shortcut(ShortcutAction::TogglePomodoro));
}

void MainWindow::onSettingsClicked()
{
    SettingsDialog dialog(&m_settings, m_themeManager, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    // Тема уже применена живьём самим ThemeManager (внутри SettingsDialog::onAccept).
    // Здесь применяем то, что ThemeManager не касается.
    m_calendar->setFirstDayOfWeek(m_settings.sundayFirst());
    applyShortcuts();
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
