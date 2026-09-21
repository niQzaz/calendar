#include "TimeGridView.h"
#include "EventLayout.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFontMetrics>
#include <algorithm>

TimeGridView::TimeGridView(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    updateMinimumHeight();
}

QSize TimeGridView::sizeHint() const
{
    return QSize(400, kHeaderHeight + 24 * m_hourHeight);
}

void TimeGridView::updateMinimumHeight()
{
    setMinimumHeight(kHeaderHeight + 24 * m_hourHeight);
}

void TimeGridView::setColumnDates(const QVector<QDate> &dates)
{
    m_columnDates = dates;
    m_eventsByColumn.clear();
    m_eventsByColumn.resize(dates.size());
    update();
}

void TimeGridView::setEventsForColumns(const QMap<QDate, QVector<Event>> &eventsByDate)
{
    m_eventsByColumn.clear();
    m_eventsByColumn.resize(m_columnDates.size());
    for (int i = 0; i < m_columnDates.size(); ++i)
        m_eventsByColumn[i] = eventsByDate.value(m_columnDates[i]);
    update();
}

void TimeGridView::setTheme(const Theme &theme)
{
    m_theme = theme;
    update();
}

void TimeGridView::setSelectedEventId(int eventId)
{
    if (m_selectedEventId == eventId)
        return;
    m_selectedEventId = eventId;
    update();
}

int TimeGridView::timeToY(const QTime &time) const
{
    const QRect grid = computeGridRect();
    const double minutesFromMidnight = time.hour() * 60 + time.minute();
    return grid.top() + static_cast<int>(minutesFromMidnight / 60.0 * m_hourHeight);
}

QTime TimeGridView::yToTime(int y) const
{
    const QRect grid = computeGridRect();
    const int relativeY = y - grid.top();
    const int totalMinutes = qBound(0, static_cast<int>(relativeY / double(m_hourHeight) * 60.0), 24 * 60 - 1);
    return QTime(totalMinutes / 60, totalMinutes % 60);
}

QTime TimeGridView::snapToGrid(const QTime &time)
{
    const int totalMinutes = time.hour() * 60 + time.minute();
    const int snapped = ((totalMinutes + kSnapMinutes / 2) / kSnapMinutes) * kSnapMinutes;
    const int clamped = qBound(0, snapped, 24 * 60 - kSnapMinutes);
    return QTime(clamped / 60, clamped % 60);
}

int TimeGridView::yForTime(const QTime &time) const
{
    return timeToY(time);
}

QRect TimeGridView::computeGridRect() const
{
    return QRect(kGutterWidth, kHeaderHeight, qMax(0, width() - kGutterWidth), qMax(0, height() - kHeaderHeight));
}

QRect TimeGridView::columnRect(int columnIndex, const QRect &gridRect) const
{
    if (m_columnDates.isEmpty())
        return gridRect;
    const int columnWidth = gridRect.width() / m_columnDates.size();
    return QRect(gridRect.left() + columnIndex * columnWidth, gridRect.top(), columnWidth, gridRect.height());
}

int TimeGridView::columnIndexAt(int x, const QRect &gridRect) const
{
    if (m_columnDates.isEmpty() || x < gridRect.left() || x >= gridRect.right())
        return -1;
    const int columnWidth = gridRect.width() / m_columnDates.size();
    if (columnWidth <= 0)
        return -1;
    const int index = (x - gridRect.left()) / columnWidth;
    return qBound(0, index, m_columnDates.size() - 1);
}

int TimeGridView::hitTestEvent(const QPoint &pos) const
{
    for (const HitEvent &hit : m_eventHitRects) {
        if (hit.rect.contains(pos))
            return hit.id;
    }
    return -1;
}

void TimeGridView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), m_theme.background);

    const QRect gridRect = computeGridRect();
    const QRect headerRect(kGutterWidth, 0, gridRect.width(), kHeaderHeight);
    const QRect gutterRect(0, kHeaderHeight, kGutterWidth, gridRect.height());

    paintGridLines(painter, gridRect);
    paintTimeGutter(painter, gutterRect);
    paintColumnHeaders(painter, headerRect);
    paintEvents(painter, gridRect);
    paintCurrentTimeLine(painter, gridRect);
}

