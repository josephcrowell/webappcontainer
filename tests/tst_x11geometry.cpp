// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "app/settingsstore.h"

#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QTemporaryDir>
#include <QTest>
#include <QWindow>
#include <QtGui/qguiapplication_platform.h>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>

class TestX11Geometry : public QObject {
  Q_OBJECT

private slots:
  void restoredWindowPublishesUserPositionOnSavedScreen();
};

void TestX11Geometry::restoredWindowPublishesUserPositionOnSavedScreen() {
  if (QGuiApplication::platformName() != QStringLiteral("xcb"))
    QSKIP("Native WM_NORMAL_HINTS coverage requires X11/XCB");
  const QList<QScreen *> screens = QGuiApplication::screens();
  if (screens.size() < 2)
    QSKIP("Native screen-selection coverage requires two screens");
  QScreen *target = screens.at(1);
  const QRect available = target->availableGeometry();
  const QRect expected(available.topLeft() + QPoint(120, 80), QSize(720, 520));
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  {
    SettingsStore settings(directory.path());
    QWindow source(target);
    source.setGeometry(expected);
    settings.saveWindowGeometry(&source);
    QVERIFY(settings.sync());
  }

  SettingsStore restored(directory.path());
  QWindow window;
  restored.restoreWindowGeometry(&window);
  QCOMPARE(window.screen(), target);
  QCOMPARE(window.geometry(), expected);
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window, 5000));

  auto *native = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
  QVERIFY(native);
  xcb_size_hints_t hints{};
  const xcb_get_property_cookie_t cookie =
      xcb_icccm_get_wm_normal_hints(native->connection(), window.winId());
  QVERIFY(xcb_icccm_get_wm_normal_hints_reply(native->connection(), cookie,
                                              &hints, nullptr));
  QVERIFY2(hints.flags & XCB_ICCCM_SIZE_HINT_US_POSITION,
           "Restored window did not publish an explicit USPosition hint");
  QVERIFY2(target->geometry().contains(window.geometry().center()),
           "KWin placed the restored window outside its saved screen");
}

QTEST_MAIN(TestX11Geometry)
#include "tst_x11geometry.moc"
