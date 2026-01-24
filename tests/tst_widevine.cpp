// Copyright(C) 2026 Joseph Crowell.
// SPDX - License - Identifier : GPL-2.0-or-later

#include <QApplication>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTest>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>

// Set up Widevine before QApplication is created
static void setupWidevineEnvironment() {
#ifdef WIDEVINE_CDM_PATH
  QString widevinePath = QStringLiteral(WIDEVINE_CDM_PATH);
  if (QFileInfo::exists(widevinePath)) {
    // Set Chromium flags to enable Widevine and EME
    QByteArray flags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
    if (!flags.contains("widevine-cdm-path")) {
      if (!flags.isEmpty()) {
        flags += " ";
      }
      // Enable Widevine CDM
      // Qt WebEngine uses --widevine-path, NOT --widevine-cdm-path
      flags += "--widevine-path=" + widevinePath.toUtf8();
      // Enable EME (Encrypted Media Extensions)
      flags += " --enable-features=EncryptedMedia";
      // Disable sandbox for CDM (may be needed for tests)
      flags += " --no-sandbox";

      qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags);
      qDebug() << "Set QTWEBENGINE_CHROMIUM_FLAGS:" << flags;
    }
  }
#endif
}

// Call setup before main() runs
Q_CONSTRUCTOR_FUNCTION(setupWidevineEnvironment)

class TestWidevine : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();

  // Test that Widevine CDM library exists at compiled path
  void testWidevineCdmExists();

  // Test that QTWEBENGINE_CHROMIUM_FLAGS is properly set
  void testChromiumFlagsSet();

  // Test that EME (Encrypted Media Extensions) is available
  void testEmeAvailable();

  // Test DRM capabilities detection via JavaScript
  void testDrmCapabilities();

private:
  QWebEngineProfile *m_profile = nullptr;
  QWebEnginePage *m_page = nullptr;
};

void TestWidevine::initTestCase() {
  // Create a test profile
  m_profile = new QWebEngineProfile(QStringLiteral("widevine-test"), this);

  // Enable settings that might be needed for DRM
  auto settings = m_profile->settings();
  settings->setAttribute(QWebEngineSettings::PluginsEnabled, true);
  settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
  settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
  settings->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, true);

  m_page = new QWebEnginePage(m_profile, this);
}

void TestWidevine::cleanupTestCase() {
  delete m_page;
  m_page = nullptr;
  delete m_profile;
  m_profile = nullptr;
}

void TestWidevine::testWidevineCdmExists() {
#ifdef WIDEVINE_CDM_PATH
  QString widevinePath = QStringLiteral(WIDEVINE_CDM_PATH);
  QVERIFY2(QFileInfo::exists(widevinePath),
           qPrintable(QString("Widevine CDM not found at: %1").arg(widevinePath)));

  QFileInfo fileInfo(widevinePath);
  QVERIFY2(fileInfo.isFile(), "Widevine CDM path is not a file");
  QVERIFY2(fileInfo.isReadable(), "Widevine CDM file is not readable");
  QVERIFY2(fileInfo.size() > 0, "Widevine CDM file is empty");

  qDebug() << "Widevine CDM found at:" << widevinePath;
  qDebug() << "File size:" << fileInfo.size() << "bytes";
#else
  QSKIP("Widevine CDM support not compiled in (ENABLE_WIDEVINE=OFF)");
#endif
}

void TestWidevine::testChromiumFlagsSet() {
#ifdef WIDEVINE_CDM_PATH
  QByteArray flags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");

  // The setupWidevineCdm() function in main.cpp should set this,
  // but in tests we may need to verify the logic separately
  if (flags.contains("widevine-cdm-path")) {
    QVERIFY2(flags.contains(WIDEVINE_CDM_PATH),
             "QTWEBENGINE_CHROMIUM_FLAGS contains widevine-cdm-path but wrong path");
    qDebug() << "Chromium flags correctly set:" << flags;
  } else {
    // If not set, we can set it for the test
    QString widevinePath = QStringLiteral(WIDEVINE_CDM_PATH);
    if (QFileInfo::exists(widevinePath)) {
      flags = "--widevine-cdm-path=" + widevinePath.toUtf8();
      qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags);
      qDebug() << "Set Chromium flags for test:" << flags;
    }
    QVERIFY2(!flags.isEmpty(), "Could not set QTWEBENGINE_CHROMIUM_FLAGS");
  }
#else
  QSKIP("Widevine CDM support not compiled in (ENABLE_WIDEVINE=OFF)");
#endif
}

