// Copyright(C) 2026 Joseph Crowell.
// SPDX - License - Identifier : GPL-2.0-or-later

#include "browserwindow.h"
#include "./ui_browserwindow.h"

#include "webpage.h"

#include <QCloseEvent>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QImageReader>
#include <QNetworkCookie>
#include <QSettings>
#include <QStyle>
#include <QWebEngineCookieStore>
#include <QWebEngineNotification>

BrowserWindow::BrowserWindow(QWebEngineProfile *profile, const QString appName,
                             const QString iconPath, const QString trayIconPath,
                             bool notify, QWidget *parent)
    : QDialog(parent), ui(new Ui::BrowserWindow), m_profile(profile),
      m_webView(new WebView(profile, this)), m_notify(notify) {
  ui->setupUi(this);

  loadLayout();

  // Set the Window Title
  if (!appName.isEmpty()) {
    setWindowTitle(appName);
  }

  if (isValidImage(iconPath)) {
    setWindowIcon(QIcon(iconPath));
  } else {
    setWindowIcon(style()->standardIcon(QStyle::SP_TitleBarMenuButton));
  }

  // Create Actions for the tray icon Menu
  restoreAction = new QAction("Restore", this);
  connect(restoreAction, &QAction::triggered, this, &QWidget::showNormal);

  quitAction = new QAction("Exit", this);
  connect(quitAction, &QAction::triggered, this, [this]() {
    isQuitting = true; // Tell the closeEvent we actually want to quit
    qApp->quit();
  });

  // Create the tray icon Context Menu
  m_trayMenu = new QMenu(this);
  m_trayMenu->addAction(restoreAction);
  m_trayMenu->addSeparator();
  m_trayMenu->addAction(quitAction);

  // Initialize Tray Icon
  m_trayIcon = new QSystemTrayIcon(this);
  m_trayIcon->setContextMenu(m_trayMenu);
  if (appName.isEmpty()) {
    m_trayIcon->setToolTip("Web App Container");
  } else {
    m_trayIcon->setToolTip(appName);
  }

  if (isValidImage(trayIconPath)) {
    m_trayIcon->setIcon(QIcon(trayIconPath));
  } else if (isValidImage(iconPath)) {
    // Fallback to the window icon if no specific tray icon is provided
    m_trayIcon->setIcon(QIcon(iconPath));
  } else {
    m_trayIcon->setIcon(style()->standardIcon(QStyle::SP_TitleBarMenuButton));
  }

  m_trayIcon->show();

  // Handle double-click on tray icon
  connect(m_trayIcon, &QSystemTrayIcon::activated,
          [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger) { // Trigger = Left Click
              if (this->isVisible() && !this->isMinimized()) {
                this->hide();
              } else {
                this->showNormal();
                this->activateWindow();
                this->raise();
              }
            }
          });

  // Notifications happen at the Profile level, not the Page level
  connect(m_webView->page()->profile(),
          &QWebEngineProfile::setNotificationPresenter, this,
          &BrowserWindow::handleWebNotification);

  // Quit application if the download manager window is the only remaining
  // window
  m_downloadManagerWidget.setAttribute(Qt::WA_QuitOnClose, false);

  QWebEngineCookieStore *store = m_profile->cookieStore();
  // This tells the engine: "Yes, allow every cookie request"
  store->setCookieFilter(
      [](const QWebEngineCookieStore::FilterRequest &request) { return true; });
  // Force the profile to 'touch' the storage
  store->loadAllCookies();

  QObject::connect(m_profile, &QWebEngineProfile::downloadRequested,
                   &m_downloadManagerWidget,
                   &DownloadManagerWidget::downloadRequested);

  // Add to layout
  ui->webViewLayout->addWidget(m_webView);
}

BrowserWindow::~BrowserWindow() { delete ui; }

void BrowserWindow::loadLayout() {
  QSettings settings(m_profile->persistentStoragePath() + "/settings.ini",
                     QSettings::IniFormat);

  settings.beginGroup("BrowserWindow");
  QByteArray geometry = settings.value("geometry").toByteArray();
  if (!geometry.isEmpty()) {
    restoreGeometry(geometry);
  }
  settings.endGroup();
}

void BrowserWindow::saveLayout() {
  QSettings settings(m_profile->persistentStoragePath() + "/settings.ini",
                     QSettings::IniFormat);

  settings.beginGroup("BrowserWindow");
  settings.setValue("geometry", saveGeometry());
  settings.endGroup();

  settings.sync();
}

bool BrowserWindow::isValidImage(const QString &path) {
  if (path.isEmpty() || !QFileInfo::exists(path)) {
    return false;
  }

  QImageReader reader(path);
  // This checks the file header to see if Qt can actually decode it
  if (reader.canRead()) {
    return true;
  }

  qWarning() << "Invalid or unsupported image file:" << path
             << "Error:" << reader.errorString();
  return false;
}

WebView *BrowserWindow::webView() const { return m_webView; }

void BrowserWindow::closeEvent(QCloseEvent *event) {
  // If we are exiting via the tray menu, just let it happen
  if (isQuitting) {
    saveLayout();
    event->accept();
  } else {
    // Check if the tray icon is actually functional
    if (m_trayIcon->isVisible()) {
      this->hide();    // Hide the window from the taskbar and desktop
      event->ignore(); // "Ignore" means: Don't actually close/exit the app

      // Optional notification
      if (m_notify) {
        m_trayIcon->showMessage(
            "Running in background",
            "The application is still active in the system tray.",
            QSystemTrayIcon::Information, 2000);
      }
    } else {
      // If for some reason the tray icon isn't loaded,
      // allow the app to close so the user isn't stuck.
      saveLayout();
      event->accept();
    }
  }
}

void BrowserWindow::handleWebNotification(
    QWebEngineNotification *notification) {

  m_trayIcon->showMessage(notification->title(), notification->message(),
                          m_trayIcon->icon());

  notification->show();
}

void BrowserWindow::changeEvent(QEvent *event) {
  if (event->type() == QEvent::WindowStateChange) {
    if (this->isMinimized() && m_trayIcon && m_trayIcon->isVisible()) {
      // Using a singleShot timer with 0ms delay ensures the
      // hide happens after the OS finishes its minimize animation.
      QTimer::singleShot(0, this, &BrowserWindow::hide);

      // Optional: Notify the user
      if (m_notify) {
        m_trayIcon->showMessage(
            "App Minimized",
            "The application is still running in the system tray.",
            QSystemTrayIcon::Information, 2000);
      }
    }
  }

  // Always call the base class implementation!
  QDialog::changeEvent(event);
}
