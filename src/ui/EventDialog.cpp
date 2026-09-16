#include "EventDialog.h"

#include <QLineEdit>
#include <QDateEdit>
#include <QTimeEdit>
#include <QPlainTextEdit>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QMessageBox>

EventDialog::EventDialog(const QDate &defaultDate, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("New event");

    m_titleEdit = new QLineEdit(this);
    m_titleEdit->setPlaceholderText("Event title");

    m_dateEdit = new QDateEdit(defaultDate, this);
    m_dateEdit->setCalendarPopup(true);
    m_dateEdit->setDisplayFormat("dd.MM.yyyy");

    m_startTimeEdit = new QTimeEdit(QTime(9, 0), this);
    m_endTimeEdit = new QTimeEdit(QTime(10, 0), this);
    m_startTimeEdit->setDisplayFormat("HH:mm");
    m_endTimeEdit->setDisplayFormat("HH:mm");

    m_descriptionEdit = new QPlainTextEdit(this);
    m_descriptionEdit->setPlaceholderText("Description (optional)");
    m_descriptionEdit->setFixedHeight(80);

    auto *formLayout = new QFormLayout();
    formLayout->addRow("Title", m_titleEdit);
    formLayout->addRow("Date", m_dateEdit);
    formLayout->addRow("Start", m_startTimeEdit);
    formLayout->addRow("End", m_endTimeEdit);
    formLayout->addRow("Description", m_descriptionEdit);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->addLayout(formLayout);
    rootLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &EventDialog::onAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    setMinimumWidth(320);
}

void EventDialog::onAccept()
{
    if (m_titleEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Invalid event", "Please enter an event title.");
        return;
    }

    if (m_startTimeEdit->time() >= m_endTimeEdit->time()) {
        QMessageBox::warning(this, "Invalid event", "End time must be after start time.");
        return;
    }

    accept();
}

Event EventDialog::toEvent() const
{
    Event result;
    result.title = m_titleEdit->text().trimmed();
    result.date = m_dateEdit->date();
    result.startTime = m_startTimeEdit->time();
    result.endTime = m_endTimeEdit->time();
    result.description = m_descriptionEdit->toPlainText().trimmed();
    return result;
}
