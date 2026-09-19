#include "WeekView.h"

#include <QLabel>
#include <QVBoxLayout>

namespace {
QDate startOfWeek(const QDate &date)
{
    // Понедельник этой недели. Настройка "первый день недели" (Этап 8)
    // на заголовок периода не влияет - в Phase E, когда появится сама
    // сетка недели, она будет учитываться отдельно, как в MonthView.
    return date.addDays(-(date.dayOfWeek() - 1));
}
}

WeekView::WeekView(QWidget *parent)
    : QWidget(parent)
    , m_weekStart(startOfWeek(QDate::currentDate()))
{
    m_placeholderLabel = new QLabel(this);
    m_placeholderLabel->setAlignment(Qt::AlignCenter);
    m_placeholderLabel->setWordWrap(true);
    m_placeholderLabel->setObjectName("hintLabel");

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_placeholderLabel);

    updatePlaceholderText();
}

void WeekView::goToPrevious()
{
    m_weekStart = m_weekStart.addDays(-7);
    updatePlaceholderText();
}

void WeekView::goToNext()
{
    m_weekStart = m_weekStart.addDays(7);
    updatePlaceholderText();
}

void WeekView::goToToday()
{
    m_weekStart = startOfWeek(QDate::currentDate());
    updatePlaceholderText();
}

QString WeekView::headerTitle() const
{
    const QDate weekEnd = m_weekStart.addDays(6);

    if (m_weekStart.month() == weekEnd.month()) {
        return QString("%1 - %2 %3")
            .arg(m_weekStart.day())
            .arg(weekEnd.day())
            .arg(weekEnd.toString("MMMM yyyy"));
    }

    return QString("%1 - %2").arg(m_weekStart.toString("d MMMM")).arg(weekEnd.toString("d MMMM yyyy"));
}

void WeekView::updatePlaceholderText()
{
    m_placeholderLabel->setText(
        QString("Week view is coming in Phase E.\n\nCurrently showing: %1").arg(headerTitle())
    );
}
