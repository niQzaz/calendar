#pragma once

#include <QWidget>
#include <QDate>

#include "ICalendarPage.h"

class QLabel;

// Заготовка Day View (Phase D) - см. подробный комментарий в WeekView.h,
// та же идея, только период - один день, а не неделя. Полноценная
// реализация (Phase F) переиспользует общую логику с WeekView, а не
// дублирует её - но это будет видно только когда появится сама
// временная сетка.
class DayView : public QWidget, public ICalendarPage
{
    Q_OBJECT

public:
    explicit DayView(QWidget *parent = nullptr);

    // --- ICalendarPage ---
    void goToPrevious() override;
    void goToNext() override;
    void goToToday() override;
    QString headerTitle() const override;

private:
    void updatePlaceholderText();

    QDate m_date;
    QLabel *m_placeholderLabel = nullptr;
};
