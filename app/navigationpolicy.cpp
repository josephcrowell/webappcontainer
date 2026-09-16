// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "navigationpolicy.h"

#include "publicsuffixlist.h"

#include <QDesktopServices>
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>

bool NavigationPolicy::isFacebookHost(const QString &host) {
  const QString lower = host.toLower();
  const auto isHostOrSubdomain = [&lower](const QString &domain) {
    return lower == domain || lower.endsWith(QLatin1Char('.') + domain);
  };
  return isHostOrSubdomain(QStringLiteral("facebook.com")) ||
         isHostOrSubdomain(QStringLiteral("messenger.com"));
}

bool NavigationPolicy::shouldOpenInApplication(const QUrl &currentUrl,
                                               const QUrl &requestedUrl) const {
  if (!requestedUrl.isValid() || requestedUrl == QUrl(QStringLiteral("about:blank")) ||
      requestedUrl.host().isEmpty())
    return true;

  const QString path = requestedUrl.path().toLower();
  if (isFacebookHost(currentUrl.host()) && isFacebookHost(requestedUrl.host()) &&
      (path.startsWith(QStringLiteral("/groupcall/")) ||
       path.startsWith(QStringLiteral("/settings/")))) {
    return true;
  }
  return PublicSuffixList::instance()->isSameDomain(currentUrl.host(),
                                                     requestedUrl.host());
}

bool NavigationPolicy::openExternal(const QUrl &url) const {
  if (!url.isValid())
    return false;
  const QString scheme = url.scheme().toLower();
  if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))
    return false;
  return QDesktopServices::openUrl(url);
}

QRect NavigationPolicy::availableGeometry(QWindow *window) const {
  QScreen *screen = window ? window->screen() : QGuiApplication::primaryScreen();
  return screen ? screen->availableGeometry() : QRect();
}
