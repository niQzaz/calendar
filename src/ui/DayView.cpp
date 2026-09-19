#include "DayView.h"

#include <QLabel>
#include <QVBoxLayout>

DayView::DayView(QWidget *parent)
    : QWidget(parent)
    , m_date(QDate::currentDate())
{
    m_placeholderLabel = new QLabel(this);
    m_placeholderLabel->setAlignment(Qt::AlignCenter);
    m_placeholderLabel->setWordWrap(true);
    m_placeholderLabel->setObjectName("hintLabel");

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_placeholderLabel);

    updatePlaceholderText();
}

void DayView::goToPrevious()
{
    m_date = m_date.addDays(-1);
    updatePlaceholderText();
}

void DayView::goToNext()
{
    m_date = m_date.addDays(1);
    updatePlaceholderText();
}

void DayView::goToToday()
{
    m_date = QDate::currentDate();
    updatePlaceholderText();
}

QString DayView::headerTitle() const
{
    return m_date.toString("dddd, d MMMM yyyy");
}

void DayView::updatePlaceholderText()
{
    m_placeholderLabel->setText(
        QString("Day view is coming in Phase F.\n\nCurrently showing: %1").arg(headerTitle())
    );
}
