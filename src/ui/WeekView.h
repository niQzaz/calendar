#pragma once

#include <QWidget>
#include <QDate>

#include "ICalendarPage.h"

class QLabel;

// Заготовка Week View (Phase D).
//
// Полноценная реализация с временной сеткой, карточками событий, drag/resize
// появится в Phase E. Сейчас класс умеет ровно то, что нужно CalendarView
// уже на этом этапе: заголовок периода и навигацию вперёд/назад по неделям -
// чтобы переключение Month/Week/Day уже сейчас вело себя правдоподобно
// (с настоящими датами), а не как пустая заглушка без состояния.
class WeekView : public QWidget, public ICalendarPage
{
    Q_OBJECT

public:
    explicit WeekView(QWidget *parent = nullptr);

    // --- ICalendarPage ---
    void goToPrevious() override;
    void goToNext() override;
    void goToToday() override;
    QString headerTitle() const override;

private:
    void updatePlaceholderText();

    QDate m_weekStart; // понедельник отображаемой недели
    QLabel *m_placeholderLabel = nullptr;
};
