// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "app/settingsstore.h"

#include <QDir>
#include <QGuiApplication>
#include <QScreen>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <QWindow>
#include <QWidget>
#include <QWebEnginePermission>

class TestSettingsStore : public QObject {
  Q_OBJECT

private slots:
  void defaultsAndPersistence();
  void preservesLegacyGeometry();
  void persistsOriginScopedPermissionGrant();
  void migratesLegacyHostPermissionGrant();
  void clampsGeometryToAvailableScreen();
  void savesAndRestoresScreenRelativeGeometry();
  void restoresMatchingSecondaryScreen();
  void choosesMatchingScreenByIdentityAndGeometry();
  void persistsProtocolHandlerAllowAndDeny();
  void migratesLegacyProtocolHandlerDecision();
  void migratesLegacyWidgetGeometry();
  void createsMediaBootstrapOnlyForSavedOriginGrants();
};

void TestSettingsStore::defaultsAndPersistence() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  {
    SettingsStore settings(directory.path());
    QCOMPARE(settings.hideOnMinimize(), false);
    QCOMPARE(settings.hideOnClose(), true);
    QCOMPARE(settings.windowWidth(), 967);
    QCOMPARE(settings.windowHeight(), 557);
    settings.setHideOnMinimize(true);
    settings.setHideOnClose(false);
    settings.setWindowWidth(1200);
    settings.setWindowHeight(700);
    settings.setWindowMaximized(true);
    QVERIFY(settings.sync());
  }
  SettingsStore restored(directory.path());
  QVERIFY(restored.hideOnMinimize());
  QVERIFY(!restored.hideOnClose());
  QCOMPARE(restored.windowWidth(), 1200);
  QCOMPARE(restored.windowHeight(), 700);
  QVERIFY(restored.windowMaximized());
}

void TestSettingsStore::preservesLegacyGeometry() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("settings.ini"));
  const QByteArray legacy("opaque-widget-geometry");
  {
    QSettings raw(path, QSettings::IniFormat);
    raw.setValue(QStringLiteral("BrowserWindow/geometry"), legacy);
    raw.sync();
  }
  {
    SettingsStore settings(directory.path());
    settings.setWindowWidth(800);
    QVERIFY(settings.sync());
  }
  QSettings raw(path, QSettings::IniFormat);
  QCOMPARE(raw.value(QStringLiteral("BrowserWindow/geometry")).toByteArray(), legacy);
}

void TestSettingsStore::persistsOriginScopedPermissionGrant() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QUrl origin(QStringLiteral("https://example.com:8443/path"));
  {
    SettingsStore settings(directory.path());
    QVERIFY(!settings.hasPermissionGrant(origin, 2));
    QVERIFY(settings.rememberPermissionGrant(origin, 2));
  }
  SettingsStore restored(directory.path());
  QVERIFY(restored.hasPermissionGrant(origin, 2));
  QVERIFY(!restored.hasPermissionGrant(QUrl(QStringLiteral("http://example.com:8443")), 2));
  QVERIFY(!restored.hasPermissionGrant(QUrl(QStringLiteral("https://example.com")), 2));
  QVERIFY(!restored.hasPermissionGrant(origin, 3));
}

void TestSettingsStore::migratesLegacyHostPermissionGrant() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("settings.ini"));
  {
    QSettings legacy(path, QSettings::IniFormat);
    legacy.setValue(QStringLiteral("Permissions/grants/example_com/7"), true);
    legacy.sync();
  }
  const QUrl origin(QStringLiteral("https://example.com"));
  {
    SettingsStore settings(directory.path());
    QVERIFY(settings.hasPermissionGrant(origin, 7));
  }
  QSettings migrated(path, QSettings::IniFormat);
  migrated.remove(QStringLiteral("Permissions/grants/example_com/7"));
  migrated.sync();
  SettingsStore restored(directory.path());
  QVERIFY(restored.hasPermissionGrant(origin, 7));
}

