#include "MonthDayCell.h"

#include <QPainter>
#include <QMouseEvent>
#include <QFontMetrics>

namespace {
// Временная палитра (см. комментарий в заголовке) - до появления
// ThemeManager в Phase C цвета здесь захардкожены под тёмную тему.
const QColor kTodayBorder(94, 129, 244);
const QColor kSelectedFill(94, 129, 244);
const QColor kSelectedText(255, 255, 255);
const QColor kNormalText(230, 230, 232);
const QColor kOtherMonthText(102, 102, 102);
const QColor kChipBackground(58, 58, 68);
const QColor kChipAccent(94, 129, 244);
const QColor kChipText(220, 220, 224);
const QColor kMoreText(150, 150, 155);

constexpr int kCellPadding = 6;
constexpr int kChipHeight = 16;
constexpr int kChipSpacing = 2;
constexpr int kChipRadius = 3;
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

void MonthDayCell::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRect bounds = rect().adjusted(2, 2, -2, -2);

    // --- Фон выбранного/сегодняшнего дня ---
    if (m_isSelected) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(kSelectedFill);
        painter.drawRoundedRect(bounds, 8, 8);
    } else if (m_isToday) {
        painter.setPen(QPen(kTodayBorder, 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(bounds.adjusted(1, 1, -1, -1), 8, 8);
    }

    // --- Номер дня ---
    QFont dayFont = font();
    dayFont.setPointSize(dayFont.pointSize() + 1);
    dayFont.setBold(m_isToday || m_isSelected);
    painter.setFont(dayFont);

    QColor dayColor = kNormalText;
    if (m_isSelected)
        dayColor = kSelectedText;
    else if (m_isOtherMonth)
        dayColor = kOtherMonthText;

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
        painter.setBrush(kChipBackground);
        painter.drawRoundedRect(chipRect, kChipRadius, kChipRadius);

        // Цветная полоса слева - визуальный акцент, как в задумке дизайна.
        const QRect accentRect(chipRect.left(), chipRect.top(), 3, chipRect.height());
        painter.setBrush(kChipAccent);
        painter.drawRect(accentRect);

        const QString label = QString("%1 %2").arg(ev.startTime.toString("HH:mm")).arg(ev.title);
        const QRect textRect = chipRect.adjusted(6, 0, -4, 0);
        painter.setPen(kChipText);
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
        painter.setPen(kMoreText);
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
