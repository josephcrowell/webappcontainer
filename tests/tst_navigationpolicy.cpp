// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "app/navigationpolicy.h"

#include <QTest>

class TestNavigationPolicy : public QObject {
  Q_OBJECT

private slots:
  void sameRegistrableDomain();
  void differentDomain();
  void facebookSpecialCases();
  void blankRequestStaysInApplication();
};

void TestNavigationPolicy::sameRegistrableDomain() {
  NavigationPolicy policy;
  QVERIFY(policy.shouldOpenInApplication(QUrl(QStringLiteral("https://app.example.com/a")),
                                         QUrl(QStringLiteral("https://login.example.com/b"))));
}

void TestNavigationPolicy::differentDomain() {
  NavigationPolicy policy;
  QVERIFY(!policy.shouldOpenInApplication(QUrl(QStringLiteral("https://example.com")),
                                          QUrl(QStringLiteral("https://example.net"))));
}

void TestNavigationPolicy::facebookSpecialCases() {
  NavigationPolicy policy;
  QVERIFY(policy.shouldOpenInApplication(
      QUrl(QStringLiteral("https://www.facebook.com")),
      QUrl(QStringLiteral("https://www.messenger.com/groupcall/123"))));
  QVERIFY(!policy.shouldOpenInApplication(
      QUrl(QStringLiteral("https://notfacebook.com")),
      QUrl(QStringLiteral("https://evilfacebook.com/groupcall/123"))));
}

void TestNavigationPolicy::blankRequestStaysInApplication() {
  NavigationPolicy policy;
  QVERIFY(policy.shouldOpenInApplication(QUrl(QStringLiteral("https://example.com")),
                                         QUrl(QStringLiteral("about:blank"))));
  QVERIFY(policy.shouldOpenInApplication(QUrl(QStringLiteral("https://example.com")),
                                         QUrl()));
}

QTEST_APPLESS_MAIN(TestNavigationPolicy)
#include "tst_navigationpolicy.moc"
