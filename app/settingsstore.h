// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QSettings>
#include <QUrl>
#include <QRect>

class QScreen;
class QWindow;

class SettingsStore final : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("SettingsStore is created by the application")
  Q_PROPERTY(bool hideOnMinimize READ hideOnMinimize WRITE setHideOnMinimize
                 NOTIFY hideOnMinimizeChanged)
  Q_PROPERTY(bool hideOnClose READ hideOnClose WRITE setHideOnClose
                 NOTIFY hideOnCloseChanged)
  Q_PROPERTY(int windowWidth READ windowWidth WRITE setWindowWidth NOTIFY windowGeometryChanged)
  Q_PROPERTY(int windowHeight READ windowHeight WRITE setWindowHeight NOTIFY windowGeometryChanged)
  Q_PROPERTY(bool windowMaximized READ windowMaximized WRITE setWindowMaximized
                 NOTIFY windowGeometryChanged)

public:
  struct ScreenDescription {
    QString id;
    QString name;
    QRect geometry;
  };
  explicit SettingsStore(const QString &profilePath, QObject *parent = nullptr);
  ~SettingsStore() override;

  bool hideOnMinimize() const { return m_hideOnMinimize; }
  bool hideOnClose() const { return m_hideOnClose; }
  int windowWidth() const { return m_windowWidth; }
  int windowHeight() const { return m_windowHeight; }
  bool windowMaximized() const { return m_windowMaximized; }
  bool hasWindowPosition() const { return m_hasWindowPosition; }
  int windowOffsetX() const { return m_windowOffsetX; }
  int windowOffsetY() const { return m_windowOffsetY; }
  QString windowScreenId() const { return m_windowScreenId; }

  void setHideOnMinimize(bool value);
  void setHideOnClose(bool value);
  void setWindowWidth(int value);
  void setWindowHeight(int value);
  void setWindowMaximized(bool value);
  Q_INVOKABLE bool sync();
  Q_INVOKABLE bool hasPermissionGrant(const QUrl &origin, int permissionType);
  Q_INVOKABLE bool rememberPermissionGrant(const QUrl &origin, int permissionType);
  Q_INVOKABLE int protocolHandlerDecision(const QUrl &origin, const QString &scheme);
  Q_INVOKABLE bool rememberProtocolHandlerDecision(const QUrl &origin,
                                                   const QString &scheme,
                                                   bool accepted);
  void restoreWindowGeometry(QWindow *window) const;
  void saveWindowGeometry(QWindow *window);
  void bindWindow(QWindow *window);
  static QString screenIdentifier(const QScreen *screen);
  static qsizetype chooseScreen(const QString &savedId, const QString &savedName,
                                const QRect &savedGeometry,
                                const QList<ScreenDescription> &screens);
  static QRect clampGeometry(const QRect &geometry, const QRect &availableGeometry);

signals:
  void hideOnMinimizeChanged();
  void hideOnCloseChanged();
  void windowGeometryChanged();
  void persistenceError(const QString &message);

private:
  QString permissionKey(const QUrl &origin, int permissionType) const;
  QString legacyPermissionKey(const QUrl &origin, int permissionType) const;
  QString protocolHandlerKey(const QUrl &origin, const QString &scheme) const;
  QString legacyProtocolHandlerKey(const QUrl &origin, const QString &scheme) const;
  QSettings m_settings;
  bool m_hideOnMinimize = false;
  bool m_hideOnClose = true;
  int m_windowWidth = 967;
  int m_windowHeight = 557;
  bool m_windowMaximized = false;
  bool m_hasWindowPosition = false;
  int m_windowOffsetX = 0;
  int m_windowOffsetY = 0;
  QString m_windowScreenId;
  QString m_windowScreenName;
  QRect m_windowScreenGeometry;
};
