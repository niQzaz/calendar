#pragma once

#include <QMainWindow>
#include <QDate>
#include <QTime>

#include "services/EventManager.h"
#include "services/AppSettings.h"

class CalendarView;
enum class CalendarViewMode;
class PomodoroWidget;
class NotificationService;
class EventReminder;
class ThemeManager;
class QListWidget;
class QListWidgetItem;
class QLabel;
class QPushButton;
class QAction;
class QTimer;

// Главное окно приложения.
//
// QMainWindow даёт готовую структуру (центральная область, возможность
// в будущем добавить тулбар/статусбар), хотя в MVP используется только
// центральный виджет - статусбар понадобится позже, например, для
// уведомлений в Этапе 7.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onDateSelected(const QDate &date);
    void onAddEventClicked();
    void onDeleteEventClicked();
    void onEventSelectionChanged();
    void onEventDoubleClicked(QListWidgetItem *item);
    void onStartPomodoroClicked();
    void onStartCurrentTaskClicked(); // PomodoroWidget::startCurrentTaskRequested()
    void onPomodoroCompletedForEvent(int eventId, const QDate &occurrenceDate);
    void onStartTaskClicked();
    void onCompleteTaskClicked();
    void onImportCsvClicked();
    void onVisibleRangeChanged(const QDate &start, const QDate &end);
    void onSettingsClicked();
    void onManageCategoriesClicked();
    void onEventRescheduled(int eventId, const QDate &newDate, const QTime &newStartTime, const QTime &newEndTime);
    void onCreateEventRequested(const QDate &date);   // двойной клик по пустому месту ячейки (Month)
    void onCreateEventRequestedWithTime(const QDate &date, const QTime &time); // клик по сетке (Week/Day)
    void onEditEventRequested(int eventId);            // двойной клик по мини-карточке события
    void onCalendarViewModeChanged(CalendarViewMode mode);

private:
    void refreshEventsList();
    void refreshCalendarMarkers();

    // NOW screen (продолжение MVP3) - пересчитывает snapshot на "сейчас"
    // и прокидывает в CalendarView. Вызывается из refreshEventsList()
    // (этого достаточно - она уже вызывается после любого действия,
    // способного повлиять на NOW: Start/Complete, создание/правка/удаление
    // события, drag/resize, смена категории) и по таймеру m_nowRefreshTimer -
    // чтобы Missed успевал "проявиться" в UI, даже если пользователь просто
    // сидит на экране и ничего не делает. НЕ пишет ничего в БД - см.
    // комментарий у EventManager::nowSnapshot().
    void refreshNowView();
    void applyShortcuts(); // выставляет QKeySequence каждому QAction из AppSettings

    // Единственное место, где Pomodoro и Execution встречаются - ни
    // PomodoroWidget, ни PomodoroTimer про EventManager не знают (и не
    // должны). Если occurrence ещё не Running - стартует Execution
    // (startOccurrence), actual_start не трогает, если он уже есть
    // (effectiveStatus() == Running). В любом случае - запускает Pomodoro
    // для той же пары (event.id, occurrenceDate). Используется и починенным
    // onStartPomodoroClicked() (выбор из списка), и onStartCurrentTaskClicked()
    // (Current Task из nowSnapshot()) - оба раньше дублировали бы эту логику.
    void startPomodoroForOccurrence(const Event &event, const QDate &occurrenceDate);

    // Общая логика открытия EventDialog - используется и кнопкой/списком
    // (как раньше), и новыми сигналами от Month/Week/Day (двойной клик
    // по пустому месту / по мини-карточке события / клик по сетке),
    // чтобы не дублировать код.
    void openNewEventDialog(const QDate &defaultDate);
    void openNewEventDialog(const QDate &defaultDate, const QTime &defaultStartTime); // Week/Day: время уже известно
    void openEditEventDialog(int eventId);

    CalendarView *m_calendarView = nullptr;
    QWidget *m_rightPanel = nullptr; // скрывается в Week/Day - там события внутри сетки
    QListWidget *m_eventsList = nullptr;
    QLabel *m_selectedDateLabel = nullptr;
    QPushButton *m_addEventButton = nullptr;
    QPushButton *m_deleteEventButton = nullptr;
    QPushButton *m_startPomodoroButton = nullptr;

    // MVP 3.0 - Task Execution Foundation: минимальный технический UI для
    // Start/Complete. Работают с тем же выбранным элементом m_eventsList,
    // что и остальные кнопки этой панели - см. onEventSelectionChanged().
    // Из-за m_rightPanel->setVisible(mode == Month) выше эти кнопки сейчас
    // недостижимы в Week/Day - известное ограничение, зафиксированное
    // отдельно, не то, что решается в рамках NOW screen.
    QPushButton *m_startTaskButton = nullptr;
    QPushButton *m_completeTaskButton = nullptr;
    QLabel *m_taskStatusLabel = nullptr;
    PomodoroWidget *m_pomodoro = nullptr;
    NotificationService *m_notificationService = nullptr;
    EventReminder *m_eventReminder = nullptr;
    ThemeManager *m_themeManager = nullptr;

    // Этап 8: настройки приложения (QSettings-обёртка) и действия
    // с настраиваемыми горячими клавишами.
    AppSettings m_settings;
    QAction *m_newEventAction = nullptr;
    QAction *m_importCsvAction = nullptr;
    QAction *m_goToTodayAction = nullptr;
    QAction *m_togglePomodoroAction = nullptr;

    // Видимый диапазон сетки календаря - обновляется по сигналу
    // CalendarView::visibleRangeChanged, используется в refreshCalendarMarkers().
    QDate m_visibleRangeStart;
    QDate m_visibleRangeEnd;

    // MainWindow владеет единственным экземпляром EventManager на всё
    // приложение. Для MVP этого достаточно - передавать его через
    // конструкторы других виджетов пока не нужно, т.к. только MainWindow
    // работает с событиями напрямую.
    EventManager m_eventManager;

    // NOW screen - периодически (раз в минуту) вызывает refreshNowView()
    // ЧИСТО ДЛЯ ОТОБРАЖЕНИЯ (пересчёт effectiveStatus/Missed), в БД ничего
    // не пишет - раздел 6 Product Context запрещает именно мутацию по
    // таймеру, а не обновление UI по таймеру, это разные вещи.
    QTimer *m_nowRefreshTimer = nullptr;
};
