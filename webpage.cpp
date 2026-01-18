// Copyright(C) 2026 Joseph Crowell.
// SPDX - License - Identifier : GPL-2.0-or-later

#include "webpage.h"
#include "webview.h"
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
