// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "launchconfiguration.h"
#include "iconloader.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QStandardPaths>

LaunchConfiguration::LaunchConfiguration(QCoreApplication &application, QObject *parent)
    : QObject(parent) {
  QCommandLineParser parser;
  parser.setApplicationDescription(
      QStringLiteral("Web App Container\nA simple web container for web apps\n"
                     "Copyright (C) 2026 Joseph Crowell\n"
                     "License: GNU GPL version 2 or later <https://gnu.org/licenses/gpl.html>"));
  parser.addHelpOption();
  parser.addVersionOption();
  const QCommandLineOption urlOption({QStringLiteral("u"), QStringLiteral("url")},
                                     QStringLiteral("The URL to open on startup."),
                                     QStringLiteral("url"));
  const QCommandLineOption appIdOption({QStringLiteral("a"), QStringLiteral("app-id")},
                                       QStringLiteral("The unique desktop entry ID for this instance."),
                                       QStringLiteral("id"));
  const QCommandLineOption profileOption({QStringLiteral("p"), QStringLiteral("profile")},
                                         QStringLiteral("The Profile name to use."),
                                         QStringLiteral("profile"));
  const QCommandLineOption nameOption({QStringLiteral("n"), QStringLiteral("name")},
                                      QStringLiteral("The name of the application."),
                                      QStringLiteral("name"));
  const QCommandLineOption iconOption({QStringLiteral("i"), QStringLiteral("icon")},
                                      QStringLiteral("The application icon."),
                                      QStringLiteral("icon"));
  const QCommandLineOption trayIconOption({QStringLiteral("t"), QStringLiteral("tray-icon")},
                                          QStringLiteral("The tray icon."),
                                          QStringLiteral("trayicon"));
  const QCommandLineOption minimizedOption(QStringLiteral("minimized"),
                                           QStringLiteral("Start the application minimized to the tray."));
  const QCommandLineOption noNotifyOption(QStringLiteral("no-notify"),
                                          QStringLiteral("Don't notify when minimizing or closing to the tray."));
  parser.addOptions({urlOption, appIdOption, profileOption, nameOption, iconOption,
                     trayIconOption, minimizedOption, noNotifyOption});
  parser.process(application);

  m_applicationName = parser.value(nameOption);
  if (m_applicationName.isEmpty())
    m_applicationName = QStringLiteral("Web App Container");
  application.setApplicationName(m_applicationName);
  QGuiApplication::setApplicationDisplayName(m_applicationName);

  const QString appId = parser.value(appIdOption);
  if (!appId.isEmpty()) {
    const QString userDesktop =
        QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) +
        QDir::separator() + appId + QStringLiteral(".desktop");
    const QString systemDesktop =
        QStringLiteral("/usr/share/applications/%1.desktop").arg(appId);
    if (QFile::exists(userDesktop) || QFile::exists(systemDesktop))
      QGuiApplication::setDesktopFileName(appId);
    else
      qWarning() << "No desktop file found for" << appId
                 << "- skipping Portal registration.";
  }

  m_profileName = parser.value(profileOption);
  if (m_profileName.isEmpty())
    m_profileName = QStringLiteral("default");
  m_profilePath =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
      QDir::separator() + QStringLiteral("QtWebEngine") + QDir::separator() +
      m_profileName;

  QString urlText = parser.value(urlOption);
  if (urlText.isEmpty())
    m_startUrl = QUrl(QStringLiteral("https://www.google.com"));
  else {
    QUrl url(urlText);
    if (url.scheme().isEmpty())
      url = QUrl(QStringLiteral("https://") + urlText);
    m_startUrl = url;
  }

  const QString requestedIcon = parser.value(iconOption);
  if (!requestedIcon.isEmpty()) {
    if (IconLoader::isUsable(requestedIcon))
      m_iconPath = requestedIcon;
    else
      qWarning() << "Could not load application icon" << requestedIcon
                 << "- using the packaged fallback.";
  }
  const QString requestedTrayIcon = parser.value(trayIconOption);
  if (!requestedTrayIcon.isEmpty()) {
    if (IconLoader::isUsable(requestedTrayIcon))
      m_trayIconPath = requestedTrayIcon;
    else
      qWarning() << "Could not load tray icon" << requestedTrayIcon
                 << "- using the application icon.";
  }
  m_startMinimized = parser.isSet(minimizedOption);
  m_showBackgroundHints = !parser.isSet(noNotifyOption);
  QFile pushScript(QStringLiteral(":/scripts/push-subscription-persistence.js"));
  if (pushScript.open(QIODevice::ReadOnly))
    m_pushSubscriptionScript = QString::fromUtf8(pushScript.readAll());
  else
    qWarning() << "Could not load the push subscription persistence script";
}

QUrl LaunchConfiguration::trayIconUrl() const {
  const QString path = m_trayIconPath.isEmpty() ? m_iconPath : m_trayIconPath;
  return path.isEmpty() ? QUrl(QStringLiteral("qrc:/icons/default.svg"))
                        : QUrl::fromLocalFile(path);
}
