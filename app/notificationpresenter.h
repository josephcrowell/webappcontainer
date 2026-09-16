// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QHash>
#include <QUrl>
#include <QQmlEngine>

class NotificationModel;

class NotificationPresenter final : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("NotificationPresenter is created by the application")

public:
  NotificationPresenter(NotificationModel *model, const QString &applicationName,
                        const QUrl &fallbackIcon, QObject *parent = nullptr);

signals:
  void fallbackRequested(quint64 id, const QString &title,
                         const QString &message);

private slots:
  void present(quint64 id, const QString &title, const QString &message,
               const QUrl &iconUrl);
  void close(quint64 id);
  void actionInvoked(uint notificationId, const QString &action);
  void notificationClosed(uint notificationId, uint reason);

private:
  NotificationModel *m_model = nullptr;
  QString m_applicationName;
  QUrl m_fallbackIcon;
  QHash<quint64, uint> m_nativeIds;
  QHash<uint, quint64> m_modelIds;
};