void TimeGridView::paintColumnHeaders(QPainter &painter, const QRect &headerRect)
{
    if (m_columnDates.isEmpty())
        return;

    const int columnWidth = headerRect.width() / m_columnDates.size();
    const QDate today = QDate::currentDate();

    QFont headerFont = font();
    headerFont.setBold(true);

    for (int i = 0; i < m_columnDates.size(); ++i) {
        const QDate date = m_columnDates[i];
        const QRect colRect(headerRect.left() + i * columnWidth, headerRect.top(), columnWidth, headerRect.height());

        if (date == today) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(m_theme.selection);
            painter.drawRoundedRect(colRect.adjusted(6, 3, -6, -3), 6, 6);
        }

        painter.setFont(headerFont);
        painter.setPen(date == today ? Qt::white : m_theme.text);
        painter.drawText(colRect, Qt::AlignCenter, date.toString("ddd d"));
    }
}

void TimeGridView::paintTimeGutter(QPainter &painter, const QRect &gutterRect)
{
    painter.setPen(m_theme.textSecondary);
    QFont smallFont = font();
    smallFont.setPointSize(qMax(7, smallFont.pointSize() - 1));
    painter.setFont(smallFont);
    QFontMetrics metrics(smallFont);

    for (int hour = 0; hour < 24; ++hour) {
        const int y = timeToY(QTime(hour, 0));
        if (y - gutterRect.top() < metrics.height() / 2)
            continue; // "00:00" у самого верха, подпись всё равно бы обрезалась
        const QRect labelRect(gutterRect.left(), y - 8, gutterRect.width() - 6, 16);
        painter.drawText(labelRect, Qt::AlignRight | Qt::AlignVCenter, QString("%1:00").arg(hour, 2, 10, QChar('0')));
    }
}

void TimeGridView::paintGridLines(QPainter &painter, const QRect &gridRect)
{
    for (int hour = 0; hour < 24; ++hour) {
        const int yHour = timeToY(QTime(hour, 0));
        painter.setPen(m_theme.gridLine);
        painter.drawLine(gridRect.left(), yHour, gridRect.right(), yHour);

        const int yHalf = timeToY(QTime(hour, 30));
        QPen halfPen(m_theme.gridLine);
        halfPen.setStyle(Qt::DotLine);
        painter.setPen(halfPen);
        painter.drawLine(gridRect.left(), yHalf, gridRect.right(), yHalf);
    }

    if (!m_columnDates.isEmpty()) {
        const int columnWidth = gridRect.width() / m_columnDates.size();
        painter.setPen(m_theme.gridLine);
        for (int i = 0; i <= m_columnDates.size(); ++i) {
            const int x = gridRect.left() + i * columnWidth;
            painter.drawLine(x, gridRect.top(), x, gridRect.bottom());
        }
    }
}

void TimeGridView::paintCurrentTimeLine(QPainter &painter, const QRect &gridRect)
{
    const int columnIndex = m_columnDates.indexOf(QDate::currentDate());
    if (columnIndex < 0)
        return; // сегодняшнего дня нет среди видимых колонок

    const QRect col = columnRect(columnIndex, gridRect);
    const int y = timeToY(QTime::currentTime());

    QPen pen(m_theme.accent);
    pen.setWidth(2);
    painter.setPen(pen);
    painter.drawLine(col.left(), y, col.right(), y);

    painter.setPen(Qt::NoPen);
    painter.setBrush(m_theme.accent);
    painter.drawEllipse(QPoint(col.left(), y), 4, 4);
}

