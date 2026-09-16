// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "appcontroller.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QTimer>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QWindow>

AppController::AppController(QObject *parent) : QObject(parent) {
  QDBusConnectionInterface *interface = QDBusConnection::sessionBus().interface();
  if (!interface)
    return;
  m_nativeTrayAvailable =
      interface->isServiceRegistered(QStringLiteral("org.kde.StatusNotifierWatcher")) ||
      interface->isServiceRegistered(
          QStringLiteral("org.freedesktop.StatusNotifierWatcher"));
}

void AppController::setNotificationBadge(bool value) {
  if (m_notificationBadge == value)
    return;
  m_notificationBadge = value;
  emit notificationBadgeChanged();
  emit trayIconUrlChanged();
}

QUrl AppController::trayIconUrl() const {
  const QString path = m_notificationBadge ? m_badgedTrayIconPath
                                           : m_normalTrayIconPath;
  return path.isEmpty() ? QUrl() : QUrl::fromLocalFile(path);
}

void AppController::configureTrayIcon(const QIcon &icon) {
  if (icon.isNull() || !m_trayIconDirectory.isValid())
    return;
  const QPixmap pixmap = icon.pixmap(QSize(64, 64));
  if (pixmap.isNull())
    return;
  const QImage normal = pixmap.toImage().convertToFormat(
      QImage::Format_ARGB32_Premultiplied);
  QImage badged = normal;
  QPainter painter(&badged);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);
  const int dotSize = 64 / 3;
  const int x = 64 - dotSize - 1;
  const int y = 1;
  painter.setBrush(Qt::white);
  painter.drawEllipse(x - 1, y - 1, dotSize + 2, dotSize + 2);
  painter.setBrush(QColor(255, 59, 48));
  painter.drawEllipse(x, y, dotSize, dotSize);
  painter.end();

  const QString normalPath =
      m_trayIconDirectory.filePath(QStringLiteral("tray-normal.png"));
  const QString badgedPath =
      m_trayIconDirectory.filePath(QStringLiteral("tray-notification.png"));
  if (!normal.save(normalPath, "PNG") || !badged.save(badgedPath, "PNG"))
    return;
  m_normalTrayIconPath = normalPath;
  m_badgedTrayIconPath = badgedPath;
  emit trayIconUrlChanged();
}

void AppController::restoreWindow(QWindow *window, bool maximized) {
  if (!window)
    return;
  if (maximized)
    window->showMaximized();
  else
    window->showNormal();
  window->raise();
  window->requestActivate();
  setNotificationBadge(false);
  if (auto *quickWindow = qobject_cast<QQuickWindow *>(window)) {
    QPointer<QQuickWindow> guard(quickWindow);
    QTimer::singleShot(0, quickWindow, [guard] {
      if (!guard)
        return;
      if (guard->contentItem())
        guard->contentItem()->update();
      guard->update();
    });
  }
}

void AppController::requestQuit() {
  if (m_quitting)
    return;
  m_quitting = true;
  emit quittingChanged();
  QTimer::singleShot(0, QCoreApplication::instance(), &QCoreApplication::quit);
}
