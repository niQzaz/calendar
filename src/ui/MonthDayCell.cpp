#include "MonthDayCell.h"

#include <QPainter>
#include <QMouseEvent>
#include <QFontMetrics>

namespace {
constexpr int kCellPadding = 6;
constexpr int kChipHeight = 16;
constexpr int kChipSpacing = 2;
constexpr int kChipRadius = 3;

// Смешивает два цвета: t=0 -> чистый a, t=1 -> чистый b. Используется для
// "притушенного" цвета дней соседних месяцев - у Theme нет отдельного поля
// специально под это, проще получить его смешиванием textSecondary с фоном,
// чем заводить ещё одну роль цвета ради одного частного случая.
QColor blend(const QColor &a, const QColor &b, qreal t)
{
    return QColor::fromRgbF(
        a.redF() * (1 - t) + b.redF() * t,
        a.greenF() * (1 - t) + b.greenF() * t,
        a.blueF() * (1 - t) + b.blueF() * t
    );
}
}

MonthDayCell::MonthDayCell(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QSize MonthDayCell::sizeHint() const
{
    return QSize(110, 84);
}

void MonthDayCell::setDate(const QDate &date)
{
    m_date = date;
}

void MonthDayCell::setEvents(const QVector<Event> &events)
{
    m_events = events;
    update();
}

void MonthDayCell::setToday(bool isToday)
{
    if (m_isToday == isToday)
        return;
    m_isToday = isToday;
    update();
}

void MonthDayCell::setSelected(bool selected)
{
    if (m_isSelected == selected)
        return;
    m_isSelected = selected;
    update();
}

void MonthDayCell::setOtherMonth(bool otherMonth)
{
    if (m_isOtherMonth == otherMonth)
        return;
    m_isOtherMonth = otherMonth;
    update();
}

void MonthDayCell::setTheme(const Theme &theme)
{
    m_theme = theme;
    update();
}

void MonthDayCell::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRect bounds = rect().adjusted(2, 2, -2, -2);

    // --- Фон выбранного/сегодняшнего дня ---
    if (m_isSelected) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_theme.selection);
        painter.drawRoundedRect(bounds, 8, 8);
    } else if (m_isToday) {
        painter.setPen(QPen(m_theme.today, 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(bounds.adjusted(1, 1, -1, -1), 8, 8);
    }

    // --- Номер дня ---
    QFont dayFont = font();
    dayFont.setPointSize(dayFont.pointSize() + 1);
    dayFont.setBold(m_isToday || m_isSelected);
    painter.setFont(dayFont);

    QColor dayColor = m_theme.text;
    if (m_isSelected)
        dayColor = Qt::white; // все цвета selection достаточно тёмные для белого текста
    else if (m_isOtherMonth)
        dayColor = blend(m_theme.textSecondary, m_theme.background, 0.5);

    painter.setPen(dayColor);
    const QRect dayNumberRect(bounds.left() + kCellPadding, bounds.top() + 2,
                               bounds.width() - 2 * kCellPadding, 20);
    painter.drawText(dayNumberRect, Qt::AlignLeft | Qt::AlignVCenter, QString::number(m_date.day()));

    // --- Мини-карточки событий ---
    m_eventRects.clear();
    m_eventRectIds.clear();

    if (m_isOtherMonth) {
        // Соседние месяцы показываем совсем "тихими" - без событий,
        // чтобы не отвлекать внимание от текущего месяца.
        return;
    }

    const int shownCount = m_events.size() <= kMaxVisibleEvents
        ? m_events.size()
        : kMaxVisibleEvents - 1;

    int y = dayNumberRect.bottom() + 2;
    QFont chipFont = font();
    chipFont.setPointSize(chipFont.pointSize() - 1);
    painter.setFont(chipFont);
    QFontMetrics chipMetrics(chipFont);

    for (int i = 0; i < shownCount; ++i) {
        const Event &ev = m_events[i];
        const QRect chipRect(bounds.left() + kCellPadding, y,
                              bounds.width() - 2 * kCellPadding, kChipHeight);
        if (chipRect.bottom() > bounds.bottom())
            break;

        painter.setPen(Qt::NoPen);
        painter.setBrush(m_theme.eventBackground);
        painter.drawRoundedRect(chipRect, kChipRadius, kChipRadius);

        // Цветная полоса слева - визуальный акцент, как в задумке дизайна.
        const QRect accentRect(chipRect.left(), chipRect.top(), 3, chipRect.height());
        painter.setBrush(m_theme.accent);
        painter.drawRect(accentRect);

        const QString label = QString("%1 %2").arg(ev.startTime.toString("HH:mm")).arg(ev.title);
        const QRect textRect = chipRect.adjusted(6, 0, -4, 0);
        painter.setPen(m_theme.eventText);
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                          chipMetrics.elidedText(label, Qt::ElideRight, textRect.width()));

        m_eventRects.append(chipRect);
        m_eventRectIds.append(ev.id);

        y += kChipHeight + kChipSpacing;
    }

    if (m_events.size() > shownCount) {
        const int more = m_events.size() - shownCount;
        const QRect moreRect(bounds.left() + kCellPadding, y,
                              bounds.width() - 2 * kCellPadding, kChipHeight);
        painter.setPen(m_theme.textSecondary);
        painter.drawText(moreRect, Qt::AlignLeft | Qt::AlignVCenter, QString("+%1 more").arg(more));
    }
}

int MonthDayCell::eventIdAt(const QPoint &pos) const
{
    for (int i = 0; i < m_eventRects.size(); ++i) {
        if (m_eventRects[i].contains(pos))
            return m_eventRectIds[i];
    }
    return -1;
}

void MonthDayCell::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked(m_date);
}

void MonthDayCell::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    const int eventId = eventIdAt(event->pos());
    if (eventId >= 0)
        emit editEventRequested(eventId);
    else
        emit createEventRequested(m_date);
}
