// Copyright(C) 2026 Joseph Crowell.
// SPDX - License - Identifier : GPL-2.0-or-later

#include "browserwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QString>
#include <QStyle>
#include <QTranslator>
#include <QWebEngineProfile>
#include <QWebEngineProfileBuilder>
#include <QWebEngineSettings>

using namespace Qt::StringLiterals;

// Find Widevine CDM library at runtime
// Searches in order: environment override, lib folder next to executable,
// system lib Note: Can't use QCoreApplication::applicationDirPath() before
// QApplication exists, so we need to parse argv[0] manually
static QString findWidevineCdm(const char *argv0) {
  // Get executable directory from argv[0]
  QString exePath = QString::fromLocal8Bit(argv0);
  QFileInfo exeInfo(exePath);
  QString appDir = exeInfo.absolutePath();

  QStringList searchPaths = {
      // Development build: build/widevine/libwidevinecdm.so
      appDir + "/widevine/libwidevinecdm.so",
      // Installed location: <prefix>/lib/webappcontainer/libwidevinecdm.so
      appDir + "/../lib/webappcontainer/libwidevinecdm.so",
      // Flatpak/AppImage style: libs next to binary
      appDir + "/lib/libwidevinecdm.so",
      appDir + "/libwidevinecdm.so",
      // System-wide fallback
      "/usr/lib/webappcontainer/libwidevinecdm.so",
      "/usr/local/lib/webappcontainer/libwidevinecdm.so",
  };

  for (const QString &path : searchPaths) {
    QFileInfo fi(path);
    if (fi.exists() && fi.isFile()) {
      return fi.canonicalFilePath();
    }
  }

  return QString();
}

// Setup Widevine CDM for DRM playback
// MUST be called BEFORE QApplication is created!
static void setupWidevineCdm(const char *argv0) {
#ifdef WIDEVINE_CDM_ENABLED
  // Check if user already specified a Widevine path via environment
  QByteArray existingFlags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
  if (existingFlags.contains("widevine-cdm-path")) {
    // Already configured, don't override
    return;
  }

  QString widevinePath = findWidevineCdm(argv0);
  if (widevinePath.isEmpty()) {
    // Will warn later after logging is set up
    return;
  }

  // Get the directory containing the Widevine CDM
  // Chromium expects the directory, not the .so file directly
  // It will look for _platform_specific/linux_x64/libwidevinecdm.so
  QFileInfo cdmFile(widevinePath);
  QString widevineDir = cdmFile.absolutePath();

  // Set Chromium flags for Widevine
  // Qt WebEngine uses --widevine-path, NOT --widevine-cdm-path
  QByteArray flags = existingFlags;
  if (!flags.isEmpty()) {
    flags += " ";
  }
  flags += "--widevine-path=" + widevinePath.toUtf8();
  qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags);
#endif
}

int main(int argc, char *argv[]) {
  // Setup Widevine BEFORE QApplication (Qt WebEngine initializes with
  // QApplication)
  setupWidevineCdm(argv[0]);

  // Enable service workers on localhost (allows self-signed certs for testing)
  QByteArray flags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
  if (!flags.contains("allow-insecure-localhost")) {
    if (!flags.isEmpty())
      flags += " ";
    flags += "--allow-insecure-localhost";
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags);
  }

  QApplication application(argc, argv);

  // Log Widevine status after application is created
#ifdef WIDEVINE_CDM_ENABLED
  flags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
  if (flags.contains("widevine-cdm-path")) {
    qDebug() << "Widevine CDM enabled via:" << flags;
  } else {
    qWarning() << "Widevine CDM not found. DRM content (Netflix, Spotify, "
                  "etc.) may not play.";
  }
#else
  qDebug() << "Widevine CDM support not compiled in (ENABLE_WIDEVINE=OFF)";
#endif

#ifdef QT_DEBUG
  QLoggingCategory::setFilterRules(u"qt.webenginecontext.debug=true"_s);
#else
  QLoggingCategory::setFilterRules(u"qt.webenginecontext.debug=false"_s);
