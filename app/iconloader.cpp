// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "iconloader.h"

#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

namespace IconLoader {

QIcon load(const QString &path) {
  const QFileInfo info(path);
  if (!info.exists() || !info.isFile() || !info.isReadable())
    return {};

  if (info.suffix().compare(QStringLiteral("svg"), Qt::CaseInsensitive) == 0 ||
      info.suffix().compare(QStringLiteral("svgz"), Qt::CaseInsensitive) == 0) {
    QSvgRenderer renderer(path);
    if (!renderer.isValid())
      return {};
    QImage image(QSize(256, 256), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    renderer.render(&painter);
    painter.end();
    return QIcon(QPixmap::fromImage(image));
  }

  QImageReader reader(path);
  if (!reader.canRead())
    return {};
  const QImage image = reader.read();
  return image.isNull() ? QIcon() : QIcon(QPixmap::fromImage(image));
}

bool isUsable(const QString &path) { return !load(path).isNull(); }

} // namespace IconLoader
