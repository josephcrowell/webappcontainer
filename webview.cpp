// Copyright(C) 2026 Joseph Crowell.
// SPDX - License - Identifier : GPL-2.0-or-later

#include "webview.h"
#include "ui_certificateerrordialog.h"
#include "ui_passworddialog.h"
#include "webauthdialog.h"
#include "webpage.h"
#include "webpopupwindow.h"
#include <QAuthenticator>
#include <QContextMenuEvent>
#include <QDebug>
#include <QMenu>
#include <QMessageBox>
#include <QSettings>
#include <QStyle>
#include <QTimer>
#include <QWebEngineProfile>

using namespace Qt::StringLiterals;

WebView::WebView(QWebEngineProfile *profile, QWidget *parent)
    : QWebEngineView(parent) {
  WebPage *page = new WebPage(profile, this);
  setPage(page);

  connect(this, &QWebEngineView::loadStarted, [this]() {
    m_loadProgress = 0;
    emit favIconChanged(favIcon());
  });
  connect(this, &QWebEngineView::loadProgress,
          [this](int progress) { m_loadProgress = progress; });
  connect(this, &QWebEngineView::loadFinished, [this](bool success) {
    m_loadProgress = success ? 100 : -1;
    emit favIconChanged(favIcon());
  });
  connect(this, &QWebEngineView::iconChanged,
          [this](const QIcon &) { emit favIconChanged(favIcon()); });

  connect(this, &QWebEngineView::renderProcessTerminated,
          [this](QWebEnginePage::RenderProcessTerminationStatus termStatus,
                 int statusCode) {
            QString status;
            switch (termStatus) {
            case QWebEnginePage::NormalTerminationStatus:
              status = tr("Render process normal exit");
              break;
            case QWebEnginePage::AbnormalTerminationStatus:
              status = tr("Render process abnormal exit");
              break;
            case QWebEnginePage::CrashedTerminationStatus:
              status = tr("Render process crashed");
              break;
            case QWebEnginePage::KilledTerminationStatus:
              status = tr("Render process killed");
              break;
            }
            QMessageBox::StandardButton btn =
                QMessageBox::question(window(), status,
                                      tr("Render process exited with code: %1\n"
                                         "Do you want to reload the page ?")
                                          .arg(statusCode));
            if (btn == QMessageBox::Yes)
              QTimer::singleShot(0, this, &WebView::reload);
          });
}

WebView::~WebView() {
  if (m_imageAnimationGroup)
    delete m_imageAnimationGroup;

  m_imageAnimationGroup = nullptr;
}

inline QString
questionForPermissionType(QWebEnginePermission::PermissionType permissionType) {
  switch (permissionType) {
  case QWebEnginePermission::PermissionType::Geolocation:
    return QObject::tr("Allow %1 to access your location information?");
  case QWebEnginePermission::PermissionType::MediaAudioCapture:
    return QObject::tr("Allow %1 to access your microphone?");
  case QWebEnginePermission::PermissionType::MediaVideoCapture:
    return QObject::tr("Allow %1 to access your webcam?");
  case QWebEnginePermission::PermissionType::MediaAudioVideoCapture:
    return QObject::tr("Allow %1 to access your microphone and webcam?");
  case QWebEnginePermission::PermissionType::MouseLock:
    return QObject::tr("Allow %1 to lock your mouse cursor?");
  case QWebEnginePermission::PermissionType::DesktopVideoCapture:
    return QObject::tr("Allow %1 to capture video of your desktop?");
  case QWebEnginePermission::PermissionType::DesktopAudioVideoCapture:
    return QObject::tr("Allow %1 to capture audio and video of your desktop?");
  case QWebEnginePermission::PermissionType::Notifications:
    return QObject::tr("Allow %1 to show notification on your desktop?");
  case QWebEnginePermission::PermissionType::ClipboardReadWrite:
    return QObject::tr("Allow %1 to read from and write to the clipboard?");
  case QWebEnginePermission::PermissionType::LocalFontsAccess:
    return QObject::tr("Allow %1 to access fonts stored on this machine?");
  case QWebEnginePermission::PermissionType::Unsupported:
    break;
  }
  return QString();
}

