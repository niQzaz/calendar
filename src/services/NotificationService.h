#pragma once

#include <QObject>
#include <QSystemTrayIcon>

// Обёртка над QSystemTrayIcon для показа desktop-уведомлений.
//
// Почему QSystemTrayIcon, а не прямой вызов D-Bus (org.freedesktop.Notifications)
// или сторонняя библиотека: это стандартный кросс-платформенный класс Qt,
// не требует ничего сверх Qt Widgets. На Linux showMessage() сам показывает
// системное уведомление через тот же механизм (libnotify/freedesktop),
// что и "нативные" способы - просто без ручной работы с D-Bus.
class NotificationService : public QObject
{
    Q_OBJECT

public:
    explicit NotificationService(QObject *parent = nullptr);

    // Показывает системное уведомление. Если системный трей недоступен
    // (бывает в некоторых минимальных окружениях/оконных менеджерах) -
    // тихо пишет предупреждение в консоль и ничего не показывает,
    // приложение при этом не падает и продолжает работать как обычно.
    void showNotification(const QString &title, const QString &message);

private:
    QSystemTrayIcon *m_trayIcon = nullptr;
};
