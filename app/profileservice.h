// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QQuickWebEngineProfile>
#include <QQmlEngine>

class LaunchConfiguration;
class ProfileService final : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("ProfileService is created by the application")
  Q_PROPERTY(QQuickWebEngineProfile *profile READ profile CONSTANT)

public:
  explicit ProfileService(const LaunchConfiguration &configuration,
                          QObject *parent = nullptr);
  QQuickWebEngineProfile *profile() const { return m_profile; }

private:
  QQuickWebEngineProfile *m_profile = nullptr;
};