void WebView::setPage(WebPage *page) {
  if (auto oldPage = qobject_cast<WebPage *>(QWebEngineView::page())) {
    disconnect(oldPage, &WebPage::createCertificateErrorDialog, this,
               &WebView::handleCertificateError);
    disconnect(oldPage, &QWebEnginePage::authenticationRequired, this,
               &WebView::handleAuthenticationRequired);
    disconnect(oldPage, &QWebEnginePage::permissionRequested, this,
               &WebView::handlePermissionRequested);
    disconnect(oldPage, &QWebEnginePage::proxyAuthenticationRequired, this,
               &WebView::handleProxyAuthenticationRequired);
    disconnect(oldPage, &QWebEnginePage::registerProtocolHandlerRequested, this,
               &WebView::handleRegisterProtocolHandlerRequested);
    disconnect(oldPage, &QWebEnginePage::webAuthUxRequested, this,
               &WebView::handleWebAuthUxRequested);
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    disconnect(oldPage, &QWebEnginePage::fileSystemAccessRequested, this,
               &WebView::handleFileSystemAccessRequested);
#endif
  }
  createWebActionTrigger(page, QWebEnginePage::Forward);
  createWebActionTrigger(page, QWebEnginePage::Back);
  createWebActionTrigger(page, QWebEnginePage::Reload);
  createWebActionTrigger(page, QWebEnginePage::Stop);
  QWebEngineView::setPage(page);
  connect(page, &WebPage::createCertificateErrorDialog, this,
          &WebView::handleCertificateError);
  connect(page, &QWebEnginePage::authenticationRequired, this,
          &WebView::handleAuthenticationRequired);
  connect(page, &QWebEnginePage::permissionRequested, this,
          &WebView::handlePermissionRequested);
  connect(page, &QWebEnginePage::proxyAuthenticationRequired, this,
          &WebView::handleProxyAuthenticationRequired);
  connect(page, &QWebEnginePage::registerProtocolHandlerRequested, this,
          &WebView::handleRegisterProtocolHandlerRequested);
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  connect(page, &QWebEnginePage::fileSystemAccessRequested, this,
          &WebView::handleFileSystemAccessRequested);
#endif
  connect(page, &QWebEnginePage::webAuthUxRequested, this,
          &WebView::handleWebAuthUxRequested);
}

int WebView::loadProgress() const { return m_loadProgress; }

void WebView::createWebActionTrigger(QWebEnginePage *page,
                                     QWebEnginePage::WebAction webAction) {
  QAction *action = page->action(webAction);
  connect(action, &QAction::changed, [this, action, webAction] {
    emit webActionEnabledChanged(webAction, action->isEnabled());
  });
}

bool WebView::isWebActionEnabled(QWebEnginePage::WebAction webAction) const {
  return page()->action(webAction)->isEnabled();
}

QIcon WebView::favIcon() const {
  QIcon favIcon = icon();
  if (!favIcon.isNull())
    return favIcon;

  if (m_loadProgress < 0) {
    static QIcon errorIcon(u":dialog-error.png"_s);
    return errorIcon;
  }
  if (m_loadProgress < 100) {
    static QIcon loadingIcon = QIcon::fromTheme(QIcon::ThemeIcon::ViewRefresh,
                                                QIcon(":view-refresh.png"_L1));
    return loadingIcon;
  }

  static QIcon defaultIcon(u":text-html.png"_s);
  return defaultIcon;
}

void WebView::contextMenuEvent(QContextMenuEvent *event) {
  QMenu *menu = createStandardContextMenu();
  const QList<QAction *> actions = menu->actions();
  for (QAction *action : actions) {
    if (action == page()->action(QWebEnginePage::SavePage) ||
        action == page()->action(QWebEnginePage::InspectElement) ||
        action == page()->action(QWebEnginePage::ViewSource)) {

      menu->removeAction(action);
    }
  }

  menu->popup(event->globalPos());
}

void WebView::handleCertificateError(QWebEngineCertificateError error) {
  QDialog dialog(window());
  dialog.setModal(true);
  dialog.setWindowFlags(dialog.windowFlags() &
                        ~Qt::WindowContextHelpButtonHint);

  Ui::CertificateErrorDialog certificateDialog;
  certificateDialog.setupUi(&dialog);
  certificateDialog.m_iconLabel->setText(QString());
  QIcon icon(window()->style()->standardIcon(QStyle::SP_MessageBoxWarning, 0,
                                             window()));
  certificateDialog.m_iconLabel->setPixmap(icon.pixmap(32, 32));
  certificateDialog.m_errorLabel->setText(error.description());
  dialog.setWindowTitle(tr("Certificate Error"));

  if (dialog.exec() == QDialog::Accepted)
    error.acceptCertificate();
  else
    error.rejectCertificate();
}

void WebView::handleAuthenticationRequired(const QUrl &requestUrl,
                                           QAuthenticator *auth) {
  QDialog dialog(window());
  dialog.setModal(true);
  dialog.setWindowFlags(dialog.windowFlags() &
                        ~Qt::WindowContextHelpButtonHint);

  Ui::PasswordDialog passwordDialog;
  passwordDialog.setupUi(&dialog);

  passwordDialog.m_iconLabel->setText(QString());
  QIcon icon(window()->style()->standardIcon(QStyle::SP_MessageBoxQuestion, 0,
                                             window()));
  passwordDialog.m_iconLabel->setPixmap(icon.pixmap(32, 32));

  QString introMessage(
      tr("Enter username and password for \"%1\" at %2")
          .arg(auth->realm(), requestUrl.toString().toHtmlEscaped()));
  passwordDialog.m_infoLabel->setText(introMessage);
  passwordDialog.m_infoLabel->setWordWrap(true);

  if (dialog.exec() == QDialog::Accepted) {
    auth->setUser(passwordDialog.m_userNameLineEdit->text());
    auth->setPassword(passwordDialog.m_passwordLineEdit->text());
  } else {
    // Set authenticator null if dialog is cancelled
    *auth = QAuthenticator();
  }
}

