// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/startup.h"
#include "app/downloadcontroller.h"
#include "app/downloadmodel.h"
#include "app/notificationmodel.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHostAddress>
#include <QProcess>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWebEngineProfile>
#include <QQuickWindow>
#include <QWebEngineFileSystemAccessRequest>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTcpSocket>
#include <QTest>
#include <QtWebEngineQuick/QtWebEngineQuick>

class TestQuickWebEngine : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();
  void serviceWorkerApiAvailable();
  void serviceWorkerRegistrationContract();
  void serviceWorkerOnHttpOrigin();
  void emeApiAvailable();
  void widevineCdmExists();
  void widevineKeySystem();
  void realDataDownload();
  void certificateFrameClassification();
  void popupRequestIsAdopted();
  void webNotificationPresentation();
  void fileSystemAccessSignalWiring();
  void notificationPermissionSurvivesProfileRestart();
  void serviceWorkerRegistrationSurvivesProfileRestart();

private:
  void load(const QString &html);
  QString evaluate(const QString &script);
  QString waitForDom(const QString &id, const QString &initial,
                     int timeoutMs = 10000);

  QTemporaryDir m_tempDir;
  QQmlEngine m_engine;
  QQuickWebEngineProfile *m_profile = nullptr;
  QObject *m_harness = nullptr;
  DownloadModel *m_downloadModel = nullptr;
  DownloadController *m_downloadController = nullptr;
  NotificationModel *m_notificationModel = nullptr;
  QUrl m_registeredServiceWorkerOrigin;
  QString m_serviceWorkerDocumentRoot;
  QString m_serviceWorkerScriptPath;
  quint16 m_serviceWorkerPort = 0;
};

void TestQuickWebEngine::initTestCase() {
  QVERIFY(m_tempDir.isValid());
  m_profile = new QQuickWebEngineProfile(QStringLiteral("quick-test"), this);
  m_profile->setPersistentStoragePath(m_tempDir.filePath(QStringLiteral("profile")));
  m_profile->setCachePath(m_tempDir.filePath(QStringLiteral("cache")));
  m_profile->setPushServiceEnabled(true);
  m_downloadModel = new DownloadModel(this);
  m_downloadController = new DownloadController(
      m_downloadModel, this,
      [this](const QString &, const QString &) {
        return m_tempDir.filePath(QStringLiteral("download-result.bin"));
      });
  m_downloadController->attachProfile(m_profile);
  m_notificationModel = new NotificationModel(this);
  m_notificationModel->attachProfile(m_profile);

  static const char qml[] = R"QML(
import QtQuick
import QtWebEngine
Window {
    width: 640
    height: 480
    visible: true
    required property var webProfile
    property string pendingHtml
    property url pendingBaseUrl: "http://localhost/"
    property string pendingScript
    property url pendingUrl
    signal loadCompleted(bool success)
    signal scriptCompleted(string value)
    signal certificateObserved(bool mainFrame, bool overridable)
    signal popupAdopted(url requestedUrl)
    signal fileSystemAccessObserved(string origin, string path, int handleType, int accessFlags)
    function loadContent() { view.loadHtml(pendingHtml, pendingBaseUrl) }
    function loadUrl() { view.url = pendingUrl }
    function evaluate() {
        view.runJavaScript(pendingScript, function(value) {
            scriptCompleted(String(value))
        })
    }
    WebEngineView {
        id: view
        objectName: "mainWebView"
        anchors.fill: parent
        profile: webProfile
        settings.javascriptEnabled: true
        settings.localStorageEnabled: true
        onLoadingChanged: function(info) {
            if (info.status === WebEngineLoadingInfo.LoadSucceededStatus)
                loadCompleted(true)
            else if (info.status === WebEngineLoadingInfo.LoadFailedStatus)
                loadCompleted(false)
        }
        onCertificateError: function(error) {
            certificateObserved(error.isMainFrame, error.overridable)
            error.rejectCertificate()
        }
        onPermissionRequested: function(permission) { permission.grant() }
        onNewWindowRequested: function(request) {
            request.openIn(popupView)
            popupAdopted(request.requestedUrl)
        }
        onFileSystemAccessRequested: function(request) {
            fileSystemAccessObserved(request.origin.toString(),
                                     request.filePath.toString(),
                                     request.handleType,
                                     request.accessFlags)
            request.reject()
        }
    }
    WebEngineView {
        id: popupView
        width: 320
        height: 240
        visible: false
        profile: webProfile
    }
}
)QML";
  QQmlComponent component(&m_engine);
  component.setData(qml, QUrl(QStringLiteral("qrc:/tests/QuickHarness.qml")));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));
  m_harness = component.createWithInitialProperties(
      {{QStringLiteral("webProfile"), QVariant::fromValue(m_profile)}});
  QVERIFY2(m_harness, qPrintable(component.errorString()));
  auto *window = qobject_cast<QQuickWindow *>(m_harness);
  QVERIFY(window);
  QVERIFY2(QTest::qWaitForWindowExposed(window, 5000),
           "Quick WebEngine test window was not exposed");
}

