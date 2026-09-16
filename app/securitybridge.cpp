// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "securitybridge.h"

QString SecurityBridge::permissionQuestion(
    const QWebEnginePermission &permission) const {
  QString question;
  switch (permission.permissionType()) {
  case QWebEnginePermission::PermissionType::Geolocation:
    question = tr("Allow %1 to access your location information?"); break;
  case QWebEnginePermission::PermissionType::MediaAudioCapture:
    question = tr("Allow %1 to access your microphone?"); break;
  case QWebEnginePermission::PermissionType::MediaVideoCapture:
    question = tr("Allow %1 to access your webcam?"); break;
  case QWebEnginePermission::PermissionType::MediaAudioVideoCapture:
    question = tr("Allow %1 to access your microphone and webcam?"); break;
  case QWebEnginePermission::PermissionType::MouseLock:
    question = tr("Allow %1 to lock your mouse cursor?"); break;
  case QWebEnginePermission::PermissionType::DesktopVideoCapture:
    question = tr("Allow %1 to capture video of your desktop?"); break;
  case QWebEnginePermission::PermissionType::DesktopAudioVideoCapture:
    question = tr("Allow %1 to capture audio and video of your desktop?"); break;
  case QWebEnginePermission::PermissionType::Notifications:
    question = tr("Allow %1 to show notification on your desktop?"); break;
  case QWebEnginePermission::PermissionType::ClipboardReadWrite:
    question = tr("Allow %1 to read from and write to the clipboard?"); break;
  case QWebEnginePermission::PermissionType::LocalFontsAccess:
    question = tr("Allow %1 to access fonts stored on this machine?"); break;
  case QWebEnginePermission::PermissionType::Unsupported:
    return {};
  }
  return question.arg(permission.origin().host());
}

QString SecurityBridge::fileSystemAccessQuestion(
    const QWebEngineFileSystemAccessRequest &request) const {
  QString access;
  if (request.accessFlags() == QWebEngineFileSystemAccessRequest::Read)
    access = tr("read");
  else if (request.accessFlags() == QWebEngineFileSystemAccessRequest::Write)
    access = tr("write");
  else if (request.accessFlags() == (QWebEngineFileSystemAccessRequest::Read |
                                     QWebEngineFileSystemAccessRequest::Write))
    access = tr("read and write");
  else
    access = tr("requested");
  return tr("Give %1 %2 access to %3?")
      .arg(request.origin().host(), access, request.filePath().toString());
}

QString SecurityBridge::webAuthPinError(QWebEngineWebAuthUxRequest *request) const {
  if (!request)
    return {};
  const QWebEngineWebAuthPinRequest pin = request->pinRequest();
  QString error;
  switch (pin.error) {
  case QWebEngineWebAuthUxRequest::PinEntryError::NoError: return {};
  case QWebEngineWebAuthUxRequest::PinEntryError::InternalUvLocked:
    error = tr("Internal User Verification Locked"); break;
  case QWebEngineWebAuthUxRequest::PinEntryError::WrongPin:
    error = tr("Wrong PIN"); break;
  case QWebEngineWebAuthUxRequest::PinEntryError::TooShort:
    error = tr("Too Short"); break;
  case QWebEngineWebAuthUxRequest::PinEntryError::InvalidCharacters:
    error = tr("Invalid Characters"); break;
  case QWebEngineWebAuthUxRequest::PinEntryError::SameAsCurrentPin:
    error = tr("Same as current PIN"); break;
  }
  return tr("%1 %2 attempts remaining").arg(error).arg(pin.remainingAttempts);
}

QString SecurityBridge::webAuthFailure(QWebEngineWebAuthUxRequest *request) const {
  if (!request)
    return {};
  using Reason = QWebEngineWebAuthUxRequest::RequestFailureReason;
  switch (request->requestFailureReason()) {
  case Reason::Timeout: return tr("Request Timeout");
  case Reason::KeyNotRegistered: return tr("Key not registered");
  case Reason::KeyAlreadyRegistered:
    return tr("You already registered this device. Try again with device");
  case Reason::SoftPinBlock:
    return tr("The security key is locked because the wrong PIN was entered too many times. To unlock it, remove and reinsert it.");
  case Reason::HardPinBlock:
    return tr("The security key is locked because the wrong PIN was entered too many times. You'll need to reset the security key.");
  case Reason::AuthenticatorRemovedDuringPinEntry:
    return tr("Authenticator removed during verification. Please reinsert and try again");
  case Reason::AuthenticatorMissingResidentKeys:
    return tr("Authenticator doesn't have resident key support");
  case Reason::AuthenticatorMissingUserVerification:
    return tr("Authenticator missing user verification");
  case Reason::AuthenticatorMissingLargeBlob:
    return tr("Authenticator missing Large Blob support");
  case Reason::NoCommonAlgorithms: return tr("No common algorithms");
  case Reason::StorageFull: return tr("Storage Full");
  case Reason::UserConsentDenied: return tr("User consent denied");
  case Reason::WinUserCancelled: return tr("User Cancelled Request");
  }
  return {};
}

bool SecurityBridge::webAuthCanRetry(QWebEngineWebAuthUxRequest *request) const {
  if (!request)
    return false;
  using Reason = QWebEngineWebAuthUxRequest::RequestFailureReason;
  return request->requestFailureReason() == Reason::KeyAlreadyRegistered ||
         request->requestFailureReason() == Reason::SoftPinBlock;
}
