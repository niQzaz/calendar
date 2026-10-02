#include "MainWindow.h"
#include "CalendarView.h"
#include "EventDialog.h"
#include "PomodoroWidget.h"
#include "SettingsDialog.h"
#include "CategoryManagerDialog.h"
#include "services/CsvImporter.h"
#include "services/NotificationService.h"
#include "services/EventReminder.h"
#include "services/ThemeManager.h"
#include "models/Execution.h"

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
#include <QTimer>

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

    m_calendarView = new CalendarView(this);
    m_notificationService = new NotificationService(&m_settings, this);
    m_pomodoro = new PomodoroWidget(&m_settings, m_notificationService, this);
    m_eventReminder = new EventReminder(&m_eventManager, m_notificationService, this);

    m_calendarView->setFirstDayOfWeek(m_settings.sundayFirst());
    m_calendarView->setTheme(m_themeManager->currentTheme());
    connect(m_themeManager, &ThemeManager::themeChanged, m_calendarView, &CalendarView::setTheme);

    // Действия меню создаём только теперь - m_togglePomodoroAction
    // подключается напрямую к m_pomodoro, а m_goToTodayAction - к m_calendarView
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

    auto *categoriesAction = fileMenu->addAction("Manage categories...");
    connect(categoriesAction, &QAction::triggered, this, &MainWindow::onManageCategoriesClicked);

    // Today и Pomodoro в меню не показываем - они нужны только как
    // горячие клавиши, поэтому addAction(this), а не в меню.
    m_goToTodayAction = new QAction(this);
    connect(m_goToTodayAction, &QAction::triggered, this, [this]() { m_calendarView->goToToday(); });
    addAction(m_goToTodayAction);

    m_togglePomodoroAction = new QAction(this);
    connect(m_togglePomodoroAction, &QAction::triggered, m_pomodoro, &PomodoroWidget::toggleStartPause);
    addAction(m_togglePomodoroAction);

    applyShortcuts();

    // --- Правая панель: события выбранного дня (видна только в Month View -
    // в Week/Day события показываются прямо внутри временной сетки) ---
    m_rightPanel = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(m_rightPanel);

    m_selectedDateLabel = new QLabel(m_rightPanel);
    m_selectedDateLabel->setObjectName("selectedDateLabel");

    m_eventsList = new QListWidget(m_rightPanel);
    m_eventsList->setObjectName("eventsList");

    auto *hintLabel = new QLabel("Double-click an event to edit it", m_rightPanel);
    hintLabel->setObjectName("hintLabel");

    m_addEventButton = new QPushButton("+ Add event", m_rightPanel);
    m_addEventButton->setObjectName("panelButton");
    m_deleteEventButton = new QPushButton("Delete event", m_rightPanel);
    m_deleteEventButton->setObjectName("panelButton");
    m_deleteEventButton->setEnabled(false);
    m_startPomodoroButton = new QPushButton("Start Pomodoro", m_rightPanel);
    m_startPomodoroButton->setObjectName("panelButton");
    m_startPomodoroButton->setEnabled(false);

    // MVP 3.0 - Task Execution Foundation (минимальный технический UI,
    // см. комментарий у объявления в MainWindow.h).
    m_startTaskButton = new QPushButton("Start", m_rightPanel);
    m_startTaskButton->setObjectName("panelButton");
    m_startTaskButton->setEnabled(false);
    m_completeTaskButton = new QPushButton("Complete", m_rightPanel);
    m_completeTaskButton->setObjectName("panelButton");
    m_completeTaskButton->setEnabled(false);
    m_taskStatusLabel = new QLabel(m_rightPanel);
    m_taskStatusLabel->setObjectName("hintLabel");

    rightLayout->addWidget(m_selectedDateLabel);
    rightLayout->addWidget(m_eventsList, 1);
    rightLayout->addWidget(hintLabel);
    rightLayout->addWidget(m_addEventButton);
    rightLayout->addWidget(m_deleteEventButton);
    rightLayout->addWidget(m_startPomodoroButton);
    rightLayout->addWidget(m_startTaskButton);
    rightLayout->addWidget(m_completeTaskButton);
    rightLayout->addWidget(m_taskStatusLabel);

    auto *splitter = new QSplitter(this);
    splitter->addWidget(m_calendarView);
    splitter->addWidget(m_rightPanel);
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

    connect(m_calendarView, &CalendarView::dateSelected, this, &MainWindow::onDateSelected);
    connect(m_addEventButton, &QPushButton::clicked, this, &MainWindow::onAddEventClicked);
    connect(m_deleteEventButton, &QPushButton::clicked, this, &MainWindow::onDeleteEventClicked);
    connect(m_eventsList, &QListWidget::itemSelectionChanged, this, &MainWindow::onEventSelectionChanged);
    connect(m_eventsList, &QListWidget::itemDoubleClicked, this, &MainWindow::onEventDoubleClicked);
    connect(m_startPomodoroButton, &QPushButton::clicked, this, &MainWindow::onStartPomodoroClicked);
    connect(m_startTaskButton, &QPushButton::clicked, this, &MainWindow::onStartTaskClicked);
    connect(m_completeTaskButton, &QPushButton::clicked, this, &MainWindow::onCompleteTaskClicked);
    connect(m_pomodoro, &PomodoroWidget::pomodoroCompletedForEvent, this, &MainWindow::onPomodoroCompletedForEvent);
    connect(m_pomodoro, &PomodoroWidget::startCurrentTaskRequested, this, &MainWindow::onStartCurrentTaskClicked);
    connect(m_calendarView, &CalendarView::visibleRangeChanged, this, &MainWindow::onVisibleRangeChanged);
    connect(m_calendarView, &CalendarView::createEventRequested, this, &MainWindow::onCreateEventRequested);
    connect(m_calendarView, &CalendarView::createEventRequestedWithTime, this, &MainWindow::onCreateEventRequestedWithTime);
    connect(m_calendarView, &CalendarView::editEventRequested, this, &MainWindow::onEditEventRequested);
    connect(m_calendarView, &CalendarView::eventRescheduled, this, &MainWindow::onEventRescheduled);
    connect(m_calendarView, &CalendarView::viewModeChanged, this, &MainWindow::onCalendarViewModeChanged);

    // Инициализируем правую панель для дня, который CalendarView
    // выбрал по умолчанию (сегодня), и сразу подсвечиваем в сетке дни,
    // на которые уже есть события, загруженные из БД с прошлого запуска.
    m_visibleRangeStart = m_calendarView->monthVisibleRangeStart();
    m_visibleRangeEnd = m_calendarView->monthVisibleRangeEnd();
    onDateSelected(m_calendarView->selectedDate());
    refreshCalendarMarkers();

    // NOW screen - onDateSelected() выше уже вызвал refreshEventsList(),
    // который вызывает refreshNowView() (см. её реализацию), так что
    // первый снимок уже готов. Таймер нужен только для случаев, когда
    // пользователь просто сидит на экране и ничего не делает - иначе
    // Missed "проявится" в NOW только при следующем действии в приложении.
    // Раз в минуту достаточно: сама задача не мутирует БД, это чтение +
    // вычисление (см. комментарий у EventManager::nowSnapshot()).
    m_nowRefreshTimer = new QTimer(this);
    m_nowRefreshTimer->setInterval(60000);
    connect(m_nowRefreshTimer, &QTimer::timeout, this, &MainWindow::refreshNowView);
    m_nowRefreshTimer->start();
}