void TestQuickWebEngine::cleanupTestCase() {
  delete m_harness;
  m_harness = nullptr;
  delete m_profile;
  m_profile = nullptr;
}

void TestQuickWebEngine::load(const QString &html) {
  QSignalSpy spy(m_harness, SIGNAL(loadCompleted(bool)));
  QVERIFY(spy.isValid());
  m_harness->setProperty("pendingHtml", html);
  QVERIFY(QMetaObject::invokeMethod(m_harness, "loadContent"));
  QVERIFY2(spy.wait(10000), "Quick WebEngine load timed out");
  QVERIFY(spy.constFirst().constFirst().toBool());
}

QString TestQuickWebEngine::evaluate(const QString &script) {
  QSignalSpy spy(m_harness, SIGNAL(scriptCompleted(QString)));
  if (!spy.isValid()) {
    QTest::qFail("Invalid scriptCompleted signal spy", __FILE__, __LINE__);
    return {};
  }
  m_harness->setProperty("pendingScript", script);
  if (!QMetaObject::invokeMethod(m_harness, "evaluate")) {
    QTest::qFail("Could not invoke Quick JavaScript evaluator", __FILE__, __LINE__);
    return {};
  }
  if (!spy.wait(5000))
    return {};
  return spy.constFirst().constFirst().toString();
}

QString TestQuickWebEngine::waitForDom(const QString &id, const QString &initial,
                                       int timeoutMs) {
  QString value;
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < timeoutMs) {
    value = evaluate(QStringLiteral("document.getElementById('%1').textContent").arg(id));
    if (!value.isEmpty() && value != initial)
      return value;
    QTest::qWait(50);
  }
  return value;
}

