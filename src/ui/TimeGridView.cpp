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

const TimeGridView::HitEvent *TimeGridView::hitTestFull(const QPoint &pos) const
{
    for (const HitEvent &hit : m_eventHitRects) {
        if (hit.rect.contains(pos))
            return &hit;
    }
    return nullptr;
}

int TimeGridView::hitTestEvent(const QPoint &pos) const
{
    const HitEvent *hit = hitTestFull(pos);
    return hit ? hit->event.id : -1;
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
    paintDragPreview(painter, gridRect);
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

            // Цветная полоса слева - у события с категорией берём её цвет,
            // иначе (как и раньше) общий акцентный цвет темы.
            QColor accentColor = m_theme.accent;
            if (!slot.event.categoryColor.isEmpty()) {
                const QColor categoryColor(slot.event.categoryColor);
                if (categoryColor.isValid())
                    accentColor = categoryColor;
            }
            painter.setBrush(accentColor);
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

            m_eventHitRects.append({slot.event, eventRect});
        }
    }
}

void TimeGridView::computeDragTarget(const QPoint &mousePos, QDate &outDate, QTime &outStart, QTime &outEnd) const
{
    const QRect gridRect = computeGridRect();
    const int columnIndex = columnIndexAt(mousePos.x(), gridRect);
    outDate = (columnIndex >= 0) ? m_columnDates[columnIndex] : m_draggedEvent.date;

    // Длительность сохраняем такой же, как была - Moving двигает событие
    // целиком, не меняя его длину (за длину отвечает Resizing, computeResizeTarget()).
    int durationSecs = m_draggedEvent.startTime.secsTo(m_draggedEvent.endTime);
    if (durationSecs <= 0)
        durationSecs += 24 * 3600; // на случай события, идущего через полночь
    durationSecs = qBound(60, durationSecs, 24 * 3600);

    // yToTime() уже клэмпит y в границы суток, поэтому rawStart всегда
    // валиден - осталось только не дать событию "вылезти" за полночь
    // при пересчёте конца.
    const QTime rawStart = snapToGrid(yToTime(mousePos.y() - m_dragGrabOffset.y()));
    const int rawStartSecs = rawStart.hour() * 3600 + rawStart.minute() * 60;
    const int maxStartSecs = qMax(0, 24 * 3600 - durationSecs);
    const int clampedStartSecs = qBound(0, rawStartSecs, maxStartSecs);

    outStart = QTime(0, 0).addSecs(clampedStartSecs);
    outEnd = outStart.addSecs(durationSecs);
}

void TimeGridView::computeResizeTarget(const QPoint &mousePos, QTime &outEnd) const
{
    // В отличие от computeDragTarget() - дата и начало НЕ меняются вообще,
    // только конец. Тянем ручку у нижнего края блока - это меняет длину
    // события, а не его положение.
    const int maxSecs = 24 * 3600 - 60; // 23:59 - как и yToTime(), за полночь не переваливаем
    const int startSecs = m_draggedEvent.startTime.hour() * 3600 + m_draggedEvent.startTime.minute() * 60;
    const int minEndSecs = qMin(startSecs + kSnapMinutes * 60, maxSecs); // минимум - один шаг сетки

    const QTime rawEnd = snapToGrid(yToTime(mousePos.y()));
    const int rawEndSecs = rawEnd.hour() * 3600 + rawEnd.minute() * 60;
    const int clampedEndSecs = qBound(minEndSecs, rawEndSecs, maxSecs);

    outEnd = QTime(0, 0).addSecs(clampedEndSecs);
}

void TimeGridView::paintDragPreview(QPainter &painter, const QRect &gridRect)
{
    if (m_interactionMode == InteractionMode::None || !m_dragThresholdExceeded)
        return;

    QDate targetDate = m_draggedEvent.date;
    QTime targetStart = m_draggedEvent.startTime;
    QTime targetEnd = m_draggedEvent.endTime;

    if (m_interactionMode == InteractionMode::Moving)
        computeDragTarget(m_dragCurrentPos, targetDate, targetStart, targetEnd);
    else // Resizing - дата и начало не трогаем, меняется только конец
        computeResizeTarget(m_dragCurrentPos, targetEnd);

    const int columnIndex = m_columnDates.indexOf(targetDate);
    if (columnIndex < 0)
        return; // передвинули туда, где этой даты сейчас не видно на экране

    const QRect colRect = columnRect(columnIndex, gridRect);
    const int y1 = timeToY(targetStart);
    const int y2 = qMax(y1 + 18, timeToY(targetEnd));
    const QRect previewRect(colRect.left() + 2, y1 + 1, qMax(10, colRect.width() - 4), y2 - y1 - 2);

    QColor fill = m_theme.accent;
    fill.setAlpha(70);
    painter.setBrush(fill);
    QPen dashedPen(m_theme.accent);
    dashedPen.setWidth(2);
    dashedPen.setStyle(Qt::DashLine);
    painter.setPen(dashedPen);
    painter.drawRoundedRect(previewRect, 4, 4);

    painter.setPen(m_theme.eventText);
    const QString label = QString("%1\xE2\x80\x93%2  %3")
        .arg(targetStart.toString("HH:mm"), targetEnd.toString("HH:mm"), m_draggedEvent.title);
    painter.drawText(previewRect.adjusted(8, 2, -4, -2), Qt::AlignLeft | Qt::AlignVCenter, label);
}

