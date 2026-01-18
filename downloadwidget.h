// Copyright(C) 2026 Joseph Crowell.
// SPDX - License - Identifier : GPL-2.0-or-later

#ifndef DOWNLOADWIDGET_H
#define DOWNLOADWIDGET_H

#include "ui_downloadwidget.h"

#include <QElapsedTimer>
#include <QFrame>

QT_BEGIN_NAMESPACE
class QWebEngineDownloadRequest;
QT_END_NAMESPACE

// Displays one ongoing or finished download (QWebEngineDownloadRequest).
class DownloadWidget final : public QFrame, public Ui::DownloadWidget {
  Q_OBJECT
public:
  // Precondition: The QWebEngineDownloadRequest has been accepted.
  explicit DownloadWidget(QWebEngineDownloadRequest *download,
                          QWidget *parent = nullptr);

signals:
  // This signal is emitted when the user indicates that they want to remove
  // this download from the downloads list.
  void removeClicked(DownloadWidget *self);

private slots:
  void updateWidget();

private:
  QString withUnit(qreal bytes);

  QWebEngineDownloadRequest *m_download;
  QElapsedTimer m_timeAdded;
};

#endif // DOWNLOADWIDGET_H
