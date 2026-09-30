#pragma once

#include <QWidget>
#include <QDateTime>

#include "ICalendarPage.h"
#include "models/Execution.h"
#include "services/Theme.h"

class QLabel;

// NOW screen (продолжение MVP3 после MVP3.0 - Task Execution Foundation).
//
// Показывает текущую и следующую задачу на сегодня с отклонением от
// плана, плюс краткую сводку дня ("X из Y сделано"). В отличие от
// Month/Week/Day, NOW не про "период, по которому можно листать" - это
// всегда "сегодня и сейчас", поэтому goToPrevious()/goToNext()/goToToday()
// здесь no-op (см. .cpp) - реализованы только чтобы NowView мог, как и
// остальные три представления, участвовать в общей навигационной шапке
// CalendarView через ICalendarPage, без специальных исключений там.
//
// Чистый display-виджет, как и MonthView/WeekView/DayView: сам не читает
// EventManager и не заводит свой таймер - обновляется через setSnapshot(),
// которую периодически (раз в минуту) и по факту любых изменений
// (Start/Complete, редактирование, drag...) вызывает MainWindow. Это
// сохраняет существующий принцип "виджеты представления не лезут в БД
// напрямую" (см. комментарий в CalendarView.h) и соответствует разделу 6
// Product Context: обновление отображения - не то же самое, что мутация БД.
class NowView : public QWidget, public ICalendarPage
{
    Q_OBJECT

public:
    explicit NowView(QWidget *parent = nullptr);

    // --- ICalendarPage ---
    void goToPrevious() override; // no-op - NOW не листается
    void goToNext() override;     // no-op
    void goToToday() override;    // no-op - NOW и так всегда "сегодня"
    QString headerTitle() const override;

    // snapshot - результат EventManager::nowSnapshot(); asOf - момент,
    // на который он посчитан (используется только для заголовка).
    void setSnapshot(const NowSnapshot &snapshot, const QDateTime &asOf);
    void setTheme(const Theme &theme);

private:
    void applyPalette(); // перекрашивает уже существующие виджеты в цвета m_theme
    QWidget *buildCard(const QString &kicker, QLabel *&titleLabel, QLabel *&timeLabel, QLabel *&statusLabel);

    QLabel *m_currentKickerLabel = nullptr;
    QLabel *m_currentTitleLabel = nullptr;
    QLabel *m_currentTimeLabel = nullptr;
    QLabel *m_currentStatusLabel = nullptr;
    QWidget *m_currentCard = nullptr;

    QLabel *m_nextKickerLabel = nullptr;
    QLabel *m_nextTitleLabel = nullptr;
    QLabel *m_nextTimeLabel = nullptr;
    QLabel *m_nextStatusLabel = nullptr;
    QWidget *m_nextCard = nullptr;

    QLabel *m_summaryLabel = nullptr;

    Theme m_theme;
    QDateTime m_asOf; // для headerTitle()
};
