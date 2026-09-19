#pragma once

#include <QMainWindow>
#include <QDate>

#include "services/EventManager.h"
#include "services/AppSettings.h"

class CalendarWidget;
class PomodoroWidget;
class NotificationService;
class EventReminder;
class QListWidget;
class QListWidgetItem;
class QLabel;
class QPushButton;
class QAction;

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
    void onPomodoroCompletedForEvent(int eventId);
    void onImportCsvClicked();
    void onVisibleRangeChanged(const QDate &start, const QDate &end);
    void onSettingsClicked();
    void onCreateEventRequested(const QDate &date);   // двойной клик по пустому месту ячейки
    void onEditEventRequested(int eventId);            // двойной клик по мини-карточке события

private:
    void refreshEventsList();
    void refreshCalendarMarkers();
    void applyShortcuts(); // выставляет QKeySequence каждому QAction из AppSettings

    // Общая логика открытия EventDialog - используется и кнопкой/списком
    // (как раньше), и новыми сигналами от ячеек Month View (двойной клик
    // по пустому месту / по мини-карточке события), чтобы не дублировать код.
    void openNewEventDialog(const QDate &defaultDate);
    void openEditEventDialog(int eventId);

    CalendarWidget *m_calendar = nullptr;
    QListWidget *m_eventsList = nullptr;
    QLabel *m_selectedDateLabel = nullptr;
    QPushButton *m_addEventButton = nullptr;
    QPushButton *m_deleteEventButton = nullptr;
    QPushButton *m_startPomodoroButton = nullptr;
    PomodoroWidget *m_pomodoro = nullptr;
    NotificationService *m_notificationService = nullptr;
    EventReminder *m_eventReminder = nullptr;

    // Этап 8: настройки приложения (QSettings-обёртка) и действия
    // с настраиваемыми горячими клавишами.
    AppSettings m_settings;
    QAction *m_newEventAction = nullptr;
    QAction *m_importCsvAction = nullptr;
    QAction *m_goToTodayAction = nullptr;
    QAction *m_togglePomodoroAction = nullptr;

    // Видимый диапазон сетки календаря - обновляется по сигналу
    // CalendarWidget::visibleRangeChanged, используется в refreshCalendarMarkers().
    QDate m_visibleRangeStart;
    QDate m_visibleRangeEnd;

    // MainWindow владеет единственным экземпляром EventManager на всё
    // приложение. Для MVP этого достаточно - передавать его через
    // конструкторы других виджетов пока не нужно, т.к. только MainWindow
    // работает с событиями напрямую.
    EventManager m_eventManager;
};
