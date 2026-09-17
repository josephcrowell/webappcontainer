// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "app/mediadevicesalt.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

class TestMediaDeviceSalt : public QObject {
  Q_OBJECT

private slots:
  void createsStableSaltWithoutRemovingPreferences();
  void migratesLiteralDottedPreferenceKey();
};

void TestMediaDeviceSalt::createsStableSaltWithoutRemovingPreferences() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("user_prefs.json"));
  QFile initial(path);
  QVERIFY(initial.open(QIODevice::WriteOnly | QIODevice::Truncate));
  initial.write(R"JSON({"gcm":{"product_category_for_subtypes":"org.chromium.linux"}})JSON");
  initial.close();

  QVERIFY(MediaDeviceSalt::ensure(directory.path()));
  QFile firstFile(path);
  QVERIFY(firstFile.open(QIODevice::ReadOnly));
  const QJsonObject first = QJsonDocument::fromJson(firstFile.readAll()).object();
  const QString salt = first.value(QStringLiteral("qtwebengine")).toObject()
                           .value(QStringLiteral("media_device_salt_id")).toString();
  QVERIFY(!salt.isEmpty());
  QCOMPARE(first.value(QStringLiteral("qtwebengine.media_device_salt_id")).toString(),
           salt);
  QCOMPARE(first.value(QStringLiteral("gcm")).toObject()
               .value(QStringLiteral("product_category_for_subtypes")).toString(),
           QStringLiteral("org.chromium.linux"));

  QVERIFY(MediaDeviceSalt::ensure(directory.path()));
  QFile secondFile(path);
  QVERIFY(secondFile.open(QIODevice::ReadOnly));
  const QJsonObject second = QJsonDocument::fromJson(secondFile.readAll()).object();
  QCOMPARE(second.value(QStringLiteral("qtwebengine")).toObject()
               .value(QStringLiteral("media_device_salt_id")).toString(),
           salt);
  QCOMPARE(second.value(QStringLiteral("qtwebengine.media_device_salt_id")).toString(),
           salt);
}

void TestMediaDeviceSalt::migratesLiteralDottedPreferenceKey() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("user_prefs.json"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  file.write(R"JSON({"qtwebengine.media_device_salt_id":"existing-salt"})JSON");
  file.close();
  QVERIFY(MediaDeviceSalt::ensure(directory.path()));
  QFile migratedFile(path);
  QVERIFY(migratedFile.open(QIODevice::ReadOnly));
  const QJsonObject migrated =
      QJsonDocument::fromJson(migratedFile.readAll()).object();
  QCOMPARE(migrated.value(QStringLiteral("qtwebengine.media_device_salt_id")).toString(),
           QStringLiteral("existing-salt"));
  QCOMPARE(migrated.value(QStringLiteral("qtwebengine")).toObject()
               .value(QStringLiteral("media_device_salt_id")).toString(),
           QStringLiteral("existing-salt"));
}

QTEST_APPLESS_MAIN(TestMediaDeviceSalt)
#include "tst_mediadevicesalt.moc"
