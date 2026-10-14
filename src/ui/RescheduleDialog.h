#pragma once

#include <QDialog>
#include <QDate>
#include <QDateTime>
#include <QTime>
#include <QVector>

#include "services/ReschedulePresets.h"

class QPushButton;
class QDateEdit;
class QTimeEdit;
class QLabel;
class QVBoxLayout;

// Этап C - диалог переноса одного вхождения задачи.
//
// Сам ничего не пишет в БД: только собирает новые дату/время и отдаёт их
// через newDate()/newStart()/newEnd(), а MainWindow вызывает
// EventManager::rescheduleOccurrence() (все правила статусов и работа с
// повторениями - там, не здесь).
//
// Два способа выбора:
//  - быстрые варианты (ReschedulePresets.h) - клик сразу принимает диалог;
//  - ручной выбор даты и времени - принимается кнопкой "Reschedule".
class RescheduleDialog : public QDialog
{
    Q_OBJECT

public:
    // title - название задачи (показывается сверху), occurrenceDate/start/end -
    // текущий слот вхождения, now - текущий момент (тот же, что MainWindow
    // потом передаёт в rescheduleOccurrence(), чтобы расчёты были согласованы).
    RescheduleDialog(const QString &title,
                     const QDate &occurrenceDate,
                     const QTime &start,
                     const QTime &end,
                     const QDateTime &now,
                     QWidget *parent = nullptr);

    // Валидны только после exec() == QDialog::Accepted.
    QDate newDate() const { return m_resultDate; }
    QTime newStart() const { return m_resultStart; }
    QTime newEnd() const { return m_resultEnd; }

private slots:
    void onManualStartChanged();
    void updateManualState();
    void onManualAccept();

private:
    void buildPresetButtons(QWidget *parent, QVBoxLayout *layout);
    void applyPreset(int index);
    bool isSameAsCurrentSlot(const QDate &date, const QTime &start, const QTime &end) const;

    QDate m_origDate;
    QTime m_origStart;
    QTime m_origEnd;
    QDateTime m_now;
    int m_durationMinutes = 0;

    QDate m_resultDate;
    QTime m_resultStart;
    QTime m_resultEnd;

    QVector<RescheduleTarget> m_presetTargets; // параллельно кнопкам пресетов

    QDateEdit *m_dateEdit = nullptr;
    QTimeEdit *m_startEdit = nullptr;
    QTimeEdit *m_endEdit = nullptr;
    QLabel *m_hintLabel = nullptr;
    QPushButton *m_okButton = nullptr;
};
