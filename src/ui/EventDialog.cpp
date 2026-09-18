#include "EventDialog.h"

#include <QLineEdit>
#include <QDateEdit>
#include <QTimeEdit>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QMessageBox>

EventDialog::EventDialog(const QDate &defaultDate, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("New event");
    buildForm();

    m_dateEdit->setDate(defaultDate);
    m_startTimeEdit->setTime(QTime(9, 0));
    m_endTimeEdit->setTime(QTime(10, 0));
}

EventDialog::EventDialog(const Event &eventToEdit, QWidget *parent)
    : QDialog(parent)
{
    // Дата в форме - это дата ПЕРВОГО повторения серии (см. Event.h),
    // она может отличаться от того конкретного дня, по которому
    // пользователь кликнул в списке - поэтому явно поясняем в заголовке,
    // что редактируется вся серия, а не один день.
    setWindowTitle(eventToEdit.recurrenceType == RecurrenceType::None
        ? "Edit event"
        : "Edit event (edits entire series)");
    buildForm();

    m_editingId = eventToEdit.id;
    m_titleEdit->setText(eventToEdit.title);
    m_dateEdit->setDate(eventToEdit.date);
    m_startTimeEdit->setTime(eventToEdit.startTime);
    m_endTimeEdit->setTime(eventToEdit.endTime);
    m_descriptionEdit->setPlainText(eventToEdit.description);

    const int comboIndex = m_recurrenceCombo->findData(static_cast<int>(eventToEdit.recurrenceType));
    if (comboIndex >= 0)
        m_recurrenceCombo->setCurrentIndex(comboIndex); // сам включит/выключит спинбокс интервала

    m_customIntervalSpin->setValue(eventToEdit.recurrenceInterval);

    const bool hasEndDate = eventToEdit.recurrenceEndDate.isValid();
    m_hasEndDateCheck->setChecked(hasEndDate); // сам включит/выключит m_recurrenceEndDateEdit
    if (hasEndDate)
        m_recurrenceEndDateEdit->setDate(eventToEdit.recurrenceEndDate);
}

void EventDialog::buildForm()
{
    m_titleEdit = new QLineEdit(this);
    m_titleEdit->setPlaceholderText("Event title");

    m_dateEdit = new QDateEdit(this);
    m_dateEdit->setCalendarPopup(true);
    m_dateEdit->setDisplayFormat("dd.MM.yyyy");

    m_startTimeEdit = new QTimeEdit(this);
    m_endTimeEdit = new QTimeEdit(this);
    m_startTimeEdit->setDisplayFormat("HH:mm");
    m_endTimeEdit->setDisplayFormat("HH:mm");

    m_descriptionEdit = new QPlainTextEdit(this);
    m_descriptionEdit->setPlaceholderText("Description (optional)");
    m_descriptionEdit->setFixedHeight(80);

    // --- Повторение ---
    m_recurrenceCombo = new QComboBox(this);
    m_recurrenceCombo->addItem("Does not repeat", static_cast<int>(RecurrenceType::None));
    m_recurrenceCombo->addItem("Daily", static_cast<int>(RecurrenceType::Daily));
    m_recurrenceCombo->addItem("Weekdays (Mon\xE2\x80\x93Fri)", static_cast<int>(RecurrenceType::Weekdays));
    m_recurrenceCombo->addItem("Weekly", static_cast<int>(RecurrenceType::Weekly));
    m_recurrenceCombo->addItem("Custom interval", static_cast<int>(RecurrenceType::Custom));
    connect(m_recurrenceCombo, &QComboBox::currentIndexChanged, this, &EventDialog::onRecurrenceTypeChanged);

    m_customIntervalSpin = new QSpinBox(this);
    m_customIntervalSpin->setRange(1, 365);
    m_customIntervalSpin->setValue(2);
    m_customIntervalSpin->setSuffix(" day(s)");
    m_customIntervalSpin->setEnabled(false); // включится, только если выбран Custom interval

    m_hasEndDateCheck = new QCheckBox("Ends on", this);
    m_recurrenceEndDateEdit = new QDateEdit(QDate::currentDate().addMonths(1), this);
    m_recurrenceEndDateEdit->setCalendarPopup(true);
    m_recurrenceEndDateEdit->setDisplayFormat("dd.MM.yyyy");
    m_recurrenceEndDateEdit->setEnabled(false);
    connect(m_hasEndDateCheck, &QCheckBox::toggled, m_recurrenceEndDateEdit, &QWidget::setEnabled);

    auto *endDateRow = new QWidget(this);
    auto *endDateLayout = new QHBoxLayout(endDateRow);
    endDateLayout->setContentsMargins(0, 0, 0, 0);
    endDateLayout->addWidget(m_hasEndDateCheck);
    endDateLayout->addWidget(m_recurrenceEndDateEdit);

    auto *formLayout = new QFormLayout();
    formLayout->addRow("Title", m_titleEdit);
    formLayout->addRow("Date", m_dateEdit);
    formLayout->addRow("Start", m_startTimeEdit);
    formLayout->addRow("End", m_endTimeEdit);
    formLayout->addRow("Description", m_descriptionEdit);
    formLayout->addRow("Repeat", m_recurrenceCombo);
    formLayout->addRow("Every", m_customIntervalSpin);
    formLayout->addRow("", endDateRow);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->addLayout(formLayout);
    rootLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &EventDialog::onAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    setMinimumWidth(320);
}

void EventDialog::onRecurrenceTypeChanged(int index)
{
    const auto type = static_cast<RecurrenceType>(m_recurrenceCombo->itemData(index).toInt());
    m_customIntervalSpin->setEnabled(type == RecurrenceType::Custom);
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

    if (m_hasEndDateCheck->isChecked() && m_recurrenceEndDateEdit->date() < m_dateEdit->date()) {
        QMessageBox::warning(this, "Invalid event", "Recurrence end date must not be earlier than the event date.");
        return;
    }

    accept();
}

Event EventDialog::toEvent() const
{
    Event result;
    result.id = m_editingId;
    result.title = m_titleEdit->text().trimmed();
    result.date = m_dateEdit->date();
    result.startTime = m_startTimeEdit->time();
    result.endTime = m_endTimeEdit->time();
    result.description = m_descriptionEdit->toPlainText().trimmed();

    result.recurrenceType = static_cast<RecurrenceType>(m_recurrenceCombo->currentData().toInt());
    result.recurrenceInterval = m_customIntervalSpin->value();
    result.recurrenceEndDate = m_hasEndDateCheck->isChecked() ? m_recurrenceEndDateEdit->date() : QDate();

    return result;
}
