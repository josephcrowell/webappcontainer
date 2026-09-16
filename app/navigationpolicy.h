// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QUrl>
#include <QRect>
#include <QWindow>

class NavigationPolicy : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("NavigationPolicy is created by the application")

public:
  using QObject::QObject;
  Q_INVOKABLE bool shouldOpenInApplication(const QUrl &currentUrl,
                                           const QUrl &requestedUrl) const;
  Q_INVOKABLE bool openExternal(const QUrl &url) const;
  Q_INVOKABLE QRect availableGeometry(QWindow *window) const;

private:
  static bool isFacebookHost(const QString &host);
};
