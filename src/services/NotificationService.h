#pragma once

#include <QObject>
#include <QSystemTrayIcon>

class AppSettings;

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
    explicit NotificationService(AppSettings *settings, QObject *parent = nullptr);

    // Показывает системное уведомление. Ничего не делает, если пользователь
    // выключил уведомления в настройках (Этап 8), а также если системный
    // трей недоступен (бывает в некоторых минимальных окружениях/оконных
    // менеджерах) - в этом случае просто пишет предупреждение в консоль,
    // приложение при этом не падает.
    void showNotification(const QString &title, const QString &message);

private:
    AppSettings *m_settings;
    QSystemTrayIcon *m_trayIcon = nullptr;
};
