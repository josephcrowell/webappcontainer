// Copyright(C) 2026 Joseph Crowell.
// SPDX - License - Identifier : GPL-2.0-or-later

#include "downloadmanagerwidget.h"

#include "downloadwidget.h"

#include <QDir>
#include <QFileDialog>
#include <QWebEngineDownloadRequest>

DownloadManagerWidget::DownloadManagerWidget(QWidget *parent)
    : QWidget(parent) {
  setupUi(this);
}

void DownloadManagerWidget::downloadRequested(
    QWebEngineDownloadRequest *download) {
  Q_ASSERT(download &&
           download->state() == QWebEngineDownloadRequest::DownloadRequested);

  QString path =
      QFileDialog::getSaveFileName(this, tr("Save as"),
                                   QDir(download->downloadDirectory())
                                       .filePath(download->downloadFileName()));
  if (path.isEmpty())
    return;

  download->setDownloadDirectory(QFileInfo(path).path());
  download->setDownloadFileName(QFileInfo(path).fileName());
  download->accept();
  add(new DownloadWidget(download));

  show();
}

void DownloadManagerWidget::add(DownloadWidget *downloadWidget) {
  connect(downloadWidget, &DownloadWidget::removeClicked, this,
          &DownloadManagerWidget::remove);
  m_itemsLayout->insertWidget(0, downloadWidget, 0, Qt::AlignTop);
  if (m_numDownloads++ == 0)
    m_zeroItemsLabel->hide();
}

void DownloadManagerWidget::remove(DownloadWidget *downloadWidget) {
  m_itemsLayout->removeWidget(downloadWidget);
  downloadWidget->deleteLater();
  if (--m_numDownloads == 0)
    m_zeroItemsLabel->show();
}
