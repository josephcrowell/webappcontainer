// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "downloadcontroller.h"

#include "downloadmodel.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QQuickWebEngineDownloadRequest>
#include <QQuickWebEngineProfile>
#include <QStandardPaths>
#include <QWebEngineDownloadRequest>

DownloadController::DownloadController(DownloadModel *model, QObject *parent,
                                       SavePathSelector selector)
    : QObject(parent), m_model(model), m_savePathSelector(std::move(selector)) {
  if (!m_savePathSelector) {
    m_savePathSelector = [](const QString &directory, const QString &fileName) {
      return QFileDialog::getSaveFileName(nullptr, tr("Save download"),
                                          QDir(directory).filePath(fileName));
    };
  }
  connect(model, &DownloadModel::cancelRequested, this, [this](quint64 id) {
    const auto iterator = m_active.find(id);
    if (iterator != m_active.end() && iterator->request)
      iterator->request->cancel();
  });
}

void DownloadController::attachProfile(QQuickWebEngineProfile *profile) {
  connect(profile, &QQuickWebEngineProfile::downloadRequested, this,
          &DownloadController::requested, Qt::DirectConnection);
}

void DownloadController::requested(QQuickWebEngineDownloadRequest *quickRequest) {
  auto *request = static_cast<QWebEngineDownloadRequest *>(quickRequest);
  if (!request)
    return;
  if (request->isSavePageDownload()) {
    request->cancel();
    return;
  }

  QString initialDirectory = request->downloadDirectory();
  if (initialDirectory.isEmpty())
    initialDirectory = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
  const QString suggestedName = QFileInfo(request->suggestedFileName()).fileName();
  const QString selected = m_savePathSelector(initialDirectory, suggestedName);
  if (selected.isEmpty())
    return;

  const QFileInfo destination(selected);
  request->setDownloadDirectory(destination.absolutePath());
  request->setDownloadFileName(destination.fileName());

  DownloadModel::Item item;
  item.fileName = destination.fileName();
  item.destination = destination.absoluteFilePath();
  item.source = request->url();
  item.receivedBytes = request->receivedBytes();
  item.totalBytes = request->totalBytes();
  item.state = DownloadModel::Requested;
  const quint64 modelId = m_model->append(item);
  ActiveDownload active;
  active.request = request;
  active.timer.start();
  m_active.insert(modelId, active);

  const auto refresh = [this, modelId] { update(modelId); };
  connect(request, &QWebEngineDownloadRequest::stateChanged, this,
          [refresh](QWebEngineDownloadRequest::DownloadState) { refresh(); });
  connect(request, &QWebEngineDownloadRequest::receivedBytesChanged, this, refresh);
  connect(request, &QWebEngineDownloadRequest::totalBytesChanged, this, refresh);
  connect(request, &QWebEngineDownloadRequest::interruptReasonChanged, this, refresh);
  connect(request, &QWebEngineDownloadRequest::isFinishedChanged, this, refresh);
  connect(request, &QObject::destroyed, this, [this, modelId] { m_active.remove(modelId); });
  request->accept();
  update(modelId);
}

void DownloadController::update(quint64 modelId) {
  auto iterator = m_active.find(modelId);
  if (iterator == m_active.end() || !iterator->request)
    return;
  QWebEngineDownloadRequest *request = iterator->request;
  DownloadModel::Item item;
  item.fileName = request->downloadFileName();
  item.destination =
      QDir(request->downloadDirectory()).filePath(request->downloadFileName());
  item.source = request->url();
  item.receivedBytes = request->receivedBytes();
  item.totalBytes = request->totalBytes();
  item.finished = request->isFinished();
  item.interruptionReason = request->interruptReasonString();
  switch (request->state()) {
  case QWebEngineDownloadRequest::DownloadRequested:
    item.state = DownloadModel::Requested;
    break;
  case QWebEngineDownloadRequest::DownloadInProgress:
    item.state = DownloadModel::InProgress;
    break;
  case QWebEngineDownloadRequest::DownloadCompleted:
    item.state = DownloadModel::Completed;
    break;
  case QWebEngineDownloadRequest::DownloadCancelled:
    item.state = DownloadModel::Cancelled;
    break;
  case QWebEngineDownloadRequest::DownloadInterrupted:
    item.state = DownloadModel::Interrupted;
    break;
  }
  const qint64 elapsed = iterator->timer.elapsed();
  item.bytesPerSecond = elapsed > 0 && item.receivedBytes > 0
                            ? item.receivedBytes * 1000 / elapsed
                            : 0;
  m_model->update(modelId, item);
}
