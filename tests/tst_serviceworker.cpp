// Copyright(C) 2026 Joseph Crowell.
// SPDX - License - Identifier : GPL-2.0-or-later

#include <QApplication>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>

class TestServiceWorker : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();

  // Test that service worker API is available
  void testServiceWorkerApiAvailable();

  // Test service worker registration
  void testServiceWorkerRegistration();

  // Test service worker with HTTPS server
  void testServiceWorkerWithHttpsServer();

private:
  QWebEngineProfile *m_profile = nullptr;
  QWebEnginePage *m_page = nullptr;
  QProcess *m_httpsServer = nullptr;
  QTemporaryDir *m_tempDir = nullptr;
  int m_httpsPort = 0;
};

void TestServiceWorker::initTestCase() {
  // Create a test profile
  m_profile = new QWebEngineProfile(QStringLiteral("serviceworker-test"), this);

  // Enable settings required for service workers
  auto settings = m_profile->settings();
  settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
  settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);

  // Enable push service for service worker push notifications
  m_profile->setPushServiceEnabled(true);

  m_page = new QWebEnginePage(m_profile, this);
}

void TestServiceWorker::cleanupTestCase() {
  // Stop HTTPS server if running
  if (m_httpsServer && m_httpsServer->state() == QProcess::Running) {
    m_httpsServer->terminate();
    m_httpsServer->waitForFinished(5000);
    delete m_httpsServer;
    m_httpsServer = nullptr;
  }

  delete m_tempDir;
  m_tempDir = nullptr;
  delete m_page;
  m_page = nullptr;
  delete m_profile;
  m_profile = nullptr;
}

void TestServiceWorker::testServiceWorkerApiAvailable() {
  // Test that the Service Worker API is available in the browser
  QSignalSpy spy(m_page, &QWebEnginePage::loadFinished);

  QString html = R"(
    <!DOCTYPE html>
    <html>
    <head><title>Service Worker API Test</title></head>
    <body>
      <div id="result">Testing...</div>
      <div id="details"></div>
      <script>
        var result = document.getElementById('result');
        var details = document.getElementById('details');

        // Check for Service Worker API
        if ('serviceWorker' in navigator) {
          result.textContent = 'SW_API_SUPPORTED';
          details.textContent = 'PushManager: ' + ('PushManager' in window);
        } else {
          result.textContent = 'SW_API_NOT_SUPPORTED';
        }
      </script>
    </body>
    </html>
  )";

  // Use localhost URL to ensure secure context
  m_page->setHtml(html, QUrl("http://localhost/"));
  QVERIFY(spy.wait(10000));
  QCOMPARE(spy.count(), 1);
  QVERIFY(spy.at(0).at(0).toBool()); // Load succeeded

  // Check the result
  QString result;
  QString details;
  bool finished = false;

  m_page->runJavaScript(
      "document.getElementById('result').textContent",
      [&result, &finished](const QVariant &v) {
        result = v.toString();
        finished = true;
      });

  QTRY_VERIFY_WITH_TIMEOUT(finished, 5000);

  finished = false;
  m_page->runJavaScript(
      "document.getElementById('details').textContent",
      [&details, &finished](const QVariant &v) {
        details = v.toString();
        finished = true;
      });
  QTRY_VERIFY_WITH_TIMEOUT(finished, 5000);

  qDebug() << "Service Worker API test details:" << details;

  QVERIFY2(result == "SW_API_SUPPORTED",
           qPrintable(QString("Service Worker API not supported. Got: %1. Details: %2")
                          .arg(result, details)));
  qDebug() << "Service Worker API is supported";
}