void TestQuickWebEngine::serviceWorkerApiAvailable() {
  load(QStringLiteral(R"HTML(<div id="result">Testing...</div><script>
    document.getElementById('result').textContent =
      ('serviceWorker' in navigator) ? 'SW_API_SUPPORTED' : 'SW_API_NOT_SUPPORTED';
  </script>)HTML"));
  QCOMPARE(waitForDom(QStringLiteral("result"), QStringLiteral("Testing...")),
           QStringLiteral("SW_API_SUPPORTED"));
}

void TestQuickWebEngine::serviceWorkerRegistrationContract() {
  load(QStringLiteral(R"HTML(<div id="result">Testing...</div><script>
    const result = document.getElementById('result');
    const blob = new Blob(["self.addEventListener('install',()=>self.skipWaiting())"],
                          {type:'application/javascript'});
    navigator.serviceWorker.register(URL.createObjectURL(blob))
      .then(()=>result.textContent='SW_REGISTERED')
      .catch(error=>result.textContent=(error.toString().includes('not supported') ||
                                      error.toString().includes('blob:'))
                                      ? 'SW_API_WORKING_NEEDS_HTTPS'
                                      : 'SW_REGISTRATION_ERROR');
  </script>)HTML"));
  const QString result = waitForDom(QStringLiteral("result"), QStringLiteral("Testing..."));
  QVERIFY(result == QStringLiteral("SW_REGISTERED") ||
          result == QStringLiteral("SW_API_WORKING_NEEDS_HTTPS"));
}

void TestQuickWebEngine::serviceWorkerOnHttpOrigin() {
  const QString root = m_tempDir.filePath(QStringLiteral("quick-http"));
  QVERIFY(QDir().mkpath(root));
  const QString resources = QStringLiteral(TEST_RESOURCES_DIR);
  QVERIFY(QFile::copy(resources + QStringLiteral("/test_sw.html"),
                      root + QStringLiteral("/test_sw.html")));
  QVERIFY(QFile::copy(resources + QStringLiteral("/sw.js"),
                      root + QStringLiteral("/sw.js")));
  const QString scriptPath = m_tempDir.filePath(QStringLiteral("quick_http_server.py"));
  QFile script(scriptPath);
  QVERIFY(script.open(QIODevice::WriteOnly | QIODevice::Truncate));
  script.write(R"PY(import http.server
import os
import sys
os.chdir(sys.argv[1])
class Handler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        if self.path.endswith('.js'):
            self.send_header('Content-Type', 'application/javascript; charset=utf-8')
            self.send_header('Service-Worker-Allowed', '/')
        super().end_headers()
port = int(sys.argv[2]) if len(sys.argv) > 2 else 0
server = http.server.ThreadingHTTPServer(('127.0.0.1', port), Handler)
print(server.server_port, flush=True)
server.serve_forever()
)PY");
  script.close();

  QProcess server;
  const auto cleanup = qScopeGuard([&server] {
    if (server.state() == QProcess::NotRunning)
      return;
    server.terminate();
    if (!server.waitForFinished(5000)) {
      server.kill();
      server.waitForFinished(5000);
    }
  });
  server.start(QStringLiteral("python3"), {scriptPath, root});
  QVERIFY(server.waitForStarted(5000));
  QVERIFY(server.waitForReadyRead(5000));
  bool portOk = false;
  const quint16 port = server.readLine().trimmed().toUShort(&portOk);
  QVERIFY(portOk && port != 0);
  m_serviceWorkerDocumentRoot = root;
  m_serviceWorkerScriptPath = scriptPath;
  m_serviceWorkerPort = port;

  QTcpSocket socket;
  socket.connectToHost(QHostAddress::LocalHost, port);
  QVERIFY(socket.waitForConnected(5000));
  socket.write("GET /test_sw.html HTTP/1.0\r\nHost: localhost\r\n\r\n");
  QVERIFY(socket.waitForBytesWritten(5000));
  QVERIFY(socket.waitForReadyRead(5000));
  const QByteArray response = socket.readAll();
  QVERIFY(response.startsWith("HTTP/1.0 200") || response.startsWith("HTTP/1.1 200"));

  QSignalSpy loadSpy(m_harness, SIGNAL(loadCompleted(bool)));
  QVERIFY(loadSpy.isValid());
  m_harness->setProperty(
      "pendingUrl", QUrl(QStringLiteral("http://localhost:%1/test_sw.html").arg(port)));
  m_registeredServiceWorkerOrigin =
      QUrl(QStringLiteral("http://localhost:%1/").arg(port));
  QVERIFY(QMetaObject::invokeMethod(m_harness, "loadUrl"));
  QVERIFY(loadSpy.wait(10000));
  QVERIFY(loadSpy.constFirst().constFirst().toBool());
  QCOMPARE(waitForDom(QStringLiteral("result"), QStringLiteral("Testing...")),
           QStringLiteral("SW_REGISTERED_ACTIVE"));
}

void TestQuickWebEngine::emeApiAvailable() {
  load(QStringLiteral(R"(<div id="result">Testing...</div><script>
    document.getElementById('result').textContent =
      (typeof navigator.requestMediaKeySystemAccess !== 'undefined')
      ? 'EME_SUPPORTED' : 'EME_NOT_SUPPORTED';
  </script>)"));
  QCOMPARE(waitForDom(QStringLiteral("result"), QStringLiteral("Testing...")),
           QStringLiteral("EME_SUPPORTED"));
}

void TestQuickWebEngine::widevineCdmExists() {
#ifdef WIDEVINE_CDM_PATH
  const QFileInfo file(QStringLiteral(WIDEVINE_CDM_PATH));
  QVERIFY(file.exists());
  QVERIFY(file.isFile());
  QVERIFY(file.isReadable());
  QVERIFY(file.size() > 0);
#else
  QSKIP("Widevine CDM was not enabled for this build");
#endif
}

void TestQuickWebEngine::widevineKeySystem() {
#ifdef WIDEVINE_CDM_PATH
  load(QStringLiteral(R"HTML(<div id="result">Testing...</div><script>
    const result = document.getElementById('result');
    navigator.requestMediaKeySystemAccess('com.widevine.alpha', [{
      initDataTypes: ['cenc'],
      audioCapabilities: [{contentType: 'audio/mp4; codecs="mp4a.40.2"'}],
      videoCapabilities: [{contentType: 'video/mp4; codecs="avc1.42E01E"'}]
    }]).then(()=>result.textContent='WIDEVINE_AVAILABLE')
       .catch(error=>result.textContent='WIDEVINE_UNAVAILABLE:' + error.name);
  </script>)HTML"));
  const QString result = waitForDom(QStringLiteral("result"), QStringLiteral("Testing..."));
  if (result.startsWith(QStringLiteral("WIDEVINE_UNAVAILABLE:")))
    QSKIP(qPrintable(result));
  QCOMPARE(result, QStringLiteral("WIDEVINE_AVAILABLE"));
#else
  QSKIP("Widevine CDM was not enabled for this build");
#endif
}

void TestQuickWebEngine::realDataDownload() {
  const QString destination = m_tempDir.filePath(QStringLiteral("download-result.bin"));
  QFile::remove(destination);
  load(QStringLiteral(R"HTML(<a id="download"
      href="data:application/octet-stream;base64,aGVsbG8tZG93bmxvYWQ="
      download="payload.bin">download</a>)HTML"));
  QSignalSpy insertSpy(m_downloadModel, &QAbstractItemModel::rowsInserted);
  evaluate(QStringLiteral("document.getElementById('download').click(); 'clicked'"));
  QVERIFY2(insertSpy.wait(5000) || m_downloadModel->rowCount() > 0,
           "Quick WebEngine did not issue a download request");
  QTRY_VERIFY_WITH_TIMEOUT(
      m_downloadModel->data(m_downloadModel->index(0), DownloadModel::FinishedRole).toBool(),
      10000);
  QFile result(destination);
  QVERIFY(result.open(QIODevice::ReadOnly));
  QCOMPARE(result.readAll(), QByteArray("hello-download"));
}

void TestQuickWebEngine::certificateFrameClassification() {
  const QString tlsRoot = m_tempDir.filePath(QStringLiteral("tls"));
  QVERIFY(QDir().mkpath(tlsRoot));
  const QString keyPath = QDir(tlsRoot).filePath(QStringLiteral("key.pem"));
  const QString certificatePath = QDir(tlsRoot).filePath(QStringLiteral("cert.pem"));
  QProcess openssl;
  openssl.start(QStringLiteral("openssl"),
                {QStringLiteral("req"), QStringLiteral("-x509"),
                 QStringLiteral("-newkey"), QStringLiteral("rsa:2048"),
                 QStringLiteral("-nodes"), QStringLiteral("-subj"),
                 QStringLiteral("/CN=localhost"), QStringLiteral("-keyout"),
                 keyPath, QStringLiteral("-out"), certificatePath,
                 QStringLiteral("-days"), QStringLiteral("1")});
  QVERIFY(openssl.waitForFinished(10000));
  QCOMPARE(openssl.exitCode(), 0);

  const QString pagePath = QDir(tlsRoot).filePath(QStringLiteral("index.html"));
  QFile page(pagePath);
  QVERIFY(page.open(QIODevice::WriteOnly | QIODevice::Truncate));
  page.write("TLS fixture");
  page.close();
  const QString scriptPath = QDir(tlsRoot).filePath(QStringLiteral("https_server.py"));
  QFile script(scriptPath);
  QVERIFY(script.open(QIODevice::WriteOnly | QIODevice::Truncate));
  script.write(R"PY(import http.server
import os
import ssl
import sys
os.chdir(sys.argv[1])
server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), http.server.SimpleHTTPRequestHandler)
context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
context.load_cert_chain(sys.argv[2], sys.argv[3])
server.socket = context.wrap_socket(server.socket, server_side=True)
print(server.server_port, flush=True)
server.serve_forever()
)PY");
  script.close();

  QProcess server;
  const auto cleanup = qScopeGuard([&server] {
    if (server.state() == QProcess::NotRunning)
      return;
    server.terminate();
    if (!server.waitForFinished(5000)) {
      server.kill();
      server.waitForFinished(5000);
    }
  });
  server.start(QStringLiteral("python3"),
               {scriptPath, tlsRoot, certificatePath, keyPath});
  QVERIFY(server.waitForStarted(5000));
  QVERIFY(server.waitForReadyRead(5000));
  bool portOk = false;
  const quint16 port = server.readLine().trimmed().toUShort(&portOk);
  QVERIFY(portOk && port != 0);

  QSignalSpy certificateSpy(m_harness, SIGNAL(certificateObserved(bool,bool)));
  QVERIFY(certificateSpy.isValid());
  QSignalSpy loadSpy(m_harness, SIGNAL(loadCompleted(bool)));
  m_harness->setProperty(
      "pendingUrl", QUrl(QStringLiteral("https://localhost:%1/index.html").arg(port)));
  QVERIFY(QMetaObject::invokeMethod(m_harness, "loadUrl"));
  QVERIFY(certificateSpy.wait(10000));
  QVERIFY(certificateSpy.constFirst().at(0).toBool());
  QVERIFY(certificateSpy.constFirst().at(1).toBool());
  if (loadSpy.isEmpty())
    QVERIFY(loadSpy.wait(10000));
  QVERIFY(!loadSpy.constLast().constFirst().toBool());

  certificateSpy.clear();
  load(QStringLiteral("<html><img src='https://localhost:%1/index.html'></html>")
           .arg(port));
  QVERIFY(certificateSpy.count() > 0 || certificateSpy.wait(10000));
  bool sawSubresource = false;
  for (const QList<QVariant> &arguments : certificateSpy) {
    if (!arguments.at(0).toBool()) {
      sawSubresource = true;
      break;
    }
  }
  QVERIFY2(sawSubresource, "Expected a subresource certificate error");
}

void TestQuickWebEngine::popupRequestIsAdopted() {
  load(QStringLiteral("<html><body>popup fixture</body></html>"));
  QSignalSpy popupSpy(m_harness, SIGNAL(popupAdopted(QUrl)));
  QVERIFY(popupSpy.isValid());
  evaluate(QStringLiteral("window.open('https://example.com/popup'); 'opened'"));
  QVERIFY2(!popupSpy.isEmpty() || popupSpy.wait(5000),
           "Quick WebEngine did not emit a popup request");
  QCOMPARE(popupSpy.constFirst().constFirst().toUrl(),
           QUrl(QStringLiteral("https://example.com/popup")));
}

void TestQuickWebEngine::webNotificationPresentation() {
  QSignalSpy presentedSpy(m_notificationModel,
                          &NotificationModel::notificationPresented);
  QSignalSpy activatedSpy(m_notificationModel,
                          &NotificationModel::notificationActivated);
  load(QStringLiteral(R"HTML(<div id="result">Testing...</div><script>
    Notification.requestPermission().then(permission => {
      if (permission !== 'granted') {
        document.getElementById('result').textContent = 'NOTIFICATION_DENIED';
        return;
      }
      new Notification('Fixture title', {body: 'Fixture body', tag: 'fixture-tag'});
      document.getElementById('result').textContent = 'NOTIFICATION_CREATED';
    });
  </script>)HTML"));
  QCOMPARE(waitForDom(QStringLiteral("result"), QStringLiteral("Testing...")),
           QStringLiteral("NOTIFICATION_CREATED"));
  QTRY_COMPARE_WITH_TIMEOUT(m_notificationModel->count(), 1, 5000);
  const QModelIndex index = m_notificationModel->index(0);
  QCOMPARE(m_notificationModel->data(index, NotificationModel::TitleRole).toString(),
           QStringLiteral("Fixture title"));
  QCOMPARE(m_notificationModel->data(index, NotificationModel::MessageRole).toString(),
           QStringLiteral("Fixture body"));
  const quint64 id =
      m_notificationModel->data(index, NotificationModel::IdRole).toULongLong();
  QCOMPARE(presentedSpy.count(), 1);
  QCOMPARE(presentedSpy.constFirst().at(0).toULongLong(), id);
  QCOMPARE(presentedSpy.constFirst().at(1).toString(), QStringLiteral("Fixture title"));
  QCOMPARE(presentedSpy.constFirst().at(2).toString(), QStringLiteral("Fixture body"));
  m_notificationModel->click(id);
  QCOMPARE(activatedSpy.count(), 1);
  QCOMPARE(m_notificationModel->count(), 0);
}

void TestQuickWebEngine::fileSystemAccessSignalWiring() {
  QSignalSpy accessSpy(
      m_harness, SIGNAL(fileSystemAccessObserved(QString,QString,int,int)));
  QVERIFY(accessSpy.isValid());
  QObject *view = m_harness->findChild<QObject *>(QStringLiteral("mainWebView"));
  QVERIFY(view);
  QWebEngineFileSystemAccessRequest request;
  QVERIFY(QMetaObject::invokeMethod(
      view, "fileSystemAccessRequested", Qt::DirectConnection,
      Q_ARG(QWebEngineFileSystemAccessRequest, request)));
  QCOMPARE(accessSpy.count(), 1);
}

void TestQuickWebEngine::notificationPermissionSurvivesProfileRestart() {
  const QString storage = m_tempDir.filePath(QStringLiteral("permission-restart"));
  const QUrl origin(QStringLiteral("https://push.example.com"));
  {
    QQuickWebEngineProfile profile(QStringLiteral("permission-restart"));
    profile.setPersistentStoragePath(storage);
    profile.setCachePath(m_tempDir.filePath(QStringLiteral("permission-restart-cache")));
    profile.setPersistentPermissionsPolicy(
        QQuickWebEngineProfile::PersistentPermissionsPolicy::StoreOnDisk);
    QWebEnginePermission permission = profile.queryPermission(
        origin, QWebEnginePermission::PermissionType::Notifications);
    QVERIFY(permission.isValid());
    permission.grant();
    QCOMPARE(permission.state(), QWebEnginePermission::State::Granted);
  }
  QCoreApplication::processEvents();
  {
    QQuickWebEngineProfile profile(QStringLiteral("permission-restart"));
    profile.setPersistentStoragePath(storage);
    profile.setCachePath(m_tempDir.filePath(QStringLiteral("permission-restart-cache")));
    profile.setPersistentPermissionsPolicy(
        QQuickWebEngineProfile::PersistentPermissionsPolicy::StoreOnDisk);
    const QWebEnginePermission restored = profile.queryPermission(
        origin, QWebEnginePermission::PermissionType::Notifications);
    QCOMPARE(restored.state(), QWebEnginePermission::State::Granted);
  }
}

void TestQuickWebEngine::serviceWorkerRegistrationSurvivesProfileRestart() {
  QVERIFY(m_registeredServiceWorkerOrigin.isValid());
  QVERIFY(!m_serviceWorkerDocumentRoot.isEmpty());
  QVERIFY(!m_serviceWorkerScriptPath.isEmpty());
  QVERIFY(m_serviceWorkerPort != 0);
  delete m_harness;
  m_harness = nullptr;
  m_engine.collectGarbage();
  QCoreApplication::processEvents();
  QTest::qWait(100);
  delete m_profile;
  m_profile = nullptr;
  QCoreApplication::processEvents();

  m_profile = new QQuickWebEngineProfile(QStringLiteral("quick-test"), this);
  m_profile->setPersistentStoragePath(m_tempDir.filePath(QStringLiteral("profile")));
  m_profile->setCachePath(m_tempDir.filePath(QStringLiteral("cache")));
  m_profile->setPersistentPermissionsPolicy(
      QQuickWebEngineProfile::PersistentPermissionsPolicy::StoreOnDisk);
  m_profile->setPushServiceEnabled(true);

  QProcess server;
  const auto cleanup = qScopeGuard([&server] {
    if (server.state() == QProcess::NotRunning)
      return;
    server.terminate();
    if (!server.waitForFinished(5000)) {
      server.kill();
      server.waitForFinished(5000);
    }
  });
  server.start(QStringLiteral("python3"),
               {m_serviceWorkerScriptPath, m_serviceWorkerDocumentRoot,
                QString::number(m_serviceWorkerPort)});
  QVERIFY(server.waitForStarted(5000));
  QVERIFY(server.waitForReadyRead(5000));
  bool portOk = false;
  QCOMPARE(server.readLine().trimmed().toUShort(&portOk), m_serviceWorkerPort);
  QVERIFY(portOk);
  QFile checkPage(QDir(m_serviceWorkerDocumentRoot)
                      .filePath(QStringLiteral("check_sw.html")));
  QVERIFY(checkPage.open(QIODevice::WriteOnly | QIODevice::Truncate));
  checkPage.write(R"HTML(<div id="result">Testing...</div><script>
    navigator.serviceWorker.getRegistration().then(registration => {
      document.getElementById('result').textContent =
        registration ? 'SW_RESTORED' : 'SW_MISSING';
    });
  </script>)HTML");
  checkPage.close();

  static const char restartQml[] = R"QML(
import QtQuick
import QtWebEngine
Window {
    width: 320; height: 240; visible: true
    required property var webProfile
    property string pendingHtml
    property url pendingBaseUrl
    property string pendingScript
    property url pendingUrl
    signal loadCompleted(bool success)
    signal scriptCompleted(string value)
    function loadContent() { view.loadHtml(pendingHtml, pendingBaseUrl) }
    function loadUrl() { view.url = pendingUrl }
    function evaluate() {
        view.runJavaScript(pendingScript, function(value) {
            scriptCompleted(String(value))
        })
    }
    WebEngineView {
        id: view
        anchors.fill: parent
        profile: webProfile
        onLoadingChanged: function(info) {
            if (info.status === WebEngineLoadingInfo.LoadSucceededStatus)
                loadCompleted(true)
            else if (info.status === WebEngineLoadingInfo.LoadFailedStatus)
                loadCompleted(false)
        }
    }
}
)QML";
  QQmlComponent component(&m_engine);
  component.setData(restartQml,
                    QUrl(QStringLiteral("qrc:/tests/RestartHarness.qml")));
  QVERIFY2(component.isReady(), qPrintable(component.errorString()));
  m_harness = component.createWithInitialProperties(
      {{QStringLiteral("webProfile"), QVariant::fromValue(m_profile)}});
  QVERIFY2(m_harness, qPrintable(component.errorString()));
  auto *window = qobject_cast<QQuickWindow *>(m_harness);
  QVERIFY(window);
  QVERIFY(QTest::qWaitForWindowExposed(window, 5000));
  QSignalSpy loadSpy(m_harness, SIGNAL(loadCompleted(bool)));
  QVERIFY(loadSpy.isValid());
  m_harness->setProperty(
      "pendingUrl", m_registeredServiceWorkerOrigin.resolved(
                        QUrl(QStringLiteral("check_sw.html"))));
  QVERIFY(QMetaObject::invokeMethod(m_harness, "loadUrl"));
  QVERIFY(loadSpy.wait(10000));
  QVERIFY(loadSpy.constFirst().constFirst().toBool());
  QCOMPARE(waitForDom(QStringLiteral("result"), QStringLiteral("Testing...")),
           QStringLiteral("SW_RESTORED"));
}

int main(int argc, char *argv[]) {
  Startup::WidevineOptions options;
#ifdef WIDEVINE_CDM_PATH
  options.enabled = true;
  options.configuredLibraryPath = QStringLiteral(WIDEVINE_CDM_PATH);
#endif
  Startup::configureWidevine(QString::fromLocal8Bit(argv[0]), options);
  QtWebEngineQuick::initialize();
  QApplication application(argc, argv);
  TestQuickWebEngine test;
  return QTest::qExec(&test, argc, argv);
}

#include "tst_quickwebengine.moc"
