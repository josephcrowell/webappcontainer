// Copyright(C) 2026 Joseph Crowell.
// SPDX - License - Identifier : GPL-2.0-or-later

#include "webpage.h"
#include "publicsuffixlist.h"
#include "webpopupwindow.h"
#include "webview.h"

#include <QDesktopServices>
#include <QTimer>

WebPage::WebPage(QWebEngineProfile *profile, QObject *parent)
    : QWebEnginePage(profile, parent) {
  connect(this, &QWebEnginePage::selectClientCertificate, this,
          &WebPage::handleSelectClientCertificate);
  connect(this, &QWebEnginePage::certificateError, this,
          &WebPage::handleCertificateError);
  connect(this, &QWebEnginePage::desktopMediaRequested, this,
          &WebPage::handleDesktopMediaRequest);
}

void WebPage::handleCertificateError(QWebEngineCertificateError error) {
  // Automatically block certificate errors from page resources without
  // prompting the user. This mirrors the behavior found in other major
  // browsers.
  if (!error.isMainFrame()) {
    error.rejectCertificate();
    return;
  }

  error.defer();
  QTimer::singleShot(0, this, [this, error]() mutable {
    emit createCertificateErrorDialog(error);
  });
}

void WebPage::handleSelectClientCertificate(
    QWebEngineClientCertificateSelection selection) {
  // Just select one.
  selection.select(selection.certificates().at(0));
}

void WebPage::handleDesktopMediaRequest(
    const QWebEngineDesktopMediaRequest &request) {
  // select the primary screen
  request.selectScreen(request.screensModel()->index(0));
}

bool WebPage::acceptNavigationRequest(const QUrl &url, NavigationType type,
                                      bool isMainFrame) {
  // Only intercept links clicked by the user
  if (type == QWebEnginePage::NavigationTypeLinkClicked) {
    const QUrl currentUrl = this->url();

    // Check if the host is different from the current page
    if (currentUrl.isValid() && !PublicSuffixList::instance()->isSameDomain(
                                    currentUrl.host(), url.host())) {
      // Open in the default system browser
      QDesktopServices::openUrl(url);
      return false; // Prevent the web container from loading this URL
    }
  }

  // Allow the container to load everything else (initial load, same domain
  // navigation)
  return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
}

QWebEnginePage *WebPage::createWindow(WebWindowType type) {
  // Create a ghost page to catch the URL
  QWebEnginePage *ghostPage = new QWebEnginePage(this->profile(), this);

  connect(ghostPage, &QWebEnginePage::urlChanged,
          [this, ghostPage](const QUrl &url) {
            // Skip JavaScript window.open() or target="_blank"
            if (url.isValid() && url != QUrl("about:blank")) {
              // Special handler for opening facebook calls
              if (url.host().toLower().endsWith("facebook.com") ||
                  url.host().toLower().endsWith("messenger.com") &&
                      url.path().toLower().startsWith("/groupcall/")) {
                // Create a popup window for Facebook calls
                WebPopupWindow *popup = new WebPopupWindow(this->profile());
                popup->view()->setUrl(url);
                popup->show();
              } else if (url.host() == QString("phone.cloudtalk.io")) {
                // Create a popup window for CloudTalk calls
                WebPopupWindow *popup = new WebPopupWindow(this->profile());
                popup->view()->setUrl(url);
                popup->show();
              } else {
                QDesktopServices::openUrl(url);
              }
              // We've sent the URL to the browser, now kill the ghost page
              ghostPage->deleteLater();
            }
          });

  // Return the ghost page to the engine so it thinks it succeeded,
  // but the URL will immediately trigger our signal above.
  return ghostPage;
}
