#pragma once

#include <QWidget>
#include <QDate>
#include <QVector>
#include <QRect>

#include "models/Event.h"
#include "services/Theme.h"

// Одна ячейка дня в сетке месяца.
//
// MonthDayCell - самостоятельный QWidget на ячейку (42 штуки в QGridLayout
// у CalendarWidget), с кастомной отрисовкой через paintEvent(), что даёт
// полный контроль над видом: номер дня, состояния today/selected/other-month,
// и до kMaxVisibleEvents мини-карточек событий с "+K more" при переполнении.
//
// Цвета берутся из Theme (см. setTheme()), а не захардкожены - при смене
// темы в настройках CalendarWidget прокидывает новую Theme в каждую ячейку,
// и она сама перерисовывается.
class MonthDayCell : public QWidget
{
    Q_OBJECT

public:
    explicit MonthDayCell(QWidget *parent = nullptr);

    void setDate(const QDate &date);
    void setEvents(const QVector<Event> &events); // уже отсортированы по startTime
    void setToday(bool isToday);
    void setSelected(bool selected);
    void setOtherMonth(bool otherMonth);
    void setTheme(const Theme &theme);

    QDate date() const { return m_date; }

    QSize sizeHint() const override;

signals:
    // Одиночный клик в любом месте ячейки - выбор дня (без уточнения,
    // по событию кликнули или по пустому месту - для выбора дня это неважно).
    void clicked(const QDate &date);

    // Двойной клик по пустому месту ячейки - запрос на создание события в этот день.
    void createEventRequested(const QDate &date);

    // Двойной клик по конкретной мини-карточке события - запрос на её редактирование.
    void editEventRequested(int eventId);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    static constexpr int kMaxVisibleEvents = 3;

    // Ищет, под каким из отрисованных прямоугольников событий находится точка.
    // Возвращает id события или -1, если точка попала на пустое место ячейки.
    int eventIdAt(const QPoint &pos) const;

    QDate m_date;
    QVector<Event> m_events;
    bool m_isToday = false;
    bool m_isSelected = false;
    bool m_isOtherMonth = false;
    Theme m_theme = {}; // до первого setTheme() - пустая (чёрная) тема, но виджет
                        // ещё не показан к этому моменту, так что не видно

    // Прямоугольники видимых мини-карточек событий, пересчитываются в paintEvent()
    // и используются в eventIdAt() для попадания курсора - чтобы не дублировать
    // геометрию раскладки в двух местах.
    QVector<QRect> m_eventRects;
    QVector<int> m_eventRectIds;
};
