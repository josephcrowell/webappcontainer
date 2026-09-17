// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/startup.h"
#include "app/launchconfiguration.h"
#include "app/iconloader.h"
#include "app/navigationpolicy.h"
#include "app/notificationmodel.h"
#include "app/notificationpresenter.h"
#include "app/appcontroller.h"
#include "app/downloadmodel.h"
#include "app/downloadcontroller.h"
#include "app/profileservice.h"
#include "app/securitybridge.h"
#include "app/settingsstore.h"

#include <QApplication>
#include <QQuickWindow>
#include <QWindow>
#include <QQmlApplicationEngine>
#include <QtWebEngineQuick/QtWebEngineQuick>

int main(int argc, char *argv[]) {
  const QString initialApplicationName = Startup::applicationNameFromArguments(
      argc, argv, QStringLiteral("Web App Container"));
  QCoreApplication::setOrganizationName(QStringLiteral("JosephCrowell"));
  QCoreApplication::setApplicationName(initialApplicationName);
  QGuiApplication::setApplicationDisplayName(initialApplicationName);

  Startup::WidevineOptions widevineOptions;
#ifdef WIDEVINE_CDM_ENABLED
  widevineOptions.enabled = true;
#ifdef WIDEVINE_CDM_PATH
  widevineOptions.configuredLibraryPath = QStringLiteral(WIDEVINE_CDM_PATH);
#endif
#endif
  Startup::configureWidevine(QString::fromLocal8Bit(argv[0]), widevineOptions);

  QtWebEngineQuick::initialize();
  QApplication application(argc, argv);
  application.setApplicationVersion(QStringLiteral("1.0.0"));

  LaunchConfiguration launchConfiguration(application);
  QIcon applicationIcon = IconLoader::load(launchConfiguration.iconPath());
  if (applicationIcon.isNull())
    applicationIcon = IconLoader::load(QStringLiteral(":/icons/default.svg"));
  QApplication::setWindowIcon(applicationIcon);
  ProfileService profileService(launchConfiguration);
  SettingsStore settingsStore(launchConfiguration.profilePath());
  settingsStore.configureInitialOrigin(launchConfiguration.startUrl());
  AppController appController;
  QIcon trayIcon = IconLoader::load(launchConfiguration.trayIconPath());
  if (trayIcon.isNull())
    trayIcon = applicationIcon;
  appController.configureTrayIcon(trayIcon);
  NavigationPolicy navigationPolicy;
  SecurityBridge securityBridge;
  DownloadModel downloadModel;
  DownloadController downloadController(&downloadModel);
  downloadController.attachProfile(profileService.profile());
  NotificationModel notificationModel;
  notificationModel.attachProfile(profileService.profile());
  NotificationPresenter notificationPresenter(
      &notificationModel, launchConfiguration.applicationName(),
      appController.trayIconUrl());
  application.setQuitOnLastWindowClosed(false);
  QObject::connect(&application, &QCoreApplication::aboutToQuit, &settingsStore,
                   [&settingsStore] { settingsStore.sync(); });
  QQmlApplicationEngine engine;
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
      [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
  engine.setInitialProperties({
      {QStringLiteral("launchConfiguration"),
       QVariant::fromValue(&launchConfiguration)},
      {QStringLiteral("profileService"), QVariant::fromValue(&profileService)},
      {QStringLiteral("settingsStore"), QVariant::fromValue(&settingsStore)},
      {QStringLiteral("appController"), QVariant::fromValue(&appController)},
      {QStringLiteral("navigationPolicy"), QVariant::fromValue(&navigationPolicy)},
      {QStringLiteral("securityBridge"), QVariant::fromValue(&securityBridge)},
      {QStringLiteral("downloadModel"), QVariant::fromValue(&downloadModel)},
      {QStringLiteral("notificationModel"), QVariant::fromValue(&notificationModel)},
      {QStringLiteral("notificationPresenter"),
       QVariant::fromValue(&notificationPresenter)},
  });
  engine.loadFromModule(QStringLiteral("WebAppContainer"), QStringLiteral("Main"));
  if (engine.rootObjects().isEmpty())
    return EXIT_FAILURE;
  if (auto *mainWindow = qobject_cast<QWindow *>(engine.rootObjects().constFirst())) {
    mainWindow->setIcon(applicationIcon);
    if (auto *quickWindow = qobject_cast<QQuickWindow *>(mainWindow)) {
      quickWindow->setPersistentGraphics(true);
      quickWindow->setPersistentSceneGraph(true);
      AppController::configureHiddenGraphicsCleanup(quickWindow);
    }
    settingsStore.restoreWindowGeometry(mainWindow);
    settingsStore.bindWindow(mainWindow);
    QMetaObject::invokeMethod(mainWindow, "completeStartup", Qt::DirectConnection);
  }
  return application.exec();
}
