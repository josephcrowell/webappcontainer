// Copyright(C) 2026 Joseph Crowell.
// SPDX - License - Identifier : GPL-2.0-or-later

#ifndef BROWSERWINDOW_H
#define BROWSERWINDOW_H

#include <QAction>
#include <QDialog>
#include <QEvent>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QWebEngineProfile>

#include "downloadmanagerwidget.h"
#include "webview.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class BrowserWindow;
}
QT_END_NAMESPACE

class BrowserWindow : public QDialog {
  Q_OBJECT

public:
  BrowserWindow(QWebEngineProfile *profile, const QString appName = "",
                const QString iconPath = "", const QString trayIconPath = "",
                bool notify = true, QWidget *parent = nullptr);
  ~BrowserWindow();
  WebView *webView() const;
  DownloadManagerWidget &downloadManagerWidget() {
    return m_downloadManagerWidget;
  }
  bool isValidImage(const QString &path);

private:
  Ui::BrowserWindow *ui;
  QSystemTrayIcon *m_trayIcon;
  QMenu *m_trayMenu;
  QAction *hideOnMinimizeAction;
  QAction *hideOnCloseAction;
  QAction *quitAction;
  QAction *restoreAction;
  WebView *m_webView;
  DownloadManagerWidget m_downloadManagerWidget;
  QWebEngineProfile *m_profile;
  bool m_notify;
  bool m_hideOnMinimize;
  bool m_hideOnClose;
  bool m_hasNotification;
  QIcon m_baseIcon;
  QIcon m_notificationIcon;

  void loadLayout();
  void saveLayout();
  void loadSettings();
  void saveSettings();
  void updateTrayIcon();
  QIcon createNotificationIcon(const QIcon &baseIcon);
  void clearNotificationIndicator();
  bool isQuitting = false;

private slots:
  void handleWebNotification(QWebEngineNotification *notification);

protected:
  void changeEvent(QEvent *event) override;
  void closeEvent(QCloseEvent *event) override;
  bool event(QEvent *event) override;
};
#endif // BROWSERWINDOW_H
