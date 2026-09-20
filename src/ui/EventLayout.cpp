#include "EventLayout.h"

#include <algorithm>

QVector<EventLayoutSlot> layoutEventsForDay(const QVector<Event> &events)
{
    QVector<EventLayoutSlot> result;
    if (events.isEmpty())
        return result;

    QVector<Event> sorted = events;
    std::sort(sorted.begin(), sorted.end(), [](const Event &a, const Event &b) {
        return a.startTime < b.startTime;
    });

    // Раскладывает один "кластер" (диапазон индексов в sorted, чьи события
    // непрерывно пересекаются друг с другом) по дорожкам и добавляет
    // готовые слоты в result.
    auto flushCluster = [&](int clusterStart, int clusterEnd) {
        QVector<QTime> laneEnds; // время окончания последнего события каждой дорожки
        QVector<int> laneOf(clusterEnd - clusterStart);

        for (int i = clusterStart; i < clusterEnd; ++i) {
            const Event &ev = sorted[i];

            int lane = -1;
            for (int l = 0; l < laneEnds.size(); ++l) {
                if (laneEnds[l] <= ev.startTime) {
                    lane = l;
                    break;
                }
            }
            if (lane == -1) {
                lane = laneEnds.size();
                laneEnds.append(ev.endTime);
            } else {
                laneEnds[lane] = ev.endTime;
            }
            laneOf[i - clusterStart] = lane;
        }

        const int laneCount = laneEnds.size();
        for (int i = clusterStart; i < clusterEnd; ++i) {
            EventLayoutSlot slot;
            slot.event = sorted[i];
            slot.laneIndex = laneOf[i - clusterStart];
            slot.laneCount = laneCount;
            result.append(slot);
        }
    };

    int clusterStart = 0;
    QTime clusterEnd = sorted[0].endTime;

    for (int i = 1; i < sorted.size(); ++i) {
        if (sorted[i].startTime < clusterEnd) {
            // Пересекается с текущим кластером - расширяем его конец при необходимости.
            if (sorted[i].endTime > clusterEnd)
                clusterEnd = sorted[i].endTime;
        } else {
            // Начинается после конца кластера - кластер закрыт, начинаем новый.
            flushCluster(clusterStart, i);
            clusterStart = i;
            clusterEnd = sorted[i].endTime;
        }
    }
    flushCluster(clusterStart, sorted.size());

    return result;
}