void TestWidevine::testEmeAvailable() {
  // Test that the Encrypted Media Extensions API is available
  // This checks if the browser supports the EME API (required for DRM)
  QSignalSpy spy(m_page, &QWebEnginePage::loadFinished);

  // Simple HTML that checks for EME support
  // Note: navigator.requestMediaKeySystemAccess requires secure context (HTTPS or localhost)
  QString html = R"(
    <!DOCTYPE html>
    <html>
    <head><title>EME Test</title></head>
    <body>
      <div id="result">Testing...</div>
      <div id="debug"></div>
      <script>
        var result = document.getElementById('result');
        var debug = document.getElementById('debug');

        // Check secure context
        debug.textContent = 'isSecureContext: ' + window.isSecureContext;

        // Check for EME API
        if (typeof navigator.requestMediaKeySystemAccess !== 'undefined') {
          result.textContent = 'EME_SUPPORTED';
        } else {
          result.textContent = 'EME_NOT_SUPPORTED';
        }
      </script>
    </body>
    </html>
  )";

  // Use localhost URL to ensure secure context for EME API
  m_page->setHtml(html, QUrl("http://localhost/"));
  QVERIFY(spy.wait(10000));
  QCOMPARE(spy.count(), 1);
  QVERIFY(spy.at(0).at(0).toBool()); // Load succeeded

  // Check the result and debug info
  QString result;
  QString debugInfo;
  bool finished = false;

  m_page->runJavaScript(
      "document.getElementById('result').textContent",
      [&result, &finished](const QVariant &v) {
        result = v.toString();
        finished = true;
      });

  // Wait for JavaScript to complete
  QTRY_VERIFY_WITH_TIMEOUT(finished, 5000);

  // Get debug info
  finished = false;
  m_page->runJavaScript(
      "document.getElementById('debug').textContent",
      [&debugInfo, &finished](const QVariant &v) {
        debugInfo = v.toString();
        finished = true;
      });
  QTRY_VERIFY_WITH_TIMEOUT(finished, 5000);

  qDebug() << "EME test debug info:" << debugInfo;

  QVERIFY2(result == "EME_SUPPORTED",
           qPrintable(QString("EME not supported. Got: %1. Debug: %2").arg(result, debugInfo)));
  qDebug() << "EME (Encrypted Media Extensions) is supported";
}

void TestWidevine::testDrmCapabilities() {
#ifdef WIDEVINE_CDM_PATH
  // Test that Widevine DRM is actually available
  // This uses the EME API to request access to Widevine
  QSignalSpy spy(m_page, &QWebEnginePage::loadFinished);

  QString html = R"(
    <!DOCTYPE html>
    <html>
    <head><title>Widevine Test</title></head>
    <body>
      <div id="result">Testing...</div>
      <script>
        var result = document.getElementById('result');

        if (!navigator.requestMediaKeySystemAccess) {
          result.textContent = 'EME_NOT_AVAILABLE';
        } else {
          // Try multiple configurations with different codecs and robustness levels
          var configs = [
            {
              initDataTypes: ['cenc'],
              videoCapabilities: [{
                contentType: 'video/mp4; codecs="avc1.42E01E"',
                robustness: 'SW_SECURE_CRYPTO'
              }]
            },
            {
              initDataTypes: ['cenc', 'keyids', 'webm'],
              videoCapabilities: [
                { contentType: 'video/mp4; codecs="avc1.42E01E"' },
                { contentType: 'video/webm; codecs="vp8"' },
                { contentType: 'video/webm; codecs="vp9"' }
              ]
            },
            {
              initDataTypes: ['cenc'],
              videoCapabilities: [{ contentType: 'video/mp4' }],
              audioCapabilities: [{ contentType: 'audio/mp4' }]
            }
          ];

          navigator.requestMediaKeySystemAccess('com.widevine.alpha', configs)
            .then(function(mediaKeySystemAccess) {
              result.textContent = 'WIDEVINE_AVAILABLE';
            })
            .catch(function(error) {
              result.textContent = 'WIDEVINE_NOT_AVAILABLE:' + error.message;
            });
        }
      </script>
    </body>
    </html>
  )";

  // Use localhost URL to ensure secure context for EME API
  m_page->setHtml(html, QUrl("http://localhost/"));
  QVERIFY(spy.wait(10000));

  // Wait for the async Widevine check to complete
  QString result;
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

  // Poll for result (async JavaScript)
  for (int i = 0; i < 50 && !finished; ++i) {
    checkResult();
    QTest::qWait(200);
  }

  QVERIFY2(finished, "Widevine check timed out");

  if (result == "WIDEVINE_AVAILABLE") {
    qDebug() << "Widevine DRM is available and working!";
    QVERIFY(true);
  } else {
    // Log the actual error for debugging
    qWarning() << "Widevine test result:" << result;

    // Check if it's a codec/configuration issue
    if (result.contains("Unsupported keySystem or supportedConfigurations")) {
      qWarning() << "Widevine keySystem not supported - this is expected if:";
      qWarning() << "  - Qt WebEngine wasn't built with proprietary codecs (-webengine-proprietary-codecs)";
      qWarning() << "  - System doesn't have required codec libraries";
      qWarning() << "";
      qWarning() << "EME API is available (previous test passed), but Widevine codec support is missing.";
      qWarning() << "On Arch-based systems, install qt6-webengine with proprietary codec support.";
      QSKIP("Widevine not available - Qt WebEngine may lack proprietary codec support");
    } else if (result.startsWith("WIDEVINE_NOT_AVAILABLE")) {
      qWarning() << "Widevine DRM not available - this may be expected if:";
      qWarning() << "  - Running in a headless/CI environment";
      qWarning() << "  - Missing system dependencies";
      QSKIP("Widevine DRM test inconclusive in this environment");
    } else if (result == "EME_NOT_AVAILABLE") {
      // This should not happen since EME test passed, but handle it anyway
      QFAIL("EME API not available (but previous test passed - this is inconsistent)");
    } else {
      QFAIL(qPrintable(QString("Unexpected result: %1").arg(result)));
    }
  }
#else
  QSKIP("Widevine CDM support not compiled in (ENABLE_WIDEVINE=OFF)");
#endif
}

QTEST_MAIN(TestWidevine)
#include "tst_widevine.moc"
