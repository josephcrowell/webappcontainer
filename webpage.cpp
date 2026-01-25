// Copyright(C) 2026 Joseph Crowell.
// SPDX - License - Identifier : GPL-2.0-or-later

#include "webpage.h"
#include "publicsuffixlist.h"
#include "webpopupwindow.h"
#include "webview.h"

#include <QDesktopServices>
#include <QTimer>

WebPage::WebPage(QWebEngineProfile *profile, QWidget *parent)
    : m_parent(parent), QWebEnginePage(profile, parent) {
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

QWebEnginePage *WebPage::createWindow(WebWindowType type) {
  // Create a ghost page to catch the URL
  QWebEnginePage *ghostPage = new QWebEnginePage(this->profile(), this);

  // Store pending geometry
  QRect *pendingGeometry = new QRect();

  connect(ghostPage, &QWebEnginePage::geometryChangeRequested,
          [pendingGeometry](const QRect &geom) { *pendingGeometry = geom; });

  connect(ghostPage, &QWebEnginePage::urlChanged,
          [this, ghostPage, pendingGeometry](const QUrl &url) {
            const QUrl currentUrl = this->url();

            // Skip JavaScript window.open() or target="_blank"
            if (url.isValid() && url != QUrl("about:blank")) {
              // Special handler for opening facebook calls
              if ((currentUrl.host().toLower().endsWith("facebook.com") ||
                   currentUrl.host().toLower().endsWith("messenger.com")) &&
                  (url.host().toLower().endsWith("facebook.com") ||
                   url.host().toLower().endsWith("messenger.com")) &&
                  (url.path().toLower().startsWith("/groupcall/") ||
                   url.path().toLower().startsWith("/settings/"))) {
                // Create a popup window for Facebook calls
                WebPopupWindow *popup = new WebPopupWindow(
                    this->profile(), *pendingGeometry, m_parent);
                popup->view()->setUrl(url);
                popup->show();
              } else if (PublicSuffixList::instance()->isSameDomain(
                             currentUrl.host(), url.host())) {
                // Create a popup window for same domain popups
                WebPopupWindow *popup = new WebPopupWindow(
                    this->profile(), *pendingGeometry, m_parent);
                popup->view()->setUrl(url);
                popup->show();
              } else {
                QDesktopServices::openUrl(url);
              }

              // We've sent the URL to the browser, now clean up
              delete pendingGeometry;
              ghostPage->deleteLater();
            }
          });

  // Return the ghost page to the engine so it thinks it succeeded,
  // but the URL will immediately trigger our signal above.
  return ghostPage;
}
