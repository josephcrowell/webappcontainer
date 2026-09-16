// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "notificationmodel.h"

#include <QImage>
#include <QQuickWebEngineProfile>
#include <QWebEngineNotification>

NotificationModel::NotificationModel(QObject *parent) : QAbstractListModel(parent) {}

int NotificationModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : int(m_items.size());
}

QVariant NotificationModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
    return {};
  const Item &item = m_items.at(index.row());
  switch (role) {
  case IdRole: return QVariant::fromValue(item.id);
  case TitleRole: return item.title;
  case MessageRole: return item.message;
  case OriginRole: return item.origin;
  case IconUrlRole: return item.iconUrl;
  default: return {};
  }
}

QHash<int, QByteArray> NotificationModel::roleNames() const {
  return {{IdRole, "notificationId"}, {TitleRole, "title"},
          {MessageRole, "message"}, {OriginRole, "origin"},
          {IconUrlRole, "iconUrl"}};
}

void NotificationModel::attachProfile(QQuickWebEngineProfile *profile) {
  connect(profile, &QQuickWebEngineProfile::presentNotification, this,
          &NotificationModel::present);
}

qsizetype NotificationModel::indexOf(quint64 id) const {
  for (qsizetype index = 0; index < m_items.size(); ++index) {
    if (m_items.at(index).id == id)
      return index;
  }
  return -1;
}

void NotificationModel::present(QWebEngineNotification *notification) {
  if (!notification)
    return;

  const QString tag = notification->tag();
  if (!tag.isEmpty()) {
    for (qsizetype index = m_items.size() - 1; index >= 0; --index) {
      if (m_items.at(index).tag == tag &&
          m_items.at(index).origin == notification->origin()) {
        if (m_items.at(index).notification)
          m_items.at(index).notification->close();
        remove(m_items.at(index).id);
        break;
      }
    }
  }

  Item item;
  item.id = m_nextId++;
  item.title = notification->title();
  item.message = notification->message();
  item.origin = notification->origin();
  item.tag = tag;
  item.notification = notification;
  if (m_iconDirectory.isValid() && !notification->icon().isNull()) {
    const QString iconPath =
        m_iconDirectory.filePath(QStringLiteral("notification-%1.png").arg(item.id));
    if (notification->icon().save(iconPath, "PNG"))
      item.iconUrl = QUrl::fromLocalFile(iconPath);
  }

  const int row = int(m_items.size());
  beginInsertRows({}, row, row);
  m_items.append(item);
  endInsertRows();
  emit countChanged();
  const quint64 id = item.id;
  connect(notification, &QWebEngineNotification::closed, this,
          [this, id] { remove(id); });
  connect(notification, &QObject::destroyed, this, [this, id] { remove(id); });
  notification->show();
  emit notificationPresented(id, item.title, item.message, item.iconUrl);
}

void NotificationModel::remove(quint64 id) {
  const qsizetype row = indexOf(id);
  if (row < 0)
    return;
  beginRemoveRows({}, int(row), int(row));
  m_items.removeAt(row);
  endRemoveRows();
  emit notificationRemoved(id);
  emit countChanged();
}

void NotificationModel::click(quint64 id) {
  const qsizetype row = indexOf(id);
  if (row < 0 || !m_items.at(row).notification)
    return;
  emit notificationActivated();
  m_items.at(row).notification->click();
  m_items.at(row).notification->close();
  remove(id);
}

void NotificationModel::close(quint64 id) {
  const qsizetype row = indexOf(id);
  if (row < 0)
    return;
  if (m_items.at(row).notification)
    m_items.at(row).notification->close();
  remove(id);
}
