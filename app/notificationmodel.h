// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QPointer>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QUrl>

class QQuickWebEngineProfile;
class QWebEngineNotification;

class NotificationModel : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("NotificationModel is created by the application")
  Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
  enum Role { IdRole = Qt::UserRole + 1, TitleRole, MessageRole, OriginRole, IconUrlRole };
  Q_ENUM(Role)

  explicit NotificationModel(QObject *parent = nullptr);
  int rowCount(const QModelIndex &parent = {}) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;
  int count() const { return int(m_items.size()); }
  void attachProfile(QQuickWebEngineProfile *profile);
  Q_INVOKABLE void click(quint64 id);
  Q_INVOKABLE void close(quint64 id);

signals:
  void countChanged();
  void notificationActivated();
  void notificationPresented(quint64 id, const QString &title,
                             const QString &message, const QUrl &iconUrl);
  void notificationRemoved(quint64 id);

private:
  struct Item {
    quint64 id = 0;
    QString title;
    QString message;
    QUrl origin;
    QString tag;
    QUrl iconUrl;
    QPointer<QWebEngineNotification> notification;
  };
  void present(QWebEngineNotification *notification);
  void remove(quint64 id);
  qsizetype indexOf(quint64 id) const;
  QList<Item> m_items;
  QTemporaryDir m_iconDirectory;
  quint64 m_nextId = 1;
};
