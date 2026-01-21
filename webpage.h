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
  explicit WebPage(QWebEngineProfile *profile, QWidget *parent = nullptr);

signals:
  void createCertificateErrorDialog(QWebEngineCertificateError error);

private:
  QString getBaseDomain(const QString &host);

private slots:
  void handleCertificateError(QWebEngineCertificateError error);
  void handleSelectClientCertificate(
      QWebEngineClientCertificateSelection clientCertSelection);
  void handleDesktopMediaRequest(const QWebEngineDesktopMediaRequest &request);

protected:
  QWebEnginePage *createWindow(WebWindowType type) override;
  QWidget *m_parent;
};

#endif // WEBPAGE_H
