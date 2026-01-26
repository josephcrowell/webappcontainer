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
#include <QPainter>
#include <QSettings>
#include <QStyle>
#include <QWebEngineCookieStore>
#include <QWebEngineNotification>
#include <QWindow>

BrowserWindow::BrowserWindow(QWebEngineProfile *profile, const QString appName,
                             const QString iconPath, const QString trayIconPath,
                             bool notify, QWidget *parent)
    : QDialog(parent), ui(new Ui::BrowserWindow), m_profile(profile),
      m_webView(new WebView(profile, this)), m_notify(notify),
      m_hideOnMinimize(false), m_hideOnClose(true) {
  ui->setupUi(this);

  loadLayout();
  loadSettings();

  // Set the Window Title
  if (!appName.isEmpty()) {
    setWindowTitle(appName);
  }

  if (isValidImage(iconPath)) {
    setWindowIcon(QIcon(iconPath));
  } else {
    setWindowIcon(style()->standardIcon(QStyle::SP_TitleBarMenuButton));
  }

  // Determine the base tray icon
  if (isValidImage(trayIconPath)) {
    m_baseIcon = QIcon(trayIconPath);
  } else {
    m_baseIcon = this->windowIcon();
  }

  // Create the notification variant
  m_notificationIcon = createNotificationIcon(m_baseIcon);

  // Create Actions for the tray icon Menu
  restoreAction = new QAction("Restore", this);
  connect(restoreAction, &QAction::triggered, this, &QWidget::showNormal);

  hideOnMinimizeAction = new QAction("Minimize to Tray", this);
  hideOnMinimizeAction->setCheckable(true);
  hideOnMinimizeAction->setChecked(m_hideOnMinimize);
  connect(hideOnMinimizeAction, &QAction::toggled, this, [this](bool checked) {
    m_hideOnMinimize = checked;
    saveSettings();
  });

  hideOnCloseAction = new QAction("Close to Tray", this);
  hideOnCloseAction->setCheckable(true);
  hideOnCloseAction->setChecked(m_hideOnClose);
  connect(hideOnCloseAction, &QAction::toggled, this, [this](bool checked) {
    m_hideOnClose = checked;
    saveSettings();
  });

  quitAction = new QAction("Exit", this);
  connect(quitAction, &QAction::triggered, this, [this]() {
    isQuitting = true; // Tell the closeEvent we actually want to quit
    qApp->quit();
  });

  // Create the tray icon Context Menu
  m_trayMenu = new QMenu(this);
  m_trayMenu->addAction(restoreAction);
  m_trayMenu->addSeparator();
  m_trayMenu->addAction(hideOnMinimizeAction);
  m_trayMenu->addAction(hideOnCloseAction);
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

  m_trayIcon->setIcon(m_baseIcon);
  m_trayIcon->show();

  // Handle double-click on tray icon
  connect(m_trayIcon, &QSystemTrayIcon::activated,
          [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger) { // Trigger = Left Click
              if (this->isMinimized()) {
                // Window is minimized - restore it
                this->showNormal();
                this->activateWindow();
                this->raise();
              } else if (!this->isVisible()) {
                // Window is hidden - show it
                this->showNormal();
                this->activateWindow();
                this->raise();
              } else if (this->isActiveWindow()) {
                // Window is visible and focused - hide it
                this->hide();
              } else {
                // Window is visible but not focused - raise it
                this->activateWindow();
                this->raise();
#ifdef Q_OS_LINUX
                if (QWindow *win = this->windowHandle()) {
                  win->requestActivate();
                }
#endif
              }
            }
          });

  // Handle notification clicks from system tray
  connect(m_trayIcon, &QSystemTrayIcon::messageClicked, this,
          &BrowserWindow::onNotificationClicked);

  // Notifications happen at the Profile level, not the Page level
  m_profile->setNotificationPresenter(
      [&](std::unique_ptr<QWebEngineNotification> notification) {
        BrowserWindow::handleWebNotification(notification.get());
      });

  // Quit application if the download manager is the only remaining window
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

BrowserWindow::~BrowserWindow() {
  if (m_trayIcon) {
    m_trayIcon->hide();
    disconnect(m_trayIcon, nullptr, this, nullptr);
  }

  if (m_profile) {
    m_profile->setNotificationPresenter(nullptr);
    disconnect(m_profile, nullptr, this, nullptr);
    disconnect(m_profile, nullptr, &m_downloadManagerWidget, nullptr);
  }

  m_currentNotification = nullptr;

  if (m_webView) {
    delete m_webView;
    m_webView = nullptr;
  }

  QCoreApplication::processEvents();
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

  delete ui;
}

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

void BrowserWindow::loadSettings() {
  QSettings settings(m_profile->persistentStoragePath() + "/settings.ini",
                     QSettings::IniFormat);

  settings.beginGroup("Behavior");
  m_hideOnMinimize = settings.value("hideOnMinimize", false).toBool();
  m_hideOnClose = settings.value("hideOnClose", true).toBool();
  settings.endGroup();
}