#endif

  application.setOrganizationName("JosephCrowell");
  application.setApplicationName("Web App Container");
  application.setApplicationVersion("1.0.0");

  QTranslator translator;
  const QStringList uiLanguages = QLocale::system().uiLanguages();
  for (const QString &locale : uiLanguages) {
    const QString baseName = "webappcontainer_" + QLocale(locale).name();
    if (translator.load(":/i18n/" + baseName)) {
      application.installTranslator(&translator);
      break;
    }
  }

  // Get command line arguments
  QCommandLineParser parser;
  parser.setApplicationDescription("Web App Container\n"
                                   "A simple web container for web apps\n"
                                   "Copyright (C) 2026 Joseph Crowell\n"
                                   "License: GNU GPL version 2 or later "
                                   "<https://gnu.org/licenses/gpl.html>");
  parser.addHelpOption();
  parser.addVersionOption();

  QCommandLineOption urlOption(QStringList() << "u" << "url",
                               "The URL to open on startup.", "url");
  parser.addOption(urlOption);

  QCommandLineOption appIdOption(
      QStringList() << "a" << "app-id",
      "The unique desktop entry ID for this instance.", "id");
  parser.addOption(appIdOption);

  QCommandLineOption profileOption(QStringList() << "p" << "profile",
                                   "The Profile name to use.", "profile");
  parser.addOption(profileOption);

  QCommandLineOption nameOption(QStringList() << "n" << "name",
                                "The name of the application.", "name");
  parser.addOption(nameOption);

  QCommandLineOption iconOption(QStringList() << "i" << "icon",
                                "The application icon.", "icon");
  parser.addOption(iconOption);

  QCommandLineOption trayIconOption(QStringList() << "t" << "tray-icon",
                                    "The tray icon.", "trayicon");
  parser.addOption(trayIconOption);

  QCommandLineOption minimizedOption(
      QStringList() << "minimized",
      "Start the application minimized to the tray.");
  parser.addOption(minimizedOption);

  QCommandLineOption notifyOption(
      QStringList() << "no-notify",
      "Don't notify when minimizing or closing to the tray.");
  parser.addOption(notifyOption);

  parser.process(application);
  QString startUrl = parser.value(urlOption);
  QString appId = parser.value(appIdOption);
  QString profileName = parser.value(profileOption);
  QString appName = parser.value(nameOption);
  QString iconPath = parser.value(iconOption);
  QString trayIconPath = parser.value(trayIconOption);
  bool startMinimized = parser.isSet(minimizedOption);
  bool notify = !parser.isSet(notifyOption);

  if (!appId.isEmpty()) {
    QString desktopPath =
        QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) +
        "/" + appId + ".desktop";

    if (QFile::exists(desktopPath) ||
        QFile::exists("/usr/share/applications/" + appId + ".desktop")) {
      QApplication::setDesktopFileName(appId);
    } else {
      qWarning() << "No desktop file found for" << appId
                 << "- skipping Portal registration.";
    }
  }

  // If we have an app name, set it
  if (!appName.isEmpty()) {
    application.setApplicationName(appName);
    application.setApplicationDisplayName(appName);
  }

  QString baseRoot =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  QString name = profileName.isEmpty() ? u"default"_s : profileName;
  QString profilePath;
  profilePath =
      baseRoot + QDir::separator() + "QtWebEngine" + QDir::separator() + name;
  QDir().mkpath(profilePath);

  // Create the profile and set paths
  QWebEngineProfile *profile = new QWebEngineProfile(name, &application);
  profile->setPersistentStoragePath(profilePath);
  profile->setCachePath(profilePath + QDir::separator() + "cache");

  // Set policies
  profile->setPersistentPermissionsPolicy(
      QWebEngineProfile::PersistentPermissionsPolicy::StoreOnDisk);
  profile->setPersistentCookiesPolicy(
      QWebEngineProfile::AllowPersistentCookies);

  // Enable Web Push API for service worker push notifications
  profile->setPushServiceEnabled(true);

  // Set remaining profile settings
  profile->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled,
                                    true);
  profile->settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled,
                                    true);
  profile->settings()->setAttribute(QWebEngineSettings::PluginsEnabled, true);
  profile->settings()->setAttribute(QWebEngineSettings::DnsPrefetchEnabled,
                                    true);
  profile->settings()->setAttribute(
      QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
  profile->settings()->setAttribute(
      QWebEngineSettings::LocalContentCanAccessFileUrls, false);
  profile->settings()->setAttribute(QWebEngineSettings::ScreenCaptureEnabled,
                                    true);
  profile->setHttpUserAgent(
      "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
      "Chrome/143.0.7499.169/170 Safari/537.36");

  if (profile->isOffTheRecord()) {
    qWarning() << "Warning: Profile is still Off-The-Record! This should not "
                  "happen with a named profile.";
  } else {
    qDebug() << "Profile is On-The-Record (Persistent). Storage path:"
             << profile->persistentStoragePath();
  }

  BrowserWindow *window =
      new BrowserWindow(profile, appName, iconPath, trayIconPath, notify);

  if (window->isValidImage(iconPath)) {
    application.setWindowIcon(QIcon(iconPath));
  } else {
    application.setWindowIcon(
        window->style()->standardIcon(QStyle::SP_TitleBarMenuButton));
  }

  // Set an initial URL
  if (startUrl.isEmpty()) {
    window->webView()->setUrl(QUrl("https://www.google.com"));
  } else {
    QUrl url(startUrl);

    // If no scheme is provided, default to https
    if (url.scheme().isEmpty()) {
      url = QUrl("https://" + startUrl);
    }

    window->webView()->setUrl(url);
  }

  if (startMinimized) {
    window->show();
    window->hide();
  } else {
    window->show();
  }

  return application.exec();
}