void TimeGridView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    const HitEvent *hit = hitTestFull(event->pos());
    if (hit) {
        setSelectedEventId(hit->event.id);
        emit eventSelected(hit->event.id);

        // Перетаскивать/растягивать можно только НЕ повторяющиеся события.
        // Повторяющееся вхождение - это не отдельная запись в БД, а
        // вычисленная "проекция" общего шаблона (см. Event.h) - сдвинуть
        // или растянуть его отдельно от остальной серии сейчас нечем (для
        // этого нужна отдельная модель Occurrence с исключениями, которой
        // в проекте пока нет - см. calendar_project_context_v1.md, раздел 3,
        // "целевая модель данных"). Менять вместо этого весь шаблон было бы
        // неожиданным - тронул одно вхождение, а уехала/растянулась вся
        // серия. Поэтому просто не начинаем drag/resize - клик всё равно
        // выделяет событие, как и раньше.
        if (hit->event.recurrenceType == RecurrenceType::None) {
            // Нижние kResizeEdgePx блока - "ручка" resize; остальное - Moving.
            const bool onBottomEdge = qAbs(event->pos().y() - hit->rect.bottom()) <= kResizeEdgePx;
            m_interactionMode = onBottomEdge ? InteractionMode::Resizing : InteractionMode::Moving;
            m_dragThresholdExceeded = false;
            m_draggedEvent = hit->event;
            m_dragPressPos = event->pos();
            m_dragGrabOffset = event->pos() - hit->rect.topLeft(); // нужен только для Moving
            m_dragCurrentPos = event->pos();
        }
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

void TimeGridView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_interactionMode == InteractionMode::None) {
        event->ignore(); // не наш жест - пусть Qt/QScrollArea делают что хотят
        return;
    }

    m_dragCurrentPos = event->pos();

    if (!m_dragThresholdExceeded) {
        // Порог нужен, чтобы обычный клик по событию (мышь чуть дрогнула
        // между press и release) не считался перетаскиванием/resize на пару
        // пикселей - иначе даже клик "тихо" сдвигал бы или сжимал событие
        // на 15 минут из-за snap-to-grid.
        if ((m_dragCurrentPos - m_dragPressPos).manhattanLength() < kDragThresholdPx)
            return;
        m_dragThresholdExceeded = true;
        setCursor(m_interactionMode == InteractionMode::Resizing ? Qt::SizeVerCursor : Qt::ClosedHandCursor);
    }

    update(); // перерисовать превью на новой позиции
}

void TimeGridView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || m_interactionMode == InteractionMode::None)
        return;

    if (m_dragThresholdExceeded) {
        QDate targetDate = m_draggedEvent.date;
        QTime targetStart = m_draggedEvent.startTime;
        QTime targetEnd = m_draggedEvent.endTime;
        bool changed = false;

        if (m_interactionMode == InteractionMode::Moving) {
            computeDragTarget(event->pos(), targetDate, targetStart, targetEnd);
            changed = (targetDate != m_draggedEvent.date || targetStart != m_draggedEvent.startTime);
        } else { // Resizing
            computeResizeTarget(event->pos(), targetEnd);
            changed = (targetEnd != m_draggedEvent.endTime);
        }

        // Не эмитим сигнал, если по факту ничего не изменилось (взяли
        // и отпустили на том же месте, после снэпа) - иначе MainWindow
        // сделал бы лишний updateEvent() без реальных изменений.
        if (changed)
            emit eventRescheduled(m_draggedEvent.id, targetDate, targetStart, targetEnd);
    }

    m_interactionMode = InteractionMode::None;
    m_dragThresholdExceeded = false;
    unsetCursor();
    update();
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
