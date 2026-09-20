#pragma once

#include <QWidget>
#include <QDate>
#include <QTime>
#include <QVector>
#include <QMap>

#include "models/Event.h"
#include "services/Theme.h"

// TimeGridView - общий "движок" временной сетки для Week и Day View.
//
// Не самостоятельное представление - WeekView и DayView создают его внутри
// себя и просто задают, сколько колонок дней показывать (setColumnDates:
// 7 дат для недели, 1 для дня). Вся отрисовка, масштаб, клики и
// преобразование время↔координата - здесь, одним куском кода на оба
// представления, а не продублированы.
//
// Скролл - НЕ здесь: TimeGridView рассчитан на то, что его поместят
// внутрь QScrollArea (см. WeekView/DayView) с setWidgetResizable(true).
// Сам виджет только следит, чтобы его minimumHeight соответствовал текущему
// масштабу (24 часа * hourHeight) - остальное скроллирование делает
// QScrollArea стандартными средствами Qt. Тут переопределён только
// wheelEvent(), чтобы отличить Ctrl+колесо (зум) от обычного колеса
// (обычное - специально возвращается необработанным, чтобы "всплыть"
// до QScrollArea и сработать как обычный скролл).
//
// Почему custom-painted QWidget, а не десятки мелких QWidget на каждую
// линию сетки/событие: и производительность, и то, что вся геометрия
// (сетка, события, в будущих фазах - drag/resize preview) взаимосвязана
// и проще рисуется одним paintEvent, чем синхронизируется между виджетами.
class TimeGridView : public QWidget
{
    Q_OBJECT

public:
    explicit TimeGridView(QWidget *parent = nullptr);

    // Даты колонок слева направо (1 дата для Day, 7 для Week).
    void setColumnDates(const QVector<QDate> &dates);

    // События для отрисовки - ключ: дата, значение: события этой даты.
    // Для каждой колонки TimeGridView сам находит "свои" события по дате
    // из setColumnDates() - тот же формат данных, что и у MonthView,
    // поэтому MainWindow не нужно готовить данные по-разному для разных
    // представлений.
    void setEventsForColumns(const QMap<QDate, QVector<Event>> &eventsByDate);

    void setTheme(const Theme &theme);

    int selectedEventId() const { return m_selectedEventId; }
    void setSelectedEventId(int eventId);

    // Y-координата, соответствующая заданному времени - используется
    // WeekView/DayView, чтобы прокрутить сетку к разумному времени
    // (например, 7:00) при открытии, а не к полуночи.
    int yForTime(const QTime &time) const;

signals:
    // Одиночный клик по событию - просто выделение, ничего не меняет.
    void eventSelected(int eventId);

    // Одиночный клик по пустому месту сетки - запрос создать событие
    // на эту дату и время (уже округлённое до ближайших 15 минут).
    void createEventRequested(const QDate &date, const QTime &time);

    // Двойной клик по событию - запрос открыть его на редактирование.
    void editEventRequested(int eventId);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    QSize sizeHint() const override;

private:
    struct HitEvent
    {
        int id;
        QRect rect;
    };

    void paintColumnHeaders(QPainter &painter, const QRect &headerRect);
    void paintTimeGutter(QPainter &painter, const QRect &gutterRect);
    void paintGridLines(QPainter &painter, const QRect &gridRect);
    void paintCurrentTimeLine(QPainter &painter, const QRect &gridRect);
    void paintEvents(QPainter &painter, const QRect &gridRect);

    QRect computeGridRect() const;
    QRect columnRect(int columnIndex, const QRect &gridRect) const;
    int columnIndexAt(int x, const QRect &gridRect) const;
    int hitTestEvent(const QPoint &pos) const;

    int timeToY(const QTime &time) const;
    QTime yToTime(int y) const;
    static QTime snapToGrid(const QTime &time);

    void updateMinimumHeight();

    static constexpr int kMinHourHeight = 32;
    static constexpr int kMaxHourHeight = 160;
    static constexpr int kHeaderHeight = 32;
    static constexpr int kGutterWidth = 52;
    static constexpr int kSnapMinutes = 15;
    static constexpr int kZoomStepPerNotch = 8;

    QVector<QDate> m_columnDates;
    QVector<QVector<Event>> m_eventsByColumn; // тот же порядок, что m_columnDates
    int m_hourHeight = 56;
    int m_selectedEventId = -1;
    Theme m_theme;

    // Прямоугольники всех сейчас отрисованных событий - для hit-testing
    // клика, пересчитываются заново в paintEvents() на каждой перерисовке.
    QVector<HitEvent> m_eventHitRects;
};
