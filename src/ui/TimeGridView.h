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
// (сетка, события, drag-превью) взаимосвязана и проще рисуется одним
// paintEvent, чем синхронизируется между виджетами.
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

    // Событие перетащили или растянули мышью (MVP2, drag & drop / resize).
    // eventId - id события; newDate/newStartTime/newEndTime - результат:
    // при перемещении меняются дата+начало (длительность та же), при
    // resize за нижний край - только конец (дата и начало прежние).
    // Эмитится только для НЕ повторяющихся событий - см. комментарий
    // в mousePressEvent(), почему повторяющиеся так тащить нельзя.
    void eventRescheduled(int eventId, const QDate &newDate, const QTime &newStartTime, const QTime &newEndTime);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    QSize sizeHint() const override;

private:
    struct HitEvent
    {
        Event event; // полная копия, не только id - нужна для drag (дата/время/recurrenceType)
        QRect rect;
    };

    void paintColumnHeaders(QPainter &painter, const QRect &headerRect);
    void paintTimeGutter(QPainter &painter, const QRect &gutterRect);
    void paintGridLines(QPainter &painter, const QRect &gridRect);
    void paintCurrentTimeLine(QPainter &painter, const QRect &gridRect);
    void paintEvents(QPainter &painter, const QRect &gridRect);
    void paintDragPreview(QPainter &painter, const QRect &gridRect);

    QRect computeGridRect() const;
    QRect columnRect(int columnIndex, const QRect &gridRect) const;
    int columnIndexAt(int x, const QRect &gridRect) const;
    int hitTestEvent(const QPoint &pos) const;
    const HitEvent *hitTestFull(const QPoint &pos) const;

    // Куда попадёт перетаскиваемое событие, если отпустить мышь прямо
    // сейчас, в позиции mousePos - общая логика и для превью во время
    // drag (paintDragPreview), и для применения результата в
    // mouseReleaseEvent, чтобы то, что нарисовано, совпадало с тем,
    // что реально произойдёт. Версия для перемещения (дата+начало,
    // длительность сохраняется) и версия для resize (только конец,
    // дата и начало не меняются) - два разных режима взаимодействия,
    // см. InteractionMode ниже.
    void computeDragTarget(const QPoint &mousePos, QDate &outDate, QTime &outStart, QTime &outEnd) const;
    void computeResizeTarget(const QPoint &mousePos, QTime &outEnd) const;

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
    static constexpr int kDragThresholdPx = 6; // сколько нужно сдвинуть мышь, чтобы клик считался перетаскиванием/resize
    static constexpr int kResizeEdgePx = 8;    // толщина "ручки" resize у нижнего края блока события

    QVector<QDate> m_columnDates;
    QVector<QVector<Event>> m_eventsByColumn; // тот же порядок, что m_columnDates
    int m_hourHeight = 56;
    int m_selectedEventId = -1;
    Theme m_theme;

    // Прямоугольники всех сейчас отрисованных событий - для hit-testing
    // клика, пересчитываются заново в paintEvents() на каждой перерисовке.
    QVector<HitEvent> m_eventHitRects;

    // Состояние взаимодействия мышью (MVP2, drag & drop + resize).
    // InteractionMode выбирается в mousePressEvent() по тому, в какую зону
    // блока события попал клик (нижние kResizeEdgePx пикселей - Resizing,
    // остальное - Moving); None - обычный клик/пустое место, ничего не тащим.
    // Реально "тащим" (и это видно превью) только после того, как мышь
    // сдвинулась больше чем на kDragThresholdPx (m_dragThresholdExceeded) -
    // иначе обычный клик по событию выглядел бы как микро-перетаскивание
    // и норовил бы сдвинуть/сжать событие на пару пикселей.
    enum class InteractionMode { None, Moving, Resizing };
    InteractionMode m_interactionMode = InteractionMode::None;
    bool m_dragThresholdExceeded = false;
    Event m_draggedEvent;      // снимок события на момент начала взаимодействия - оригинальные date/start/end
    QPoint m_dragPressPos;     // позиция мыши в момент mousePressEvent - точка отсчёта для порога
    QPoint m_dragGrabOffset;   // где внутри блока события "схватили" (для Moving) - чтобы блок не прыгал под курсор
    QPoint m_dragCurrentPos;   // текущая позиция мыши во время взаимодействия - для отрисовки превью
};