void MainWindow::onDateSelected(const QDate &date)
{
    m_selectedDateLabel->setText(date.toString("dddd, d MMMM yyyy"));
    refreshEventsList();
}

void MainWindow::refreshEventsList()
{
    m_eventsList->clear();

    const QDate date = m_calendarView->selectedDate();
    const QVector<Event> events = m_eventManager.eventsForDate(date);
    const QDateTime now = QDateTime::currentDateTime();

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

        // MVP 3.0 - execution status. Ничего не пишет в БД - effectiveStatus()
        // только читает сохранённое состояние (если есть) и текущее время.
        // Planned - самый частый случай (ничего ещё не произошло), поэтому
        // для него бейдж не показываем - не загромождать список пометкой
        // "и так понятного по умолчанию" состояния.
        const ExecutionStatus status = m_eventManager.effectiveStatus(
            event.id, event.date, event.startTime, event.endTime, now);
        switch (status) {
        case ExecutionStatus::Running:   text += "   \xE2\x8F\xB1 Running"; break;   // ⏱
        case ExecutionStatus::Completed: text += "   \xE2\x9C\x93 Done"; break;      // ✓
        case ExecutionStatus::Missed:    text += "   \xE2\x9A\xA0 Missed"; break;    // ⚠
        case ExecutionStatus::Postponed: text += "   \xE2\x86\xAA Postponed"; break; // ↪ (не выставляется UI пока - раздел 8)
        case ExecutionStatus::Cancelled: text += "   \xE2\x9C\x97 Cancelled"; break;  // ✗ (не выставляется UI пока)
        case ExecutionStatus::Planned:   break;
        }

        auto *item = new QListWidgetItem(text, m_eventsList);
        item->setData(Qt::UserRole, event.id);
        if (!event.description.isEmpty())
            item->setToolTip(event.description);
    }

    m_deleteEventButton->setEnabled(false);
    m_startPomodoroButton->setEnabled(false);
    m_startTaskButton->setEnabled(false);
    m_completeTaskButton->setEnabled(false);
    m_taskStatusLabel->clear();

    refreshNowView();
}

