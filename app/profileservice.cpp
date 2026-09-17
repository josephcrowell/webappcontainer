// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "profileservice.h"

#include "launchconfiguration.h"
#include "mediadevicesalt.h"

#include <QDir>
#include <QQuickWebEngineProfile>
#include <QWebEngineCookieStore>

ProfileService::ProfileService(const LaunchConfiguration &configuration,
                               QObject *parent)
    : QObject(parent) {
  if (!QDir().mkpath(configuration.profilePath()))
    qWarning() << "Could not create profile storage directory";
  if (!MediaDeviceSalt::ensure(configuration.profilePath()))
    qWarning() << "Could not persist the media device identity salt";
  m_profile = new QQuickWebEngineProfile(configuration.profileName(), this);
  m_profile->setPersistentStoragePath(configuration.profilePath());
  m_profile->setCachePath(configuration.profilePath() + QDir::separator() +
                          QStringLiteral("cache"));
  m_profile->setPersistentCookiesPolicy(
      QQuickWebEngineProfile::AllowPersistentCookies);
  m_profile->setPersistentPermissionsPolicy(
      QQuickWebEngineProfile::PersistentPermissionsPolicy::StoreOnDisk);
  m_profile->setPushServiceEnabled(true);
  m_profile->setHttpUserAgent(QStringLiteral(
      "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
      "Chrome/143.0.7499.169/170 Safari/537.36"));
  QWebEngineCookieStore *cookieStore = m_profile->cookieStore();
  cookieStore->setCookieFilter(
      [](const QWebEngineCookieStore::FilterRequest &) { return true; });
  cookieStore->loadAllCookies();
  if (m_profile->isOffTheRecord())
    qWarning() << "Named WebEngine profile unexpectedly remains off the record";
}