void TimeGridView::paintEvents(QPainter &painter, const QRect &gridRect)
{
    m_eventHitRects.clear();

    QFont eventFont = font();
    eventFont.setPointSize(qMax(7, eventFont.pointSize() - 1));
    painter.setFont(eventFont);
    QFontMetrics metrics(eventFont);

    for (int col = 0; col < m_columnDates.size() && col < m_eventsByColumn.size(); ++col) {
        const QRect colRect = columnRect(col, gridRect);
        const QVector<EventLayoutSlot> eventSlots = layoutEventsForDay(m_eventsByColumn[col]);

        for (const EventLayoutSlot &slot : eventSlots) {
            const int y1 = timeToY(slot.event.startTime);
            // Минимальная высота 18px - иначе очень короткое событие
            // (например, 15 минут при маленьком зуме) стало бы нечитаемым.
            const int y2 = qMax(y1 + 18, timeToY(slot.event.endTime));
            const int laneWidth = colRect.width() / slot.laneCount;
            const int x = colRect.left() + slot.laneIndex * laneWidth;

            const QRect eventRect(x + 2, y1 + 1, qMax(10, laneWidth - 4), y2 - y1 - 2);

            painter.setPen(Qt::NoPen);
            painter.setBrush(m_theme.eventBackground);
            painter.drawRoundedRect(eventRect, 4, 4);

            painter.setBrush(m_theme.accent);
            painter.drawRect(QRect(eventRect.left(), eventRect.top(), 3, eventRect.height()));

            if (slot.event.id == m_selectedEventId) {
                QPen selectedPen(m_theme.accent);
                selectedPen.setWidth(2);
                painter.setPen(selectedPen);
                painter.setBrush(Qt::NoBrush);
                painter.drawRoundedRect(eventRect, 4, 4);
            }

            const QRect textRect = eventRect.adjusted(8, 2, -4, -2);
            painter.setPen(m_theme.eventText);

            const QString titleLine = slot.event.title;
            const QString timeLine = QString("%1\xE2\x80\x93%2")
                .arg(slot.event.startTime.toString("HH:mm"))
                .arg(slot.event.endTime.toString("HH:mm"));

            if (textRect.height() >= 30) {
                painter.drawText(QRect(textRect.left(), textRect.top(), textRect.width(), 16),
                                  Qt::AlignLeft | Qt::AlignVCenter,
                                  metrics.elidedText(titleLine, Qt::ElideRight, textRect.width()));
                painter.drawText(QRect(textRect.left(), textRect.top() + 16, textRect.width(), 16),
                                  Qt::AlignLeft | Qt::AlignVCenter,
                                  metrics.elidedText(timeLine, Qt::ElideRight, textRect.width()));
            } else {
                const QString combined = QString("%1  %2").arg(timeLine, titleLine);
                painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                                  metrics.elidedText(combined, Qt::ElideRight, textRect.width()));
            }

            m_eventHitRects.append({slot.event.id, eventRect});
        }
    }
}

void TimeGridView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    const int hitId = hitTestEvent(event->pos());
    if (hitId >= 0) {
        setSelectedEventId(hitId);
        emit eventSelected(hitId);
        return;
    }

    const QRect gridRect = computeGridRect();
    const int columnIndex = columnIndexAt(event->pos().x(), gridRect);
    if (columnIndex < 0)
        return; // клик мимо сетки (например, по шапке или по gutter'у со временем)

    setSelectedEventId(-1); // клик по пустому месту снимает выделение с события

    const QTime snapped = snapToGrid(yToTime(event->pos().y()));
    emit createEventRequested(m_columnDates[columnIndex], snapped);
}

void TimeGridView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    const int hitId = hitTestEvent(event->pos());
    if (hitId >= 0)
        emit editEventRequested(hitId);

    // Двойной клик по пустому месту ничего не делает - одиночный клик
    // там уже открыл создание события, второго диалога не нужно.
}

void TimeGridView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const int steps = event->angleDelta().y() / 120; // ±1 за типичный "щелчок" колеса
        const int newHeight = qBound(kMinHourHeight, m_hourHeight + steps * kZoomStepPerNotch, kMaxHourHeight);

        if (newHeight != m_hourHeight) {
            m_hourHeight = newHeight;
            updateMinimumHeight();
            update();
        }

        event->accept();
        return;
    }

    // Обычное колесо не обрабатываем - пусть "всплывёт" до QScrollArea,
    // который и должен прокручивать сетку по вертикали.
    event->ignore();
}
