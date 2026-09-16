// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QTimer>
#include <QtWebEngineQuick/QtWebEngineQuick>

int main(int argc, char *argv[]) {
  QtWebEngineQuick::initialize();
  QApplication application(argc, argv);
  QCommandLineParser parser;
  parser.addHelpOption();
  QCommandLineOption storageOption(QStringLiteral("storage"), QString(),
                                   QStringLiteral("path"));
  QCommandLineOption urlOption(QStringLiteral("url"), QString(),
                               QStringLiteral("url"));
  parser.addOptions({storageOption, urlOption});
  parser.process(application);
  const QString storage = parser.value(storageOption);
  const QUrl url(parser.value(urlOption));
  if (storage.isEmpty() || !url.isValid())
    return 2;

  static const char qml[] = R"QML(
import QtQuick
import QtWebEngine
Window {
    id: root
    width: 320; height: 240; visible: true
    required property string storagePath
    required property string persistenceScript
    required property url targetUrl
    property var webProfile: null
    signal finished()
    WebEngineProfilePrototype {
        id: profilePrototype
        storageName: "push-persistence-helper"
        persistentStoragePath: root.storagePath
        cachePath: root.storagePath + "/cache"
        persistentCookiesPolicy: WebEngineProfile.AllowPersistentCookies
        persistentPermissionsPolicy: WebEngineProfile.StoreOnDisk
    }
    Loader {
        id: viewLoader
        anchors.fill: parent
        active: false
        sourceComponent: WebEngineView {
            profile: root.webProfile
            onPermissionRequested: function(permission) { permission.grant() }
            userScripts.collection: [{
                name: "WebAppContainer Push Subscription Persistence",
                sourceCode: root.persistenceScript,
                injectionPoint: WebEngineScript.DocumentCreation,
                worldId: WebEngineScript.MainWorld,
                runsOnSubFrames: false
            }]
            onTitleChanged: {
                if (title === "PUSH_TEST_DONE")
                    root.finished()
            }
        }
    }
    Component.onCompleted: {
        root.webProfile = profilePrototype.instance()
        if (root.webProfile === null) {
            root.finished()
            return
        }
        root.webProfile.isPushServiceEnabled = true
        viewLoader.active = true
        viewLoader.item.url = root.targetUrl
    }
}
)QML";
  QQmlApplicationEngine engine;
  QQmlComponent component(&engine);
  component.setData(qml, QUrl(QStringLiteral("qrc:/tests/PushHelper.qml")));
  if (!component.isReady())
    return 3;
  QObject *root = component.createWithInitialProperties({
      {QStringLiteral("storagePath"), storage},
      {QStringLiteral("targetUrl"), url},
      {QStringLiteral("persistenceScript"), [&] {
         QFile script(QStringLiteral(":/scripts/push-subscription-persistence.js"));
         return script.open(QIODevice::ReadOnly)
                    ? QString::fromUtf8(script.readAll()) : QString();
       }()},
  });
  if (!root)
    return 4;
  QObject::connect(root, SIGNAL(finished()), &application, SLOT(quit()));
  QTimer::singleShot(60000, &application, [&application] { application.exit(5); });
  const int result = application.exec();
  delete root;
  engine.collectGarbage();
  QCoreApplication::processEvents();
  return result;
}
