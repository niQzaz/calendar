#include "NotificationService.h"
#include "AppSettings.h"

#include <QApplication>
#include <QStyle>
#include <QDebug>

NotificationService::NotificationService(AppSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qWarning() << "System tray is not available - desktop notifications will be disabled.";
        return;
    }

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(qApp->style()->standardIcon(QStyle::SP_ComputerIcon));
    m_trayIcon->setToolTip("Calendar");
    m_trayIcon->show();
}

void NotificationService::showNotification(const QString &title, const QString &message)
{
    if (!m_settings->notificationsEnabled())
        return;

    if (!m_trayIcon) {
        qWarning() << "Notification skipped (no tray icon):" << title << "-" << message;
        return;
    }

    m_trayIcon->showMessage(title, message, QSystemTrayIcon::Information, 8000);
}
