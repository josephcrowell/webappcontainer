// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "notificationpresenter.h"

#include "notificationmodel.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QVariantMap>

NotificationPresenter::NotificationPresenter(NotificationModel *model,
                                             const QString &applicationName,
                                             const QUrl &fallbackIcon,
                                             QObject *parent)
    : QObject(parent), m_model(model), m_applicationName(applicationName),
      m_fallbackIcon(fallbackIcon) {
  connect(model, &NotificationModel::notificationPresented, this,
          &NotificationPresenter::present);
  connect(model, &NotificationModel::notificationRemoved, this,
          &NotificationPresenter::close);
  QDBusConnection::sessionBus().connect(
      QStringLiteral("org.freedesktop.Notifications"),
      QStringLiteral("/org/freedesktop/Notifications"),
      QStringLiteral("org.freedesktop.Notifications"),
      QStringLiteral("ActionInvoked"), this,
      SLOT(actionInvoked(uint,QString)));
  QDBusConnection::sessionBus().connect(
      QStringLiteral("org.freedesktop.Notifications"),
      QStringLiteral("/org/freedesktop/Notifications"),
      QStringLiteral("org.freedesktop.Notifications"),
      QStringLiteral("NotificationClosed"), this,
      SLOT(notificationClosed(uint,uint)));
}

void NotificationPresenter::present(quint64 id, const QString &title,
                                    const QString &message,
                                    const QUrl &iconUrl) {
  QDBusInterface notifications(
      QStringLiteral("org.freedesktop.Notifications"),
      QStringLiteral("/org/freedesktop/Notifications"),
      QStringLiteral("org.freedesktop.Notifications"),
      QDBusConnection::sessionBus());
  if (!notifications.isValid()) {
    emit fallbackRequested(id, title, message);
    return;
  }
  const QUrl selectedIcon = iconUrl.isLocalFile() ? iconUrl : m_fallbackIcon;
  QVariantMap hints;
  if (selectedIcon.isLocalFile())
    hints.insert(QStringLiteral("image-path"), selectedIcon.toLocalFile());
  const QDBusReply<uint> reply = notifications.call(
      QStringLiteral("Notify"), m_applicationName, uint(0),
      selectedIcon.isLocalFile() ? selectedIcon.toLocalFile() : QString(),
      title, message,
      QStringList{QStringLiteral("default"), tr("Open")}, hints, 10000);
  if (!reply.isValid()) {
    emit fallbackRequested(id, title, message);
    return;
  }
  m_nativeIds.insert(id, reply.value());
  m_modelIds.insert(reply.value(), id);
}

void NotificationPresenter::close(quint64 id) {
  const uint nativeId = m_nativeIds.take(id);
  if (!nativeId)
    return;
  m_modelIds.remove(nativeId);
  QDBusInterface notifications(
      QStringLiteral("org.freedesktop.Notifications"),
      QStringLiteral("/org/freedesktop/Notifications"),
      QStringLiteral("org.freedesktop.Notifications"),
      QDBusConnection::sessionBus());
  if (notifications.isValid())
    notifications.call(QStringLiteral("CloseNotification"), nativeId);
}

void NotificationPresenter::actionInvoked(uint notificationId,
                                          const QString &) {
  const quint64 modelId = m_modelIds.take(notificationId);
  if (!modelId)
    return;
  m_nativeIds.remove(modelId);
  m_model->click(modelId);
}

void NotificationPresenter::notificationClosed(uint notificationId, uint) {
  const quint64 modelId = m_modelIds.take(notificationId);
  if (modelId) {
    m_nativeIds.remove(modelId);
    m_model->close(modelId);
  }
}
