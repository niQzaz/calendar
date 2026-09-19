#pragma once

#include <QWidget>
#include <QDate>
#include <QVector>
#include <QRect>

#include "models/Event.h"

// Одна ячейка дня в сетке месяца.
//
// Раньше сетка строилась из обычных QPushButton с текстом-числом - это
// было просто, но не позволяло показать сами события внутри ячейки.
// MonthDayCell - по-прежнему один самостоятельный QWidget на ячейку
// (архитектура та же: 42 виджета в QGridLayout), просто с кастомной
// отрисовкой через paintEvent(), что даёт полный контроль над видом:
// номер дня, состояния today/selected/other-month, и до kMaxVisibleEvents
// мини-карточек событий с "+K more" при переполнении.
//
// ВАЖНО (временно, до Phase C): цвета сейчас захардкожены под тёмную тему
// приложения. Когда появится ThemeManager, отрисовка будет брать цвета
// из Theme вместо констант ниже - сам layout/логика останутся прежними.
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

    // Прямоугольники видимых мини-карточек событий, пересчитываются в paintEvent()
    // и используются в eventIdAt() для попадания курсора - чтобы не дублировать
    // геометрию раскладки в двух местах.
    QVector<QRect> m_eventRects;
    QVector<int> m_eventRectIds;
};
