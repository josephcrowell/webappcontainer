// Copyright(C) 2026 Joseph Crowell.
// SPDX - License - Identifier : GPL-2.0-or-later

#include "browserwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
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

int main(int argc, char *argv[]) {
  QApplication application(argc, argv);

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

  // Set remaining profile settings
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