void MainWindow::refreshCalendarMarkers()
{
    if (!m_visibleRangeStart.isValid() || !m_visibleRangeEnd.isValid())
        return;

    const QVector<Event> events = m_eventManager.eventsInRange(m_visibleRangeStart, m_visibleRangeEnd);

    QMap<QDate, QVector<Event>> eventsByDate;
    for (const Event &event : events)
        eventsByDate[event.date].append(event);

    m_calendarView->setEventsForVisibleRange(eventsByDate);
}

void MainWindow::refreshNowView()
{
    const QDateTime now = QDateTime::currentDateTime();
    const NowSnapshot snapshot = m_eventManager.nowSnapshot(now.date(), now);
    m_calendarView->setNowSnapshot(snapshot, now);
}

void MainWindow::onVisibleRangeChanged(const QDate &start, const QDate &end)
{
    m_visibleRangeStart = start;
    m_visibleRangeEnd = end;
    refreshCalendarMarkers();
}

void MainWindow::onCalendarViewModeChanged(CalendarViewMode mode)
{
    // Панель со списком событий имеет смысл только в Month View - в Week/Day
    // события показываются прямо внутри временной сетки, отдельный список
    // там дублировал бы ту же информацию. Pomodoro-панель пока не трогаем -
    // её возможный переезд в шапку окна обсуждали отдельно, в текущие
    // фазы это не входит.
    m_rightPanel->setVisible(mode == CalendarViewMode::Month);
}

void MainWindow::onAddEventClicked()
{
    openNewEventDialog(m_calendarView->selectedDate());
}

void MainWindow::openNewEventDialog(const QDate &defaultDate)
{
    EventDialog dialog(defaultDate, this);
    dialog.setCategories(m_eventManager.allCategories());
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_eventManager.addEvent(dialog.toEvent());
    refreshEventsList();
    refreshCalendarMarkers();
}

void MainWindow::openNewEventDialog(const QDate &defaultDate, const QTime &defaultStartTime)
{
    EventDialog dialog(defaultDate, defaultStartTime, this);
    dialog.setCategories(m_eventManager.allCategories());
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_eventManager.addEvent(dialog.toEvent());
    refreshEventsList();
    refreshCalendarMarkers();
}

