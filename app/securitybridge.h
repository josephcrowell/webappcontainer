// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QWebEngineCertificateError>
#include <QWebEngineFileSystemAccessRequest>
#include <QWebEnginePermission>
#include <QWebEngineWebAuthUxRequest>

class SecurityBridge : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("SecurityBridge is created by the application")

public:
  using QObject::QObject;
  Q_INVOKABLE bool certificateIsMainFrame(
      const QWebEngineCertificateError &error) const {
    return error.isMainFrame();
  }
  Q_INVOKABLE QString permissionQuestion(const QWebEnginePermission &permission) const;
  Q_INVOKABLE QString fileSystemAccessQuestion(
      const QWebEngineFileSystemAccessRequest &request) const;
  Q_INVOKABLE QString webAuthPinError(QWebEngineWebAuthUxRequest *request) const;
  Q_INVOKABLE QString webAuthFailure(QWebEngineWebAuthUxRequest *request) const;
  Q_INVOKABLE bool webAuthCanRetry(QWebEngineWebAuthUxRequest *request) const;
};