void TestSettingsStore::clampsGeometryToAvailableScreen() {
  const QRect available(1920, -200, 1280, 1024);
  QCOMPARE(SettingsStore::clampGeometry(QRect(9000, 9000, 1600, 1200), available),
           QRect(1920, -200, 1280, 1024));
  QCOMPARE(SettingsStore::clampGeometry(QRect(2000, -100, 800, 600), available),
           QRect(2000, -100, 800, 600));
}

void TestSettingsStore::savesAndRestoresScreenRelativeGeometry() {
  QScreen *screen = QGuiApplication::primaryScreen();
  QVERIFY(screen);
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QRect available = screen->availableGeometry();
  const QSize size(qMin(700, available.width()), qMin(500, available.height()));
  const QPoint position(available.x() + qMax(0, qMin(80, available.width() - size.width())),
                        available.y() + qMax(0, qMin(60, available.height() - size.height())));
  {
    SettingsStore settings(directory.path());
    QWindow source(screen);
    source.setGeometry(QRect(position, size));
    settings.saveWindowGeometry(&source);
    QVERIFY(settings.sync());
  }
  SettingsStore restored(directory.path());
  QVERIFY(restored.hasWindowPosition());
  QCOMPARE(restored.windowScreenId(), SettingsStore::screenIdentifier(screen));
  QWindow destination;
  restored.restoreWindowGeometry(&destination);
  QCOMPARE(destination.screen(), screen);
  QCOMPARE(destination.geometry(), QRect(position, size));
}

void TestSettingsStore::restoresMatchingSecondaryScreen() {
  const QList<QScreen *> screens = QGuiApplication::screens();
  if (screens.size() < 2)
    QSKIP("A second screen is required for screen-selection coverage");
  QScreen *screen = screens.at(1);
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  {
    SettingsStore settings(directory.path());
    QWindow source(screen);
    const QRect available = screen->availableGeometry();
    source.setGeometry(QRect(available.topLeft() + QPoint(40, 30), QSize(640, 480)));
    settings.saveWindowGeometry(&source);
    QVERIFY(settings.sync());
  }
  SettingsStore restored(directory.path());
  QWindow destination;
  restored.restoreWindowGeometry(&destination);
  QCOMPARE(destination.screen(), screen);
  QVERIFY(screen->availableGeometry().contains(destination.geometry()));
}

void TestSettingsStore::choosesMatchingScreenByIdentityAndGeometry() {
  const QList<SettingsStore::ScreenDescription> screens = {
      {QStringLiteral("same-id"), QStringLiteral("Display"), QRect(0, 0, 1920, 1080)},
      {QStringLiteral("same-id"), QStringLiteral("Display"), QRect(1920, 0, 1920, 1080)},
      {QStringLiteral("third-id"), QStringLiteral("Other"), QRect(-1280, 0, 1280, 1024)},
  };
  QCOMPARE(SettingsStore::chooseScreen(QStringLiteral("same-id"),
                                       QStringLiteral("Display"),
                                       QRect(1920, 0, 1920, 1080), screens),
           qsizetype(1));
  QCOMPARE(SettingsStore::chooseScreen(QStringLiteral("missing"),
                                       QStringLiteral("Other"), QRect(), screens),
           qsizetype(2));
  QCOMPARE(SettingsStore::chooseScreen(QStringLiteral("missing"),
                                       QStringLiteral("missing"), QRect(), screens),
           qsizetype(-1));
}

void TestSettingsStore::persistsProtocolHandlerAllowAndDeny() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QUrl allowedOrigin(QStringLiteral("https://app.example.com"));
  const QUrl deniedOrigin(QStringLiteral("https://denied.example.com:8443"));
  {
    SettingsStore settings(directory.path());
    QCOMPARE(settings.protocolHandlerDecision(allowedOrigin, QStringLiteral("web+call")), -1);
    QVERIFY(settings.rememberProtocolHandlerDecision(
        allowedOrigin, QStringLiteral("web+call"), true));
    QVERIFY(settings.rememberProtocolHandlerDecision(
        deniedOrigin, QStringLiteral("web+message"), false));
  }
  SettingsStore restored(directory.path());
  QCOMPARE(restored.protocolHandlerDecision(allowedOrigin, QStringLiteral("web+call")), 1);
  QCOMPARE(restored.protocolHandlerDecision(deniedOrigin, QStringLiteral("web+message")), 0);
  QCOMPARE(restored.protocolHandlerDecision(
               QUrl(QStringLiteral("http://app.example.com")), QStringLiteral("web+call")),
           -1);
  QCOMPARE(restored.protocolHandlerDecision(
               QUrl(QStringLiteral("https://denied.example.com")),
               QStringLiteral("web+message")),
           -1);
  QCOMPARE(restored.protocolHandlerDecision(allowedOrigin, QStringLiteral("web+other")), -1);
}

