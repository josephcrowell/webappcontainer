// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QPointer>
#include <QElapsedTimer>
#include <QHash>
#include <functional>

class DownloadModel;
class QQuickWebEngineDownloadRequest;
class QQuickWebEngineProfile;
class QWebEngineDownloadRequest;

class DownloadController final : public QObject {
  Q_OBJECT

public:
  using SavePathSelector = std::function<QString(const QString &, const QString &)>;
  explicit DownloadController(DownloadModel *model, QObject *parent = nullptr,
                              SavePathSelector selector = {});
  void attachProfile(QQuickWebEngineProfile *profile);

private:
  struct ActiveDownload {
    QPointer<QWebEngineDownloadRequest> request;
    QElapsedTimer timer;
  };
  void requested(QQuickWebEngineDownloadRequest *quickRequest);
  void update(quint64 modelId);
  DownloadModel *m_model = nullptr;
  QHash<quint64, ActiveDownload> m_active;
  SavePathSelector m_savePathSelector;
};