void TestServiceWorker::testServiceWorkerRegistration() {
  // Test service worker registration with data: URL scheme
  // Note: This tests the API but won't actually register because data: URLs
  // are not secure contexts. For actual registration, see testServiceWorkerWithHttpsServer
  QSignalSpy spy(m_page, &QWebEnginePage::loadFinished);

  QString html = R"(
    <!DOCTYPE html>
    <html>
    <head><title>Service Worker Registration Test</title></head>
    <body>
      <div id="result">Testing...</div>
      <div id="error"></div>
      <script>
        var result = document.getElementById('result');
        var error = document.getElementById('error');

        if (!('serviceWorker' in navigator)) {
          result.textContent = 'SW_NOT_AVAILABLE';
        } else {
          // Try to register (will fail with data: URL but tests API)
          const swCode = `
            self.addEventListener('install', (event) => {
              console.log('[SW] Install event');
              self.skipWaiting();
            });
          `;
          const blob = new Blob([swCode], { type: 'application/javascript' });
          const swUrl = URL.createObjectURL(blob);

          navigator.serviceWorker.register(swUrl)
            .then(registration => {
              result.textContent = 'SW_REGISTERED';
            })
            .catch(err => {
              // Expected to fail with blob: or data: URL
              error.textContent = err.toString();
              if (err.toString().includes('not supported') ||
                  err.toString().includes('blob:') ||
                  err.toString().includes('null')) {
                result.textContent = 'SW_API_WORKING_NEEDS_HTTPS';
              } else {
                result.textContent = 'SW_REGISTRATION_ERROR';
              }
            });
        }
      </script>
    </body>
    </html>
  )";

  m_page->setHtml(html, QUrl("http://localhost/"));
  QVERIFY(spy.wait(10000));

  // Wait for the async registration attempt
  QString result;
  QString errorMsg;
  bool finished = false;

  auto checkResult = [this, &result, &errorMsg, &finished]() {
    m_page->runJavaScript(
        "document.getElementById('result').textContent",
        [&result, &finished](const QVariant &v) {
          QString text = v.toString();
          if (text != "Testing...") {
            result = text;
            finished = true;
          }
        });
  };

  // Poll for result
  for (int i = 0; i < 50 && !finished; ++i) {
    checkResult();
    QTest::qWait(200);
  }

  QVERIFY2(finished, "Service Worker registration check timed out");

  // Get error message
  finished = false;
  m_page->runJavaScript(
      "document.getElementById('error').textContent",
      [&errorMsg, &finished](const QVariant &v) {
        errorMsg = v.toString();
        finished = true;
      });
  QTRY_VERIFY_WITH_TIMEOUT(finished, 5000);

  qDebug() << "Service Worker registration result:" << result;
  if (!errorMsg.isEmpty()) {
    qDebug() << "Error message:" << errorMsg;
  }

  // We expect either successful registration or a protocol error
  QVERIFY2(result == "SW_API_WORKING_NEEDS_HTTPS" || result == "SW_REGISTERED",
           qPrintable(QString("Unexpected result: %1, error: %2").arg(result, errorMsg)));
}