void WebView::handlePermissionRequested(QWebEnginePermission permission) {
  QSettings settings(page()->profile()->persistentStoragePath() +
                         "/settings.ini",
                     QSettings::IniFormat);
  settings.beginGroup("Permissions");

  // Create a unique key for this domain and permission type
  QString host = permission.origin().host().replace(".", "_");
  int type = static_cast<int>(permission.permissionType());
  QString key = QString("grants/%1/%2").arg(host).arg(type);

  // Check if we already have a saved "Yes"
  if (settings.value(key).toBool()) {
    permission.grant();
    return;
  }

  // Do the permission request and save a yes answer
  QString title = tr("Permission Request");
  QString question = questionForPermissionType(permission.permissionType())
                         .arg(permission.origin().host());
  if (!question.isEmpty() &&
      QMessageBox::question(window(), title, question) == QMessageBox::Yes) {
    settings.setValue(key, true);
    settings.sync();
    permission.grant();
  } else {
    permission.deny();
  }
}

void WebView::handleProxyAuthenticationRequired(const QUrl &,
                                                QAuthenticator *auth,
                                                const QString &proxyHost) {
  QDialog dialog(window());
  dialog.setModal(true);
  dialog.setWindowFlags(dialog.windowFlags() &
                        ~Qt::WindowContextHelpButtonHint);

  Ui::PasswordDialog passwordDialog;
  passwordDialog.setupUi(&dialog);

  passwordDialog.m_iconLabel->setText(QString());
  QIcon icon(window()->style()->standardIcon(QStyle::SP_MessageBoxQuestion, 0,
                                             window()));
  passwordDialog.m_iconLabel->setPixmap(icon.pixmap(32, 32));

  QString introMessage = tr("Connect to proxy \"%1\" using:");
  introMessage = introMessage.arg(proxyHost.toHtmlEscaped());
  passwordDialog.m_infoLabel->setText(introMessage);
  passwordDialog.m_infoLabel->setWordWrap(true);

  if (dialog.exec() == QDialog::Accepted) {
    auth->setUser(passwordDialog.m_userNameLineEdit->text());
    auth->setPassword(passwordDialog.m_passwordLineEdit->text());
  } else {
    // Set authenticator null if dialog is cancelled
    *auth = QAuthenticator();
  }
}

void WebView::handleWebAuthUxRequested(QWebEngineWebAuthUxRequest *request) {
  if (m_authDialog)
    delete m_authDialog;

  m_authDialog = new WebAuthDialog(request, window());
  m_authDialog->setModal(false);
  m_authDialog->setWindowFlags(m_authDialog->windowFlags() &
                               ~Qt::WindowContextHelpButtonHint);

  connect(request, &QWebEngineWebAuthUxRequest::stateChanged, this,
          &WebView::onStateChanged);
  m_authDialog->show();
}

void WebView::onStateChanged(QWebEngineWebAuthUxRequest::WebAuthUxState state) {
  if (QWebEngineWebAuthUxRequest::WebAuthUxState::Completed == state ||
      QWebEngineWebAuthUxRequest::WebAuthUxState::Cancelled == state) {
    if (m_authDialog) {
      delete m_authDialog;
      m_authDialog = nullptr;
    }
  } else {
    m_authDialog->updateDisplay();
  }
}

//! [registerProtocolHandlerRequested]
void WebView::handleRegisterProtocolHandlerRequested(
    QWebEngineRegisterProtocolHandlerRequest request) {
  auto answer = QMessageBox::question(window(), tr("Permission Request"),
                                      tr("Allow %1 to open all %2 links?")
                                          .arg(request.origin().host())
                                          .arg(request.scheme()));
  if (answer == QMessageBox::Yes)
    request.accept();
  else
    request.reject();
}
//! [registerProtocolHandlerRequested]

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
void WebView::handleFileSystemAccessRequested(
    QWebEngineFileSystemAccessRequest request) {
  QString accessType;
  switch (request.accessFlags()) {
  case QWebEngineFileSystemAccessRequest::Read:
    accessType = "read";
    break;
  case QWebEngineFileSystemAccessRequest::Write:
    accessType = "write";
    break;
  case QWebEngineFileSystemAccessRequest::Read |
      QWebEngineFileSystemAccessRequest::Write:
    accessType = "read and write";
    break;
  default:
    Q_UNREACHABLE();
  }

  auto answer =
      QMessageBox::question(window(), tr("File system access request"),
                            tr("Give %1 %2 access to %3?")
                                .arg(request.origin().host())
                                .arg(accessType)
                                .arg(request.filePath().toString()));
  if (answer == QMessageBox::Yes)
    request.accept();
  else
    request.reject();
}

void WebView::handleImageAnimationPolicyChange(
    QWebEngineSettings::ImageAnimationPolicy policy) {
  if (!page())
    return;

  page()->settings()->setImageAnimationPolicy(policy);
}
#endif // QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
