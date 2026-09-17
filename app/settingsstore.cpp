// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "settingsstore.h"

#include <QDir>
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
#include <QWidget>
#include <QWebEnginePermission>
#include <limits>

QString SettingsStore::permissionKey(const QUrl &origin, int permissionType) const {
  const QUrl normalized = origin.adjusted(QUrl::RemovePath | QUrl::RemoveQuery |
                                          QUrl::RemoveFragment);
  const QByteArray encoded = normalized.toEncoded().toBase64(
      QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
  return QStringLiteral("Permissions/v2/grants/%1/%2")
      .arg(QString::fromLatin1(encoded))
      .arg(permissionType);
}

QString SettingsStore::legacyPermissionKey(const QUrl &origin,
                                            int permissionType) const {
  QString host = origin.host();
  host.replace(QLatin1Char('.'), QLatin1Char('_'));
  return QStringLiteral("Permissions/grants/%1/%2").arg(host).arg(permissionType);
}

QString SettingsStore::protocolHandlerKey(const QUrl &origin,
                                          const QString &scheme) const {
  const QUrl normalized = origin.adjusted(QUrl::RemovePath | QUrl::RemoveQuery |
                                          QUrl::RemoveFragment);
  const QByteArray encodedOrigin = normalized.toEncoded().toBase64(
      QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
  const QByteArray encodedScheme = scheme.toUtf8().toBase64(
      QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
  return QStringLiteral("Permissions/v2/protocolHandlers/%1/%2")
      .arg(QString::fromLatin1(encodedOrigin), QString::fromLatin1(encodedScheme));
}

QString SettingsStore::legacyProtocolHandlerKey(const QUrl &origin,
                                                const QString &scheme) const {
  return QStringLiteral("Permissions/ProtocolHandlers/%1/%2")
      .arg(origin.host(), scheme);
}

SettingsStore::SettingsStore(const QString &profilePath, QObject *parent)
    : QObject(parent),
      m_settings(profilePath + QDir::separator() + QStringLiteral("settings.ini"),
                 QSettings::IniFormat) {
  m_settings.beginGroup(QStringLiteral("Behavior"));
  m_hideOnMinimize = m_settings.value(QStringLiteral("hideOnMinimize"), false).toBool();
  m_hideOnClose = m_settings.value(QStringLiteral("hideOnClose"), true).toBool();
  m_settings.endGroup();

  m_settings.beginGroup(QStringLiteral("QuickWindow/v2"));
  const bool hasV2 = m_settings.contains(QStringLiteral("width"));
  if (hasV2) {
    m_windowWidth = qMax(320, m_settings.value(QStringLiteral("width"), 967).toInt());
    m_windowHeight = qMax(240, m_settings.value(QStringLiteral("height"), 557).toInt());
    m_windowMaximized = m_settings.value(QStringLiteral("maximized"), false).toBool();
    m_hasWindowPosition = m_settings.value(QStringLiteral("hasPosition"), false).toBool();
    m_windowOffsetX = m_settings.value(QStringLiteral("offsetX"), 0).toInt();
    m_windowOffsetY = m_settings.value(QStringLiteral("offsetY"), 0).toInt();
    m_windowScreenId = m_settings.value(QStringLiteral("screenId")).toString();
    m_windowScreenName = m_settings.value(QStringLiteral("screenName")).toString();
    m_windowScreenGeometry = QRect(
        m_settings.value(QStringLiteral("screenX"), 0).toInt(),
        m_settings.value(QStringLiteral("screenY"), 0).toInt(),
        m_settings.value(QStringLiteral("screenWidth"), 0).toInt(),
        m_settings.value(QStringLiteral("screenHeight"), 0).toInt());
  }
  m_settings.endGroup();
  if (!hasV2) {
    m_settings.beginGroup(QStringLiteral("QuickWindow/v1"));
    m_windowWidth = qMax(320, m_settings.value(QStringLiteral("width"), 967).toInt());
    m_windowHeight = qMax(240, m_settings.value(QStringLiteral("height"), 557).toInt());
    m_windowMaximized = m_settings.value(QStringLiteral("maximized"), false).toBool();
    m_settings.endGroup();
  }
  if (!m_hasWindowPosition) {
    const QByteArray legacyGeometry =
        m_settings.value(QStringLiteral("BrowserWindow/geometry")).toByteArray();
    if (!legacyGeometry.isEmpty()) {
      QWidget legacyWindow;
      if (legacyWindow.restoreGeometry(legacyGeometry)) {
        const QRect geometry = legacyWindow.normalGeometry().isValid()
                                   ? legacyWindow.normalGeometry()
                                   : legacyWindow.geometry();
        QScreen *screen = legacyWindow.screen();
        if (screen && geometry.isValid()) {
          const QRect available = screen->availableGeometry();
          m_windowWidth = qMax(320, geometry.width());
          m_windowHeight = qMax(240, geometry.height());
          m_windowOffsetX = geometry.x() - available.x();
          m_windowOffsetY = geometry.y() - available.y();
          m_windowScreenId = screenIdentifier(screen);
          m_windowScreenName = screen->name();
          m_windowScreenGeometry = screen->geometry();
          m_hasWindowPosition = true;
          m_windowMaximized = m_windowMaximized || legacyWindow.isMaximized();
          m_settings.setValue(QStringLiteral("QuickWindow/v2/width"), m_windowWidth);
          m_settings.setValue(QStringLiteral("QuickWindow/v2/height"), m_windowHeight);
          m_settings.setValue(QStringLiteral("QuickWindow/v2/maximized"),
                              m_windowMaximized);
          m_settings.setValue(QStringLiteral("QuickWindow/v2/offsetX"), m_windowOffsetX);
          m_settings.setValue(QStringLiteral("QuickWindow/v2/offsetY"), m_windowOffsetY);
          m_settings.setValue(QStringLiteral("QuickWindow/v2/screenId"), m_windowScreenId);
          m_settings.setValue(QStringLiteral("QuickWindow/v2/screenName"), m_windowScreenName);
          m_settings.setValue(QStringLiteral("QuickWindow/v2/screenX"),
                              m_windowScreenGeometry.x());
          m_settings.setValue(QStringLiteral("QuickWindow/v2/screenY"),
                              m_windowScreenGeometry.y());
          m_settings.setValue(QStringLiteral("QuickWindow/v2/screenWidth"),
                              m_windowScreenGeometry.width());
          m_settings.setValue(QStringLiteral("QuickWindow/v2/screenHeight"),
                              m_windowScreenGeometry.height());
          m_settings.setValue(QStringLiteral("QuickWindow/v2/hasPosition"), true);
          m_settings.sync();
        }
      }
    }
  }
}

SettingsStore::~SettingsStore() { sync(); }

void SettingsStore::setHideOnMinimize(bool value) {
  if (m_hideOnMinimize == value)
    return;
  m_hideOnMinimize = value;
  m_settings.setValue(QStringLiteral("Behavior/hideOnMinimize"), value);
  emit hideOnMinimizeChanged();
}

void SettingsStore::setHideOnClose(bool value) {
  if (m_hideOnClose == value)
    return;
  m_hideOnClose = value;
  m_settings.setValue(QStringLiteral("Behavior/hideOnClose"), value);
  emit hideOnCloseChanged();
}

void SettingsStore::setWindowWidth(int value) {
  value = qMax(320, value);
  if (m_windowWidth == value)
    return;
  m_windowWidth = value;
  m_settings.setValue(QStringLiteral("QuickWindow/v2/width"), value);
  emit windowGeometryChanged();
}

void SettingsStore::setWindowHeight(int value) {
  value = qMax(240, value);
  if (m_windowHeight == value)
    return;
  m_windowHeight = value;
  m_settings.setValue(QStringLiteral("QuickWindow/v2/height"), value);
  emit windowGeometryChanged();
}

void SettingsStore::setWindowMaximized(bool value) {
  if (m_windowMaximized == value)
    return;
  m_windowMaximized = value;
  m_settings.setValue(QStringLiteral("QuickWindow/v2/maximized"), value);
  emit windowGeometryChanged();
}

bool SettingsStore::sync() {
  m_settings.sync();
  if (m_settings.status() == QSettings::NoError)
    return true;
  emit persistenceError(QStringLiteral("Could not persist application settings"));
  return false;
}

bool SettingsStore::hasPermissionGrant(const QUrl &origin, int permissionType) {
  if (!origin.isValid() || origin.scheme().isEmpty() || origin.host().isEmpty())
    return false;
  const QString key = permissionKey(origin, permissionType);
  if (m_settings.value(key, false).toBool())
    return true;
  const QString legacyKey = legacyPermissionKey(origin, permissionType);
  if (!m_settings.value(legacyKey, false).toBool())
    return false;
  m_settings.setValue(key, true);
  sync();
  return true;
}

bool SettingsStore::rememberPermissionGrant(const QUrl &origin,
                                            int permissionType) {
  if (!origin.isValid() || origin.scheme().isEmpty() || origin.host().isEmpty())
    return false;
  m_settings.setValue(permissionKey(origin, permissionType), true);
  return sync();
}

int SettingsStore::protocolHandlerDecision(const QUrl &origin,
                                           const QString &scheme) {
  if (!origin.isValid() || origin.scheme().isEmpty() || origin.host().isEmpty() ||
      scheme.isEmpty())
    return -1;
  const QString key = protocolHandlerKey(origin, scheme);
  if (m_settings.contains(key))
    return m_settings.value(key).toBool() ? 1 : 0;
  const QString legacyKey = legacyProtocolHandlerKey(origin, scheme);
  if (!m_settings.contains(legacyKey))
    return -1;
  const bool accepted = m_settings.value(legacyKey).toBool();
  m_settings.setValue(key, accepted);
  sync();
  return accepted ? 1 : 0;
}

bool SettingsStore::rememberProtocolHandlerDecision(const QUrl &origin,
                                                    const QString &scheme,
                                                    bool accepted) {
  if (!origin.isValid() || origin.scheme().isEmpty() || origin.host().isEmpty() ||
      scheme.isEmpty())
    return false;
  m_settings.setValue(protocolHandlerKey(origin, scheme), accepted);
  return sync();
}

void SettingsStore::configureInitialOrigin(const QUrl &origin) {
  const bool combined = hasPermissionGrant(
      origin, int(QWebEnginePermission::PermissionType::MediaAudioVideoCapture));
  const bool audio = combined || hasPermissionGrant(
      origin, int(QWebEnginePermission::PermissionType::MediaAudioCapture));
  const bool video = combined || hasPermissionGrant(
      origin, int(QWebEnginePermission::PermissionType::MediaVideoCapture));
  if (!audio && !video) {
    m_mediaPermissionBootstrapScript.clear();
    return;
  }
  m_mediaPermissionBootstrapScript = QStringLiteral(R"JS(
(() => {
  if (!navigator.mediaDevices || navigator.mediaDevices.__webAppContainerReady)
    return;
  const devices = navigator.mediaDevices;
  const originalEnumerate = devices.enumerateDevices.bind(devices);
  const originalGetUserMedia = devices.getUserMedia.bind(devices);
  const requested = {audio: %1, video: %2};
  let readiness = originalGetUserMedia(requested)
    .then(stream => { stream.getTracks().forEach(track => track.stop()); })
    .catch(async () => {
      if (requested.audio && requested.video) {
        const stream = await originalGetUserMedia({audio: true});
        stream.getTracks().forEach(track => track.stop());
      }
    })
    .catch(() => {});
  devices.enumerateDevices = async function() {
    await readiness;
    return originalEnumerate();
  };
  Object.defineProperty(devices, "__webAppContainerReady", {value: true});
})();
)JS").arg(audio ? QStringLiteral("true") : QStringLiteral("false"),
            video ? QStringLiteral("true") : QStringLiteral("false"));
}

QString SettingsStore::screenIdentifier(const QScreen *screen) {
  if (!screen)
    return {};
  if (!screen->serialNumber().isEmpty())
    return QStringLiteral("serial:%1").arg(screen->serialNumber());
  return QStringLiteral("display:%1|%2|%3")
      .arg(screen->name(), screen->manufacturer(), screen->model());
}

QRect SettingsStore::clampGeometry(const QRect &geometry,
                                   const QRect &availableGeometry) {
  if (!availableGeometry.isValid())
    return geometry;
  const int width = qMin(qMax(320, geometry.width()), availableGeometry.width());
  const int height = qMin(qMax(240, geometry.height()), availableGeometry.height());
  const int maximumX = availableGeometry.right() - width + 1;
  const int maximumY = availableGeometry.bottom() - height + 1;
  const int x = qBound(availableGeometry.left(), geometry.x(), maximumX);
  const int y = qBound(availableGeometry.top(), geometry.y(), maximumY);
  return QRect(x, y, width, height);
}

qsizetype SettingsStore::chooseScreen(
    const QString &savedId, const QString &savedName, const QRect &savedGeometry,
    const QList<ScreenDescription> &screens) {
  const auto chooseMatching = [&](bool matchId) {
    qsizetype best = qsizetype(-1);
    qint64 bestDistance = std::numeric_limits<qint64>::max();
    for (qsizetype index = 0; index < screens.size(); ++index) {
      const ScreenDescription &screen = screens.at(index);
      const bool matches = matchId ? (!savedId.isEmpty() && screen.id == savedId)
                                   : (!savedName.isEmpty() && screen.name == savedName);
      if (!matches)
        continue;
      const qint64 distance = savedGeometry.isValid()
                                  ? qAbs(qint64(screen.geometry.x()) - savedGeometry.x()) +
                                        qAbs(qint64(screen.geometry.y()) - savedGeometry.y()) +
                                        qAbs(qint64(screen.geometry.width()) - savedGeometry.width()) +
                                        qAbs(qint64(screen.geometry.height()) - savedGeometry.height())
                                  : 0;
      if (best < 0 || distance < bestDistance) {
        best = index;
        bestDistance = distance;
      }
    }
    return best;
  };
  const qsizetype byId = chooseMatching(true);
  return byId >= 0 ? byId : chooseMatching(false);
}

void SettingsStore::restoreWindowGeometry(QWindow *window) const {
  if (!window)
    return;
  const QList<QScreen *> applicationScreens = QGuiApplication::screens();
  QList<ScreenDescription> descriptions;
  descriptions.reserve(applicationScreens.size());
  for (QScreen *screen : applicationScreens)
    descriptions.append({screenIdentifier(screen), screen->name(), screen->geometry()});
  const qsizetype screenIndex = chooseScreen(
      m_windowScreenId, m_windowScreenName, m_windowScreenGeometry, descriptions);
  QScreen *target = screenIndex >= 0 ? applicationScreens.at(screenIndex) : nullptr;
  if (!target)
    target = QGuiApplication::primaryScreen();
  if (!target) {
    window->resize(m_windowWidth, m_windowHeight);
    return;
  }
  window->setScreen(target);
  if (!m_hasWindowPosition) {
    window->resize(m_windowWidth, m_windowHeight);
    return;
  }
  const QRect available = target->availableGeometry();
  const QRect requested(available.x() + m_windowOffsetX,
                        available.y() + m_windowOffsetY,
                        m_windowWidth, m_windowHeight);
  window->setGeometry(clampGeometry(requested, available));
}

void SettingsStore::saveWindowGeometry(QWindow *window) {
  if (!window || !window->screen())
    return;
  const QRect geometry = window->geometry();
  const QRect available = window->screen()->availableGeometry();
  m_windowWidth = qMax(320, geometry.width());
  m_windowHeight = qMax(240, geometry.height());
  m_windowOffsetX = geometry.x() - available.x();
  m_windowOffsetY = geometry.y() - available.y();
  m_windowScreenId = screenIdentifier(window->screen());
  m_windowScreenName = window->screen()->name();
  m_windowScreenGeometry = window->screen()->geometry();
  m_hasWindowPosition = true;
  m_settings.setValue(QStringLiteral("QuickWindow/v2/width"), m_windowWidth);
  m_settings.setValue(QStringLiteral("QuickWindow/v2/height"), m_windowHeight);
  m_settings.setValue(QStringLiteral("QuickWindow/v2/offsetX"), m_windowOffsetX);
  m_settings.setValue(QStringLiteral("QuickWindow/v2/offsetY"), m_windowOffsetY);
  m_settings.setValue(QStringLiteral("QuickWindow/v2/screenId"), m_windowScreenId);
  m_settings.setValue(QStringLiteral("QuickWindow/v2/screenName"), m_windowScreenName);
  m_settings.setValue(QStringLiteral("QuickWindow/v2/screenX"), m_windowScreenGeometry.x());
  m_settings.setValue(QStringLiteral("QuickWindow/v2/screenY"), m_windowScreenGeometry.y());
  m_settings.setValue(QStringLiteral("QuickWindow/v2/screenWidth"), m_windowScreenGeometry.width());
  m_settings.setValue(QStringLiteral("QuickWindow/v2/screenHeight"), m_windowScreenGeometry.height());
  m_settings.setValue(QStringLiteral("QuickWindow/v2/hasPosition"), true);
  emit windowGeometryChanged();
}

void SettingsStore::bindWindow(QWindow *window) {
  if (!window)
    return;
  const auto captureNormalGeometry = [this, window] {
    if (window->visibility() != QWindow::Windowed || !window->screen())
      return;
    saveWindowGeometry(window);
  };
  connect(window, &QWindow::xChanged, this, captureNormalGeometry);
  connect(window, &QWindow::yChanged, this, captureNormalGeometry);
  connect(window, &QWindow::widthChanged, this, captureNormalGeometry);
  connect(window, &QWindow::heightChanged, this, captureNormalGeometry);
  connect(window, &QWindow::screenChanged, this, [this, window, captureNormalGeometry](QScreen *screen) {
    if (screen) {
      m_windowScreenId = screenIdentifier(screen);
      m_windowScreenName = screen->name();
      m_windowScreenGeometry = screen->geometry();
      m_settings.setValue(QStringLiteral("QuickWindow/v2/screenId"), m_windowScreenId);
      m_settings.setValue(QStringLiteral("QuickWindow/v2/screenName"), m_windowScreenName);
      m_settings.setValue(QStringLiteral("QuickWindow/v2/screenX"), m_windowScreenGeometry.x());
      m_settings.setValue(QStringLiteral("QuickWindow/v2/screenY"), m_windowScreenGeometry.y());
      m_settings.setValue(QStringLiteral("QuickWindow/v2/screenWidth"), m_windowScreenGeometry.width());
      m_settings.setValue(QStringLiteral("QuickWindow/v2/screenHeight"), m_windowScreenGeometry.height());
    }
    captureNormalGeometry();
  });
  connect(window, &QWindow::visibilityChanged, this,
          [this, captureNormalGeometry](QWindow::Visibility visibility) {
            if (visibility == QWindow::Maximized)
              setWindowMaximized(true);
            else if (visibility == QWindow::Windowed) {
              setWindowMaximized(false);
              captureNormalGeometry();
            }
          });
}