void TestServiceWorker::testServiceWorkerWithHttpsServer() {
  // Service workers work on localhost HTTP without needing HTTPS
  // This is a special exception browsers make for development

  QString scriptDir = QCoreApplication::applicationDirPath() + "/../tests/resources";
  QString swFile = scriptDir + "/sw.js";

  // Check if sw.js exists
  if (!QFile::exists(swFile)) {
    QSKIP("sw.js not found in tests/resources");
    return;
  }

  // Start HTTP server (localhost HTTP is allowed for service workers)
  m_httpsPort = 18766; // Reuse variable but for HTTP
  m_httpsServer = new QProcess(this);

  // Create simple Python HTTP server script
  QString serverScript = QString(R"(
import http.server
import sys
import os

port = %1
serve_dir = '%2'

os.chdir(serve_dir)

class ServiceWorkerHTTPRequestHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        if self.path.endswith('.js'):
            self.send_header('Content-Type', 'application/javascript; charset=utf-8')
            self.send_header('Service-Worker-Allowed', '/')
        super().end_headers()

httpd = http.server.HTTPServer(('localhost', port), ServiceWorkerHTTPRequestHandler)
print(f"HTTP server ready on port {port}", flush=True)
httpd.serve_forever()
)")
                             .arg(m_httpsPort)
                             .arg(scriptDir);

  m_tempDir = new QTemporaryDir();
  QString scriptPath = m_tempDir->path() + "/https_server.py";
  QFile scriptFile(scriptPath);
  if (!scriptFile.open(QIODevice::WriteOnly)) {
    QSKIP("Could not create temporary HTTPS server script");
    return;
  }
  scriptFile.write(serverScript.toUtf8());
  scriptFile.close();

  m_httpsServer->start("python3", QStringList() << scriptPath);

  if (!m_httpsServer->waitForStarted(5000)) {
    QSKIP("Could not start HTTPS server - python3 required");
    return;
  }

  // Wait for server to be ready
  QTest::qWait(2000);

  // Create a test HTML file in the temp directory
  QString testHtml = QString(R"(
    <!DOCTYPE html>
    <html>
    <head><title>Service Worker HTTPS Test</title></head>
    <body>
      <div id="result">Testing...</div>
      <div id="error"></div>
      <div id="scope"></div>
      <script>
        var result = document.getElementById('result');
        var error = document.getElementById('error');
        var scope = document.getElementById('scope');

        // Wait for page to be fully loaded before registering service worker
        window.addEventListener('load', function() {
          if (!('serviceWorker' in navigator)) {
            result.textContent = 'SW_NOT_AVAILABLE';
          } else {
            navigator.serviceWorker.register('/sw.js', { scope: '/' })
              .then(registration => {
                scope.textContent = 'Scope: ' + registration.scope;
                return navigator.serviceWorker.ready;
              })
              .then(() => {
                result.textContent = 'SW_REGISTERED_ACTIVE';
              })
              .catch(err => {
                error.textContent = err.toString();
                result.textContent = 'SW_REGISTRATION_FAILED';
              });
          }
        });
      </script>
    </body>
    </html>
  )");

  // Write test HTML file to disk so it can be served by HTTPS server
  QString testHtmlPath = scriptDir + "/test_sw.html";
  QFile htmlFile(testHtmlPath);
  if (!htmlFile.open(QIODevice::WriteOnly)) {
    QSKIP("Could not create test HTML file");
    return;
  }
  htmlFile.write(testHtml.toUtf8());
  htmlFile.close();

  // Load the page from the actual HTTP server (not setHtml)
  QSignalSpy spy(m_page, &QWebEnginePage::loadFinished);
  QString testUrl = QString("http://localhost:%1/test_sw.html").arg(m_httpsPort);
  m_page->load(QUrl(testUrl));
  QVERIFY(spy.wait(10000));

  // Wait for service worker registration
  QString result;
  QString errorMsg;
  QString scopeInfo;
  bool finished = false;

  auto checkResult = [this, &result, &finished]() {
    m_page->runJavaScript(
        "document.getElementById('result').textContent",
        [&result, &finished](const QVariant &v) {
          QString text = v.toString();
          if (text != "Testing...") {
            result = text;
            finished = true;
          }
        });
  };

  // Poll for result (service worker registration is async)
  for (int i = 0; i < 100 && !finished; ++i) {
    checkResult();
    QTest::qWait(200);
  }

  QVERIFY2(finished, "Service Worker HTTPS registration check timed out");

  // Get error and scope info
  finished = false;
  m_page->runJavaScript(
      "document.getElementById('error').textContent",
      [&errorMsg, &finished](const QVariant &v) {
        errorMsg = v.toString();
        finished = true;
      });
  QTRY_VERIFY_WITH_TIMEOUT(finished, 5000);

  finished = false;
  m_page->runJavaScript(
      "document.getElementById('scope').textContent",
      [&scopeInfo, &finished](const QVariant &v) {
        scopeInfo = v.toString();
        finished = true;
      });
  QTRY_VERIFY_WITH_TIMEOUT(finished, 5000);

  qDebug() << "Service Worker HTTPS result:" << result;
  if (!errorMsg.isEmpty()) {
    qDebug() << "Error:" << errorMsg;
  }
  if (!scopeInfo.isEmpty()) {
    qDebug() << "Scope:" << scopeInfo;
  }

  if (result != "SW_REGISTERED_ACTIVE") {
    qWarning() << "Service Worker registration failed with HTTPS server";
    qWarning() << "This may indicate:";
    qWarning() << "  - Certificate validation issues";
    qWarning() << "  - Service Worker script fetch errors";
    qWarning() << "  - MIME type issues";
    qWarning() << "Result:" << result;
    qWarning() << "Error:" << errorMsg;
  }

  QVERIFY2(result == "SW_REGISTERED_ACTIVE",
           qPrintable(QString("Service Worker registration failed: %1, error: %2")
                          .arg(result, errorMsg)));

  qDebug() << "Service Worker successfully registered and activated via HTTPS!";
}

QTEST_MAIN(TestServiceWorker)
#include "tst_serviceworker.moc"
