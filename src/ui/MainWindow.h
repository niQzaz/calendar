#pragma once

#include <QMainWindow>
#include <QDate>

#include "services/EventManager.h"

class CalendarWidget;
class PomodoroWidget;
class QListWidget;
class QListWidgetItem;
class QLabel;
class QPushButton;

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

private:
    void refreshEventsList();
    void refreshCalendarMarkers();

    CalendarWidget *m_calendar = nullptr;
    QListWidget *m_eventsList = nullptr;
    QLabel *m_selectedDateLabel = nullptr;
    QPushButton *m_addEventButton = nullptr;
    QPushButton *m_deleteEventButton = nullptr;
    QPushButton *m_startPomodoroButton = nullptr;
    PomodoroWidget *m_pomodoro = nullptr;

    // MainWindow владеет единственным экземпляром EventManager на всё
    // приложение. Для MVP этого достаточно - передавать его через
    // конструкторы других виджетов пока не нужно, т.к. только MainWindow
    // работает с событиями напрямую.
    EventManager m_eventManager;
};
