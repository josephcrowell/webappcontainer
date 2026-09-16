// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QUrl>

class QCoreApplication;

class LaunchConfiguration final : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("LaunchConfiguration is created by the application")
  Q_PROPERTY(QUrl startUrl READ startUrl CONSTANT)
  Q_PROPERTY(QString applicationName READ applicationName CONSTANT)
  Q_PROPERTY(QString profileName READ profileName CONSTANT)
  Q_PROPERTY(QString profilePath READ profilePath CONSTANT)
  Q_PROPERTY(QString iconPath READ iconPath CONSTANT)
  Q_PROPERTY(QString trayIconPath READ trayIconPath CONSTANT)
  Q_PROPERTY(QUrl trayIconUrl READ trayIconUrl CONSTANT)
  Q_PROPERTY(bool startMinimized READ startMinimized CONSTANT)
  Q_PROPERTY(bool showBackgroundHints READ showBackgroundHints CONSTANT)
  Q_PROPERTY(QString pushSubscriptionScript READ pushSubscriptionScript CONSTANT)

public:
  explicit LaunchConfiguration(QCoreApplication &application, QObject *parent = nullptr);

  QUrl startUrl() const { return m_startUrl; }
  QString applicationName() const { return m_applicationName; }
  QString profileName() const { return m_profileName; }
  QString profilePath() const { return m_profilePath; }
  QString iconPath() const { return m_iconPath; }
  QString trayIconPath() const { return m_trayIconPath; }
  QUrl trayIconUrl() const;
  bool startMinimized() const { return m_startMinimized; }
  bool showBackgroundHints() const { return m_showBackgroundHints; }
  QString pushSubscriptionScript() const { return m_pushSubscriptionScript; }

private:
  QUrl m_startUrl;
  QString m_applicationName;
  QString m_profileName;
  QString m_profilePath;
  QString m_iconPath;
  QString m_trayIconPath;
  bool m_startMinimized = false;
  bool m_showBackgroundHints = true;
  QString m_pushSubscriptionScript;
};
