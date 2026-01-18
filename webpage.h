// Copyright(C) 2026 Joseph Crowell.
// SPDX - License - Identifier : GPL-2.0-or-later

#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <QWebEngineCertificateError>
#include <QWebEngineDesktopMediaRequest>
#include <QWebEnginePage>
#include <QWebEngineRegisterProtocolHandlerRequest>

class WebPage : public QWebEnginePage {
  Q_OBJECT

public:
  explicit WebPage(QWebEngineProfile *profile, QObject *parent = nullptr);

signals:
  void createCertificateErrorDialog(QWebEngineCertificateError error);

private slots:
  void handleCertificateError(QWebEngineCertificateError error);
  void handleSelectClientCertificate(
      QWebEngineClientCertificateSelection clientCertSelection);
  void handleDesktopMediaRequest(const QWebEngineDesktopMediaRequest &request);

protected:
  bool acceptNavigationRequest(const QUrl &url, NavigationType type,
                               bool isMainFrame) override;
  QWebEnginePage *createWindow(WebWindowType type) override;
};

#endif // WEBPAGE_H
