// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/startup.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

class TestStartup : public QObject {
  Q_OBJECT

private slots:
  void init();
  void cleanup();
  void disabledDoesNotChangeEnvironment();
  void configuredLibraryAddsDocumentedFlag();
  void existingOverrideIsPreserved();
  void missingLibraryIsReported();
  void resolvesBareExecutableFromPath();
  void parsesApplicationNameBeforeApplicationConstruction();

private:
  QByteArray m_originalFlags;
  QByteArray m_originalPath;
};

void TestStartup::init() {
  m_originalFlags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
  m_originalPath = qgetenv("PATH");
  qunsetenv("QTWEBENGINE_CHROMIUM_FLAGS");
}

void TestStartup::cleanup() {
  qputenv("QTWEBENGINE_CHROMIUM_FLAGS", m_originalFlags);
  qputenv("PATH", m_originalPath);
}

void TestStartup::disabledDoesNotChangeEnvironment() {
  qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--existing=value");
  const Startup::WidevineResult result =
      Startup::configureWidevine(QStringLiteral("app"), {});
  QVERIFY(!result.environmentChanged);
  QCOMPARE(qgetenv("QTWEBENGINE_CHROMIUM_FLAGS"), QByteArray("--existing=value"));
}

void TestStartup::configuredLibraryAddsDocumentedFlag() {
  QTemporaryDir directory(QStringLiteral("widevine path with spaces-XXXXXX"));
  QVERIFY(directory.isValid());
  const QString library = directory.filePath(QStringLiteral("libwidevinecdm.so"));
  QFile file(library);
  QVERIFY(file.open(QIODevice::WriteOnly));
  QVERIFY(file.write("test-cdm") > 0);
  file.close();
  qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--existing=value");

  Startup::WidevineOptions options;
  options.enabled = true;
  options.configuredLibraryPath = library;
  const Startup::WidevineResult result =
      Startup::configureWidevine(QStringLiteral("app"), options);
  QVERIFY(result.environmentChanged);
  QCOMPARE(result.libraryPath, QFileInfo(library).canonicalFilePath());
  const QStringList arguments = QProcess::splitCommand(
      QString::fromLocal8Bit(qgetenv("QTWEBENGINE_CHROMIUM_FLAGS")));
  QVERIFY(arguments.contains(QStringLiteral("--existing=value")));
  QVERIFY(arguments.contains(QStringLiteral("--widevine-path=%1").arg(result.libraryPath)));
}

void TestStartup::existingOverrideIsPreserved() {
  const QByteArray existing = "--widevine-path=/chosen/by/user --other=value";
  qputenv("QTWEBENGINE_CHROMIUM_FLAGS", existing);
  Startup::WidevineOptions options;
  options.enabled = true;
  options.configuredLibraryPath = QStringLiteral("/missing/library");
  const Startup::WidevineResult result =
      Startup::configureWidevine(QStringLiteral("app"), options);
  QVERIFY(result.alreadyConfigured);
  QVERIFY(!result.environmentChanged);
  QCOMPARE(qgetenv("QTWEBENGINE_CHROMIUM_FLAGS"), existing);
}

void TestStartup::missingLibraryIsReported() {
  Startup::WidevineOptions options;
  options.enabled = true;
  options.searchFallbacks = false;
  options.configuredLibraryPath = QStringLiteral("/definitely/missing/libwidevinecdm.so");
  const Startup::WidevineResult result = Startup::configureWidevine(
      QStringLiteral("/definitely/missing/webappcontainer"), options);
  QVERIFY(!result.environmentChanged);
  QVERIFY(!result.error.isEmpty());
}

void TestStartup::resolvesBareExecutableFromPath() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString executable = directory.filePath(QStringLiteral("fake-webappcontainer"));
  QFile file(executable);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("#!/bin/sh\n");
  file.close();
  QVERIFY(file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                              QFileDevice::ExeOwner));
  qputenv("PATH", directory.path().toLocal8Bit());
  QCOMPARE(Startup::resolveExecutablePath(QStringLiteral("fake-webappcontainer")),
           QFileInfo(executable).canonicalFilePath());
}

void TestStartup::parsesApplicationNameBeforeApplicationConstruction() {
  QByteArray executable("webappcontainer");
  QByteArray longOption("--name");
  QByteArray dialpad("DialPad");
  char *separate[] = {executable.data(), longOption.data(), dialpad.data()};
  QCOMPARE(Startup::applicationNameFromArguments(
               3, separate, QStringLiteral("Web App Container")),
           QStringLiteral("DialPad"));

  QByteArray assigned("--name=Messenger");
  char *equals[] = {executable.data(), assigned.data()};
  QCOMPARE(Startup::applicationNameFromArguments(
               2, equals, QStringLiteral("Web App Container")),
           QStringLiteral("Messenger"));

  QByteArray shortOption("-n");
  QByteArray empty;
  char *emptyName[] = {executable.data(), shortOption.data(), empty.data()};
  QCOMPARE(Startup::applicationNameFromArguments(
               3, emptyName, QStringLiteral("Web App Container")),
           QStringLiteral("Web App Container"));

  QByteArray longAlias("--n");
  char *exactUserArguments[] = {executable.data(), longAlias.data(), dialpad.data()};
  QCOMPARE(Startup::applicationNameFromArguments(
               3, exactUserArguments, QStringLiteral("Web App Container")),
           QStringLiteral("DialPad"));
}

QTEST_APPLESS_MAIN(TestStartup)
#include "tst_startup.moc"
