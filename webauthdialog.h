// Copyright(C) 2026 Joseph Crowell.
// SPDX - License - Identifier : GPL-2.0-or-later

#ifndef WEBAUTHDIALOG_H
#define WEBAUTHDIALOG_H

#include "qwebenginewebauthuxrequest.h"
#include "ui_webauthdialog.h"
#include <QButtonGroup>
#include <QDialog>
#include <QScrollArea>

class WebAuthDialog : public QDialog {
  Q_OBJECT
public:
  WebAuthDialog(QWebEngineWebAuthUxRequest *request, QWidget *parent = nullptr);
  ~WebAuthDialog();

  void updateDisplay();

private:
  QWebEngineWebAuthUxRequest *uxRequest;
  QButtonGroup *buttonGroup = nullptr;
  QScrollArea *scrollArea = nullptr;
  QWidget *selectAccountWidget = nullptr;
  QVBoxLayout *selectAccountLayout = nullptr;

  void setupSelectAccountUI();
  void setupCollectPinUI();
  void setupFinishCollectTokenUI();
  void setupErrorUI();
  void onCancelRequest();
  void onRetry();
  void onAcceptRequest();
  void clearSelectAccountButtons();

  Ui::WebAuthDialog *uiWebAuthDialog;
};

#endif // WEBAUTHDIALOG_H
