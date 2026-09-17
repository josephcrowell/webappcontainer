// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mediadevicesalt.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

namespace MediaDeviceSalt {

bool ensure(const QString &profilePath) {
  if (!QDir().mkpath(profilePath))
    return false;
  const QString path = QDir(profilePath).filePath(QStringLiteral("user_prefs.json"));
  QJsonObject preferences;
  QFile existing(path);
  if (existing.open(QIODevice::ReadOnly)) {
    const QJsonDocument document = QJsonDocument::fromJson(existing.readAll());
    if (document.isObject())
      preferences = document.object();
  }
  const QString literalKey = QStringLiteral("qtwebengine.media_device_salt_id");
  QJsonObject qtWebEngine = preferences.value(QStringLiteral("qtwebengine")).toObject();
  QString salt = qtWebEngine.value(QStringLiteral("media_device_salt_id")).toString();
  if (salt.isEmpty())
    salt = preferences.value(literalKey).toString();
  if (salt.isEmpty())
    salt = QUuid::createUuid().toString(QUuid::WithoutBraces);
  preferences.insert(literalKey, salt);
  qtWebEngine.insert(QStringLiteral("media_device_salt_id"), salt);
  preferences.insert(QStringLiteral("qtwebengine"), qtWebEngine);
  QSaveFile output(path);
  if (!output.open(QIODevice::WriteOnly))
    return false;
  if (output.write(QJsonDocument(preferences).toJson(QJsonDocument::Compact)) < 0)
    return false;
  return output.commit();
}

} // namespace MediaDeviceSalt
