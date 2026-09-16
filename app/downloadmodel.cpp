// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "downloadmodel.h"

#include <QThread>

DownloadModel::DownloadModel(QObject *parent) : QAbstractListModel(parent) {}

int DownloadModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : int(m_items.size());
}

QVariant DownloadModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
    return {};
  const Item &item = m_items.at(index.row());
  switch (role) {
  case IdRole: return QVariant::fromValue(item.id);
  case FileNameRole: return item.fileName;
  case SourceRole: return item.source;
  case DestinationRole: return item.destination;
  case ReceivedBytesRole: return item.receivedBytes;
  case TotalBytesRole: return item.totalBytes;
  case ProgressRole:
    return item.totalBytes > 0 && item.receivedBytes >= 0
               ? qBound(0.0, double(item.receivedBytes) / double(item.totalBytes), 1.0)
               : 0.0;
  case IndeterminateRole: return item.totalBytes <= 0;
  case BytesPerSecondRole: return item.bytesPerSecond;
  case StateRole: return item.state;
  case FinishedRole: return item.finished;
  case InterruptionReasonRole: return item.interruptionReason;
  case CanCancelRole: return !item.finished && item.state != Cancelled;
  case CanRemoveRole: return item.finished || item.state == Cancelled || item.state == Interrupted;
  default: return {};
  }
}

QHash<int, QByteArray> DownloadModel::roleNames() const {
  return {
      {IdRole, "downloadId"}, {FileNameRole, "fileName"},
      {SourceRole, "source"}, {DestinationRole, "destination"},
      {ReceivedBytesRole, "receivedBytes"}, {TotalBytesRole, "totalBytes"},
      {ProgressRole, "progress"}, {IndeterminateRole, "indeterminate"},
      {BytesPerSecondRole, "bytesPerSecond"}, {StateRole, "downloadState"},
      {FinishedRole, "finished"}, {InterruptionReasonRole, "interruptionReason"},
      {CanCancelRole, "canCancel"}, {CanRemoveRole, "canRemove"},
  };
}

quint64 DownloadModel::append(Item item) {
  Q_ASSERT(thread() == QThread::currentThread());
  if (item.id == 0)
    item.id = m_nextId++;
  const int row = int(m_items.size());
  beginInsertRows({}, row, row);
  m_items.append(item);
  endInsertRows();
  emit countChanged();
  return item.id;
}

qsizetype DownloadModel::indexOf(quint64 id) const {
  for (qsizetype index = 0; index < m_items.size(); ++index) {
    if (m_items.at(index).id == id)
      return index;
  }
  return -1;
}

bool DownloadModel::update(quint64 id, const Item &item) {
  Q_ASSERT(thread() == QThread::currentThread());
  const qsizetype row = indexOf(id);
  if (row < 0)
    return false;
  Item replacement = item;
  replacement.id = id;
  m_items[row] = replacement;
  emit dataChanged(index(int(row)), index(int(row)));
  return true;
}

void DownloadModel::cancel(quint64 id) {
  Q_ASSERT(thread() == QThread::currentThread());
  const qsizetype row = indexOf(id);
  if (row < 0 || m_items[row].finished || m_items[row].state == Cancelled)
    return;
  emit cancelRequested(id);
  m_items[row].state = Cancelled;
  m_items[row].finished = true;
  emit dataChanged(index(int(row)), index(int(row)),
                   {StateRole, FinishedRole, CanCancelRole, CanRemoveRole});
}

void DownloadModel::remove(quint64 id) {
  Q_ASSERT(thread() == QThread::currentThread());
  const qsizetype row = indexOf(id);
  if (row < 0 || (!m_items[row].finished && m_items[row].state != Interrupted))
    return;
  if (!m_items[row].finished)
    emit cancelRequested(id);
  beginRemoveRows({}, int(row), int(row));
  m_items.removeAt(row);
  endRemoveRows();
  emit countChanged();
}
