// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QUrl>

class DownloadModel : public QAbstractListModel {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("DownloadModel is created by the application")
  Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
  enum Role {
    IdRole = Qt::UserRole + 1,
    FileNameRole,
    SourceRole,
    DestinationRole,
    ReceivedBytesRole,
    TotalBytesRole,
    ProgressRole,
    IndeterminateRole,
    BytesPerSecondRole,
    StateRole,
    FinishedRole,
    InterruptionReasonRole,
    CanCancelRole,
    CanRemoveRole,
  };
  Q_ENUM(Role)

  enum State { Requested, InProgress, Completed, Cancelled, Interrupted };
  Q_ENUM(State)

  struct Item {
    quint64 id = 0;
    QString fileName;
    QUrl source;
    QString destination;
    qint64 receivedBytes = -1;
    qint64 totalBytes = -1;
    qint64 bytesPerSecond = 0;
    State state = Requested;
    bool finished = false;
    QString interruptionReason;
  };

  explicit DownloadModel(QObject *parent = nullptr);
  int rowCount(const QModelIndex &parent = {}) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;
  int count() const { return int(m_items.size()); }

  quint64 append(Item item);
  bool update(quint64 id, const Item &item);
  Q_INVOKABLE void cancel(quint64 id);
  Q_INVOKABLE void remove(quint64 id);

signals:
  void cancelRequested(quint64 id);
  void countChanged();

private:
  qsizetype indexOf(quint64 id) const;
  QList<Item> m_items;
  quint64 m_nextId = 1;
};