void TestSettingsStore::migratesLegacyProtocolHandlerDecision() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("settings.ini"));
  {
    QSettings legacy(path, QSettings::IniFormat);
    legacy.setValue(
        QStringLiteral("Permissions/ProtocolHandlers/app.example.com/web+call"), true);
    legacy.setValue(
        QStringLiteral("Permissions/ProtocolHandlers/app.example.com/web+deny"), false);
    legacy.sync();
  }
  const QUrl origin(QStringLiteral("https://app.example.com"));
  {
    SettingsStore settings(directory.path());
    QCOMPARE(settings.protocolHandlerDecision(origin, QStringLiteral("web+call")), 1);
    QCOMPARE(settings.protocolHandlerDecision(origin, QStringLiteral("web+deny")), 0);
  }
  QSettings raw(path, QSettings::IniFormat);
  raw.remove(QStringLiteral("Permissions/ProtocolHandlers"));
  raw.sync();
  SettingsStore restored(directory.path());
  QCOMPARE(restored.protocolHandlerDecision(origin, QStringLiteral("web+call")), 1);
  QCOMPARE(restored.protocolHandlerDecision(origin, QStringLiteral("web+deny")), 0);
}

void TestSettingsStore::migratesLegacyWidgetGeometry() {
  QScreen *screen = QGuiApplication::primaryScreen();
  QVERIFY(screen);
  const QRect available = screen->availableGeometry();
  const QRect expected(available.x() + 70, available.y() + 50,
                       qMin(900, available.width()), qMin(650, available.height()));
  QWidget legacy;
  legacy.setGeometry(expected);
  const QByteArray geometry = legacy.saveGeometry();
  QVERIFY(!geometry.isEmpty());

  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  {
    QSettings raw(directory.filePath(QStringLiteral("settings.ini")),
                  QSettings::IniFormat);
    raw.setValue(QStringLiteral("BrowserWindow/geometry"), geometry);
    raw.setValue(QStringLiteral("QuickWindow/v1/maximized"), true);
    raw.sync();
  }
  SettingsStore migrated(directory.path());
  QVERIFY(migrated.hasWindowPosition());
  QVERIFY(migrated.windowMaximized());
  QCOMPARE(migrated.windowScreenId(), SettingsStore::screenIdentifier(screen));
  QWindow restored;
  migrated.restoreWindowGeometry(&restored);
  QCOMPARE(restored.geometry(), expected);
}

void TestSettingsStore::createsMediaBootstrapOnlyForSavedOriginGrants() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QUrl dialpad(QStringLiteral("https://dialpad.com/app"));
  SettingsStore settings(directory.path());
  settings.configureInitialOrigin(dialpad);
  QVERIFY(settings.mediaPermissionBootstrapScript().isEmpty());
  QVERIFY(settings.rememberPermissionGrant(
      dialpad, int(QWebEnginePermission::PermissionType::MediaAudioCapture)));
  settings.configureInitialOrigin(dialpad);
  QVERIFY(settings.mediaPermissionBootstrapScript().contains(
      QStringLiteral("audio: true")));
  QVERIFY(settings.mediaPermissionBootstrapScript().contains(
      QStringLiteral("video: false")));
  settings.configureInitialOrigin(QUrl(QStringLiteral("https://example.com")));
  QVERIFY(settings.mediaPermissionBootstrapScript().isEmpty());
}

QTEST_MAIN(TestSettingsStore)
#include "tst_settingsstore.moc"