void MainWindow::onEventSelectionChanged()
{
    QListWidgetItem *item = m_eventsList->currentItem();
    const bool hasSelection = item != nullptr;
    m_deleteEventButton->setEnabled(hasSelection);
    m_startPomodoroButton->setEnabled(hasSelection);

    if (!hasSelection) {
        m_startTaskButton->setEnabled(false);
        m_completeTaskButton->setEnabled(false);
        m_taskStatusLabel->clear();
        return;
    }

    Event event;
    if (!m_eventManager.eventById(item->data(Qt::UserRole).toInt(), event)) {
        m_startTaskButton->setEnabled(false);
        m_completeTaskButton->setEnabled(false);
        m_taskStatusLabel->clear();
        return;
    }

    // Вхождение - та же дата, что сейчас выбрана в календаре: весь список
    // m_eventsList всегда наполняется для одной конкретной даты (см.
    // refreshEventsList()), так что дата события из БД тут не подходит
    // напрямую для повторяющегося события (event.date после eventById() -
    // это дата ЯКОРЯ серии, а не текущего вхождения).
    const QDate occurrenceDate = m_calendarView->selectedDate();
    const QDateTime now = QDateTime::currentDateTime();
    const ExecutionStatus status = m_eventManager.effectiveStatus(
        event.id, occurrenceDate, event.startTime, event.endTime, now);

    // Start доступен, пока ничего не завершено/не отменено/не перенесено -
    // включая Missed (реально сделать позже пропущенного окна - нормальный
    // сценарий, "лучше поздно, чем никогда"), но не повторно для Running.
    m_startTaskButton->setEnabled(status == ExecutionStatus::Planned || status == ExecutionStatus::Missed);
    // Complete доступен из Planned/Running/Missed - оба пути раздела 4
    // (с предварительным Start и без).
    m_completeTaskButton->setEnabled(
        status == ExecutionStatus::Planned || status == ExecutionStatus::Running || status == ExecutionStatus::Missed);

    switch (status) {
    case ExecutionStatus::Planned:
        m_taskStatusLabel->setText("Not started yet");
        break;
    case ExecutionStatus::Running: {
        EventExecution execution;
        QString since;
        if (m_eventManager.executionForOccurrence(event.id, occurrenceDate, execution) && execution.actualStart.isValid())
            since = QString(" (since %1)").arg(execution.actualStart.toString("HH:mm"));
        m_taskStatusLabel->setText("Running" + since);
        break;
    }
    case ExecutionStatus::Completed:
        m_taskStatusLabel->setText("Completed");
        break;
    case ExecutionStatus::Missed:
        m_taskStatusLabel->setText("Missed");
        break;
    case ExecutionStatus::Postponed:
        m_taskStatusLabel->setText("Postponed");
        break;
    case ExecutionStatus::Cancelled:
        m_taskStatusLabel->setText("Cancelled");
        break;
    }
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
    dialog.setCategories(m_eventManager.allCategories());
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

void MainWindow::onCreateEventRequestedWithTime(const QDate &date, const QTime &time)
{
    openNewEventDialog(date, time);
}

void MainWindow::onEditEventRequested(int eventId)
{
    openEditEventDialog(eventId);
}

void MainWindow::onEventRescheduled(int eventId, const QDate &newDate, const QTime &newStartTime, const QTime &newEndTime)
{
    // TimeGridView уже отфильтровал повторяющиеся события сам (не начинает
    // для них drag), так что eventById() здесь всегда должен найти именно
    // ту единственную запись в БД, которую и нужно подвинуть - не шаблон
    // серии и не какое-то отдельное "вхождение".
    Event event;
    if (!m_eventManager.eventById(eventId, event))
        return; // событие успели удалить, пока тащили - просто ничего не делаем

    event.date = newDate;
    event.startTime = newStartTime;
    event.endTime = newEndTime;

    if (!m_eventManager.updateEvent(event))
        return;

    refreshEventsList();
    refreshCalendarMarkers();
}

void MainWindow::onStartPomodoroClicked()
{
    QListWidgetItem *item = m_eventsList->currentItem();
    if (!item)
        return;

    const int eventId = item->data(Qt::UserRole).toInt();
    // Та же дата, что и у onStartTaskClicked() - event.date из eventById()
    // был бы датой ЯКОРЯ серии для повторяющегося события, а не датой
    // конкретного вхождения, выбранного в списке.
    const QDate occurrenceDate = m_calendarView->selectedDate();

    Event event;
    if (!m_eventManager.eventById(eventId, event))
        return;

    startPomodoroForOccurrence(event, occurrenceDate);
}

void MainWindow::onStartCurrentTaskClicked()
{
    const QDateTime now = QDateTime::currentDateTime();
    const NowSnapshot snapshot = m_eventManager.nowSnapshot(now.date(), now);

    if (!snapshot.hasCurrent) {
        m_pomodoro->showNoCurrentTask();
        return;
    }

    // snapshot.current.event.date - уже дата КОНКРЕТНОГО вхождения (для
    // повторяющегося события - именно та, на которую его развернул
    // eventsForDate() внутри nowSnapshot(), не дата якоря серии), так что
    // отдельно её вычислять не нужно, в отличие от onStartPomodoroClicked() выше.
    startPomodoroForOccurrence(snapshot.current.event, snapshot.current.event.date);
}

void MainWindow::startPomodoroForOccurrence(const Event &event, const QDate &occurrenceDate)
{
    const QDateTime now = QDateTime::currentDateTime();
    const ExecutionStatus status = m_eventManager.effectiveStatus(
        event.id, occurrenceDate, event.startTime, event.endTime, now);

    // Если уже Running - НЕ зовём startOccurrence() повторно: это
    // перезаписало бы actual_start на текущий момент, хотя задача по факту
    // уже началась раньше. Pomodoro же всё равно (пере)запускаем - это его
    // обычная, не меняющаяся семантика повторного Start.
    if (status != ExecutionStatus::Running)
        m_eventManager.startOccurrence(event.id, occurrenceDate, now);

    const QString taskLabel = QString("%1 (%2\xE2\x80\x93%3)")
        .arg(event.title, event.startTime.toString("HH:mm"), event.endTime.toString("HH:mm"));
    m_pomodoro->startForTask(event.id, occurrenceDate, taskLabel);

    refreshEventsList(); // подхватит новый статус (бейдж в списке, NOW - через refreshNowView() внутри)
}

void MainWindow::onPomodoroCompletedForEvent(int eventId, const QDate &occurrenceDate)
{
    // occurrenceDate пока не используется для хранения (incrementPomodoroCount
    // остаётся прежним, не occurrence-aware счётчиком - см. отчёт анализа:
    // полноценная occurrence-aware pomodoro-статистика - отдельная задача,
    // не в этом scope). Параметр уже протащен через сигнал, чтобы эта
    // функция была готова использовать его, когда такая статистика появится,
    // без необходимости снова менять сигнатуру сигнала.
    Q_UNUSED(occurrenceDate);
    m_eventManager.incrementPomodoroCount(eventId);
    refreshEventsList();
}

void MainWindow::onStartTaskClicked()
{
    QListWidgetItem *item = m_eventsList->currentItem();
    if (!item)
        return;

    const int eventId = item->data(Qt::UserRole).toInt();
    const QDate occurrenceDate = m_calendarView->selectedDate(); // см. onEventSelectionChanged()

    m_eventManager.startOccurrence(eventId, occurrenceDate, QDateTime::currentDateTime());
    refreshEventsList();
}

void MainWindow::onCompleteTaskClicked()
{
    QListWidgetItem *item = m_eventsList->currentItem();
    if (!item)
        return;

    const int eventId = item->data(Qt::UserRole).toInt();
    const QDate occurrenceDate = m_calendarView->selectedDate();

    m_eventManager.completeOccurrence(eventId, occurrenceDate, QDateTime::currentDateTime());
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

    if (result.unsupportedFormat) {
        QMessageBox::warning(
            this, "Import failed",
            "This file's column headers don't match a supported format.\n\n"
            "Supported formats:\n"
            "- date,start,end,subject,description\n"
            "- TYPE,CONTENT,DESCRIPTION,PRIORITY,INDENT,DATE,DATE_LANG,TIMEZONE,DURATION,DURATION_UNIT"
        );
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
    m_calendarView->setFirstDayOfWeek(m_settings.sundayFirst());
    applyShortcuts();
}

void MainWindow::onManageCategoriesClicked()
{
    CategoryManagerDialog dialog(&m_eventManager, this);
    dialog.exec(); // CategoryManagerDialog пишет в EventManager сразу по каждому действию

    // Цвет/имя категории у уже загруженных Event - "снимок" на момент чтения
    // из БД (JOIN, не живая ссылка), поэтому после закрытия диалога нужно
    // перечитать события, иначе на экране останутся старые цвета/имена.
    refreshEventsList();
    refreshCalendarMarkers();
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
