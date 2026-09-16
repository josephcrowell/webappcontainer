// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QWindow>
#include <QIcon>
#include <QTemporaryDir>
#include <QUrl>

class AppController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("AppController is created by the application")
  Q_PROPERTY(bool quitting READ quitting NOTIFY quittingChanged)
  Q_PROPERTY(bool notificationBadge READ notificationBadge WRITE setNotificationBadge
                 NOTIFY notificationBadgeChanged)
  Q_PROPERTY(bool nativeTrayAvailable READ nativeTrayAvailable CONSTANT)
  Q_PROPERTY(QUrl trayIconUrl READ trayIconUrl NOTIFY trayIconUrlChanged)

public:
  explicit AppController(QObject *parent = nullptr);
  bool quitting() const { return m_quitting; }
  bool notificationBadge() const { return m_notificationBadge; }
  bool nativeTrayAvailable() const { return m_nativeTrayAvailable; }
  QUrl trayIconUrl() const;
  void configureTrayIcon(const QIcon &icon);
  void setNotificationBadge(bool value);
  Q_INVOKABLE void restoreWindow(QWindow *window, bool maximized);
  Q_INVOKABLE void requestQuit();

signals:
  void quittingChanged();
  void notificationBadgeChanged();
  void trayIconUrlChanged();

private:
  bool m_quitting = false;
  bool m_notificationBadge = false;
  bool m_nativeTrayAvailable = false;
  QTemporaryDir m_trayIconDirectory;
  QString m_normalTrayIconPath;
  QString m_badgedTrayIconPath;
};
