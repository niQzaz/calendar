#include "RescheduleDialog.h"

#include <QDateEdit>
#include <QTimeEdit>
#include <QLabel>
#include <QPushButton>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QFrame>

namespace {

struct PresetInfo
{
    ReschedulePreset preset;
    const char *caption;
};

constexpr PresetInfo kPresets[] = {
    { ReschedulePreset::In15Min,    "In 15 minutes" },
    { ReschedulePreset::In30Min,    "In 30 minutes" },
    { ReschedulePreset::InOneHour,  "In 1 hour" },
    { ReschedulePreset::LaterToday, "Later today" },
    { ReschedulePreset::Tomorrow,   "Tomorrow" },
};

int minutesOfDay(const QTime &time)
{
    return time.hour() * 60 + time.minute();
}

} // namespace

RescheduleDialog::RescheduleDialog(const QString &title,
                                   const QDate &occurrenceDate,
                                   const QTime &start,
                                   const QTime &end,
                                   const QDateTime &now,
                                   QWidget *parent)
    : QDialog(parent)
    , m_origDate(occurrenceDate)
    , m_origStart(start)
    , m_origEnd(end)
    , m_now(now)
    , m_durationMinutes(minutesOfDay(end) - minutesOfDay(start))
{
    setWindowTitle("Reschedule");

    auto *rootLayout = new QVBoxLayout(this);

    // --- Что переносим ---
    auto *infoLabel = new QLabel(
        QString("%1\n%2, %3\xE2\x80\x93%4")
            .arg(title,
                 occurrenceDate.toString("ddd d MMM yyyy"),
                 start.toString("HH:mm"),
                 end.toString("HH:mm")),
        this);
    infoLabel->setTextFormat(Qt::PlainText); // название задачи - произвольный текст пользователя
    infoLabel->setWordWrap(true);
    rootLayout->addWidget(infoLabel);

    // --- Быстрые варианты ---
    rootLayout->addWidget(new QLabel("Quick options", this));
    buildPresetButtons(this, rootLayout);

    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    rootLayout->addWidget(separator);

    // --- Ручной выбор ---
    rootLayout->addWidget(new QLabel("Pick date and time", this));

    m_dateEdit = new QDateEdit(m_origDate, this);
    m_dateEdit->setCalendarPopup(true);
    m_dateEdit->setDisplayFormat("dd.MM.yyyy");

    m_startEdit = new QTimeEdit(m_origStart, this);
    m_startEdit->setDisplayFormat("HH:mm");
    m_endEdit = new QTimeEdit(m_origEnd, this);
    m_endEdit->setDisplayFormat("HH:mm");

    auto *formLayout = new QFormLayout();
    formLayout->addRow("Date", m_dateEdit);
    formLayout->addRow("Start", m_startEdit);
    formLayout->addRow("End", m_endEdit);
    rootLayout->addLayout(formLayout);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setWordWrap(true);
    rootLayout->addWidget(m_hintLabel);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okButton = buttonBox->button(QDialogButtonBox::Ok);
    m_okButton->setText("Reschedule");
    rootLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &RescheduleDialog::onManualAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_startEdit, &QTimeEdit::timeChanged, this, &RescheduleDialog::onManualStartChanged);
    connect(m_endEdit, &QTimeEdit::timeChanged, this, &RescheduleDialog::updateManualState);
    connect(m_dateEdit, &QDateEdit::dateChanged, this, &RescheduleDialog::updateManualState);

    updateManualState();
    setMinimumWidth(340);
}

void RescheduleDialog::buildPresetButtons(QWidget *parent, QVBoxLayout *layout)
{
    for (int i = 0; i < static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0])); ++i) {
        const RescheduleTarget target = computePresetTarget(kPresets[i].preset, m_origStart, m_origEnd, m_now);
        m_presetTargets.append(target);

        QString caption = QString::fromLatin1(kPresets[i].caption);
        QString disabledReason;

        if (!target.valid) {
            disabledReason = "Doesn't fit in today";
            if (kPresets[i].preset == ReschedulePreset::Tomorrow)
                disabledReason = "Not available";
        } else {
            // Показываем вычисленный результат - особенно важно для "через N
            // минут", где начало округляется до 5 минут.
            if (target.date == m_now.date())
                caption += QString("  \xE2\x80\x94  %1").arg(target.start.toString("HH:mm"));
            else
                caption += QString("  \xE2\x80\x94  %1, %2").arg(target.date.toString("ddd d MMM"),
                                                               target.start.toString("HH:mm"));

            if (isSameAsCurrentSlot(target.date, target.start, target.end))
                disabledReason = "Same as the current time";
        }

        auto *button = new QPushButton(caption, parent);
        button->setAutoDefault(false); // Enter должен подтверждать ручной выбор, а не первый пресет
        if (!disabledReason.isEmpty()) {
            button->setEnabled(false);
            button->setToolTip(disabledReason);
        }

        connect(button, &QPushButton::clicked, this, [this, i]() { applyPreset(i); });
        layout->addWidget(button);
    }
}

void RescheduleDialog::applyPreset(int index)
{
    if (index < 0 || index >= m_presetTargets.size())
        return;

    const RescheduleTarget &target = m_presetTargets[index];
    if (!target.valid)
        return;

    m_resultDate = target.date;
    m_resultStart = target.start;
    m_resultEnd = target.end;
    accept();
}

bool RescheduleDialog::isSameAsCurrentSlot(const QDate &date, const QTime &start, const QTime &end) const
{
    return date == m_origDate
        && minutesOfDay(start) == minutesOfDay(m_origStart)
        && minutesOfDay(end) == minutesOfDay(m_origEnd);
}

void RescheduleDialog::onManualStartChanged()
{
    // Конец следует за началом с прежней длительностью - если помещается в
    // день. Иначе конец не трогаем, а updateManualState() покажет проблему.
    const int newEndMinutes = minutesOfDay(m_startEdit->time()) + m_durationMinutes;
    if (m_durationMinutes > 0 && newEndMinutes <= 23 * 60 + 59)
        m_endEdit->setTime(QTime(newEndMinutes / 60, newEndMinutes % 60));

    updateManualState();
}

void RescheduleDialog::updateManualState()
{
    const QDate date = m_dateEdit->date();
    const QTime start = m_startEdit->time();
    const QTime end = m_endEdit->time();

    if (minutesOfDay(end) <= minutesOfDay(start)) {
        m_hintLabel->setText("End must be after start.");
        m_okButton->setEnabled(false);
        return;
    }

    if (isSameAsCurrentSlot(date, start, end)) {
        m_hintLabel->setText("This is the current time - pick a different one.");
        m_okButton->setEnabled(false);
        return;
    }

    if (QDateTime(date, start) < m_now)
        m_hintLabel->setText("This time is in the past. The task may appear as Missed.");
    else
        m_hintLabel->clear();

    m_okButton->setEnabled(true);
}

void RescheduleDialog::onManualAccept()
{
    // Подстраховка: кнопка уже отключена при невалидном вводе, но Enter в
    // полях ввода не должен проходить мимо той же проверки.
    if (!m_okButton->isEnabled())
        return;

    m_resultDate = m_dateEdit->date();
    m_resultStart = m_startEdit->time();
    m_resultEnd = m_endEdit->time();
    accept();
}