void BrowserWindow::saveSettings() {
  QSettings settings(m_profile->persistentStoragePath() + "/settings.ini",
                     QSettings::IniFormat);

  settings.beginGroup("Behavior");
  settings.setValue("hideOnMinimize", m_hideOnMinimize);
  settings.setValue("hideOnClose", m_hideOnClose);
  settings.endGroup();

  settings.sync();
}

QIcon BrowserWindow::createNotificationIcon(const QIcon &baseIcon) {
  // Get the largest available size or use a default
  QList<QSize> sizes = baseIcon.availableSizes();
  QSize iconSize = sizes.isEmpty() ? QSize(64, 64) : sizes.last();

  QPixmap pixmap = baseIcon.pixmap(iconSize);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);

  // Draw red dot in top-right corner
  int dotSize = iconSize.width() / 3;
  int x = iconSize.width() - dotSize - 1;
  int y = 1;

  // Draw white border
  painter.setBrush(Qt::white);
  painter.setPen(Qt::NoPen);
  painter.drawEllipse(x - 1, y - 1, dotSize + 2, dotSize + 2);

  // Draw red dot
  painter.setBrush(QColor(255, 59, 48)); // Apple-style red
  painter.drawEllipse(x, y, dotSize, dotSize);

  painter.end();

  return QIcon(pixmap);
}

void BrowserWindow::updateTrayIcon() {
  if (m_hasNotification) {
    m_trayIcon->setIcon(m_notificationIcon);
  } else {
    m_trayIcon->setIcon(m_baseIcon);
  }
}

void BrowserWindow::clearNotificationIndicator() {
  if (m_hasNotification) {
    m_hasNotification = false;
    updateTrayIcon();
  }
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
  if (isQuitting) {
    saveLayout();
    event->accept();
  } else if (m_hideOnClose) {
    this->hide();
    event->ignore();

    if (m_notify) {
      m_trayIcon->showMessage(
          "Running in background",
          "The application is still active in the system tray.",
          QSystemTrayIcon::Information, 2000);
    }
  } else {
    isQuitting = true;
    saveLayout();
    event->accept();
    qApp->quit();
  }
}

void BrowserWindow::handleWebNotification(
    QWebEngineNotification *notification) {

  // Store reference to current notification for click handling
  m_currentNotification = notification;

  // Connect to notification signals for proper lifecycle management
  connect(notification, &QWebEngineNotification::closed, this,
          [this]() { m_currentNotification = nullptr; });

  // Show system tray notification with icon if available
  QIcon notificationIcon;
  if (!notification->icon().isNull()) {
    notificationIcon = QIcon(QPixmap::fromImage(notification->icon()));
  } else {
    notificationIcon = m_trayIcon->icon();
  }

  m_trayIcon->showMessage(notification->title(), notification->message(),
                          notificationIcon);

  // Show the native notification through Qt WebEngine
  notification->show();

  // Log notification for debugging
  qDebug() << "Push Notification Received:";
  qDebug() << "  Title:" << notification->title();
  qDebug() << "  Message:" << notification->message();
  qDebug() << "  Origin:" << notification->origin().toString();
  qDebug() << "  Tag:" << notification->tag();
  qDebug() << "  Language:" << notification->language();
  qDebug() << "  Direction:" << notification->direction();

  // Only show indicator if window doesn't have focus
  if (!isActiveWindow()) {
    m_hasNotification = true;
    updateTrayIcon();
  }
}

void BrowserWindow::onNotificationClicked() {
  // Handle notification click - restore window and trigger notification click
  if (m_currentNotification) {
    // Tell the web page that the notification was clicked
    m_currentNotification->click();

    // Close the notification
    m_currentNotification->close();
    m_currentNotification = nullptr;
  }

  // Restore and focus the window
  if (isMinimized()) {
    showNormal();
  } else if (!isVisible()) {
    show();
  }

  activateWindow();
  raise();

#ifdef Q_OS_LINUX
  if (QWindow *win = windowHandle()) {
    win->requestActivate();
  }
#endif

  // Clear the notification indicator
  clearNotificationIndicator();
}

void BrowserWindow::changeEvent(QEvent *event) {
  if (event->type() == QEvent::WindowStateChange) {
    if (this->isMinimized() && m_hideOnMinimize && m_trayIcon &&
        m_trayIcon->isVisible()) {
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
  } else if (event->type() == QEvent::ActivationChange) {
    if (isActiveWindow()) {
      clearNotificationIndicator();
    }
  }

  QDialog::changeEvent(event);
}

bool BrowserWindow::event(QEvent *event) {
  if (event->type() == QEvent::WindowActivate) {
    clearNotificationIndicator();
  }
  return QDialog::event(event);
}
