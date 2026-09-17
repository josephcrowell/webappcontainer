// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QApplication>
#include "app/mediadevicesalt.h"
#include <QCommandLineParser>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickWebEngineProfile>
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
  QCommandLineOption cxxProfileOption(QStringLiteral("cxx-profile"));
  QCommandLineOption pregrantMediaOption(QStringLiteral("pregrant-media"));
  parser.addOptions({storageOption, urlOption, cxxProfileOption,
                     pregrantMediaOption});
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
    required property string mediaBootstrapScript
    property var externalProfile: null
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
            }, {
                name: "WebAppContainer Media Permission Bootstrap",
                sourceCode: root.mediaBootstrapScript,
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
        root.webProfile = root.externalProfile !== null
                          ? root.externalProfile : profilePrototype.instance()
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
  QQuickWebEngineProfile *externalProfile = nullptr;
  if (parser.isSet(cxxProfileOption)) {
    if (!MediaDeviceSalt::ensure(storage))
      return 6;
    externalProfile = new QQuickWebEngineProfile(
        QStringLiteral("push-persistence-helper"));
    externalProfile->setPersistentStoragePath(storage);
    externalProfile->setCachePath(storage + QStringLiteral("/cache"));
    externalProfile->setPersistentCookiesPolicy(
        QQuickWebEngineProfile::AllowPersistentCookies);
    externalProfile->setPersistentPermissionsPolicy(
        QQuickWebEngineProfile::PersistentPermissionsPolicy::StoreOnDisk);
    externalProfile->setPushServiceEnabled(true);
  }
  QObject *root = component.createWithInitialProperties({
      {QStringLiteral("storagePath"), storage},
      {QStringLiteral("targetUrl"), url},
      {QStringLiteral("externalProfile"), QVariant::fromValue(externalProfile)},
      {QStringLiteral("persistenceScript"), [&] {
         QFile script(QStringLiteral(":/scripts/push-subscription-persistence.js"));
         return script.open(QIODevice::ReadOnly)
                    ? QString::fromUtf8(script.readAll()) : QString();
       }()},
      {QStringLiteral("mediaBootstrapScript"), parser.isSet(pregrantMediaOption)
           ? QStringLiteral(R"JS((() => {
               const devices = navigator.mediaDevices;
               const enumerate = devices.enumerateDevices.bind(devices);
               const getMedia = devices.getUserMedia.bind(devices);
               const ready = getMedia({audio:true, video:true})
                 .then(stream => stream.getTracks().forEach(track => track.stop()))
                 .catch(async () => {
                   const stream = await getMedia({audio:true});
                   stream.getTracks().forEach(track => track.stop());
                 }).catch(() => {});
               devices.enumerateDevices = async () => { await ready; return enumerate(); };
             })())JS") : QString()},
  });
  if (!root)
    return 4;
  QObject::connect(root, SIGNAL(finished()), &application, SLOT(quit()));
  QTimer::singleShot(60000, &application, [&application] { application.exit(5); });
  const int result = application.exec();
  delete root;
  engine.collectGarbage();
  QCoreApplication::processEvents();
  delete externalProfile;
  return result;
}
