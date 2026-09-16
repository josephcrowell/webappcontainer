// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "app/appcontroller.h"

#include <QApplication>
#include <QColor>
#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <QWindow>

class TestAppController : public QObject {
  Q_OBJECT

private slots:
  void restoresHiddenMaximizedWindow();
  void restoresHiddenNormalWindow();
  void repaintsQuickWindowAfterRestore();
  void rendersOriginalNotificationBadgeGeometry();
};

void TestAppController::restoresHiddenMaximizedWindow() {
  AppController controller;
  QWindow window;
  window.resize(640, 480);
  controller.setNotificationBadge(true);
  controller.restoreWindow(&window, true);
  QVERIFY(QTest::qWaitForWindowExposed(&window, 5000));
  QTRY_COMPARE_WITH_TIMEOUT(window.visibility(), QWindow::Maximized, 5000);
  QVERIFY(!controller.notificationBadge());
}

void TestAppController::restoresHiddenNormalWindow() {
  AppController controller;
  QWindow window;
  window.resize(640, 480);
  window.showMaximized();
  QVERIFY(QTest::qWaitForWindowExposed(&window, 5000));
  window.hide();
  QTRY_COMPARE_WITH_TIMEOUT(window.visibility(), QWindow::Hidden, 5000);
  controller.restoreWindow(&window, false);
  QTRY_COMPARE_WITH_TIMEOUT(window.visibility(), QWindow::Windowed, 5000);
}

void TestAppController::repaintsQuickWindowAfterRestore() {
  AppController controller;
  QQuickWindow window;
  const QColor expected(QStringLiteral("#2f80ed"));
  window.resize(160, 120);
  window.setColor(expected);
  window.setPersistentGraphics(true);
  window.setPersistentSceneGraph(true);
  window.show();
  QVERIFY(QTest::qWaitForWindowExposed(&window, 5000));
  QSignalSpy initialFrame(&window, &QQuickWindow::frameSwapped);
  window.update();
  QVERIFY(initialFrame.wait(5000) || !initialFrame.isEmpty());
  QVERIFY(!window.grabWindow().isNull());

  window.hide();
  QTRY_COMPARE_WITH_TIMEOUT(window.visibility(), QWindow::Hidden, 5000);
  QSignalSpy restoredFrame(&window, &QQuickWindow::frameSwapped);
  controller.restoreWindow(&window, false);
  QVERIFY(restoredFrame.wait(5000) || !restoredFrame.isEmpty());
  const QImage restored = window.grabWindow();
  QVERIFY(!restored.isNull());
  const QColor center = restored.pixelColor(restored.width() / 2,
                                             restored.height() / 2);
  QVERIFY2(center.red() > 20 || center.green() > 20 || center.blue() > 20,
           "Restored Quick window remained black");
}

void TestAppController::rendersOriginalNotificationBadgeGeometry() {
  QImage source(QSize(64, 64), QImage::Format_ARGB32_Premultiplied);
  source.fill(QColor(QStringLiteral("#2f80ed")));
  AppController controller;
  QSignalSpy iconSpy(&controller, &AppController::trayIconUrlChanged);
  controller.configureTrayIcon(QIcon(QPixmap::fromImage(source)));
  const QUrl normalUrl = controller.trayIconUrl();
  QVERIFY(normalUrl.isLocalFile());
  const QImage normal(normalUrl.toLocalFile());
  QCOMPARE(normal.size(), QSize(64, 64));
  QVERIFY(normal.pixelColor(52, 11).blue() > normal.pixelColor(52, 11).red());

  controller.setNotificationBadge(true);
  const QUrl badgedUrl = controller.trayIconUrl();
  QVERIFY(badgedUrl.isLocalFile());
  QVERIFY(badgedUrl != normalUrl);
  const QImage badged(badgedUrl.toLocalFile());
  QCOMPARE(badged.size(), QSize(64, 64));
  const QColor badgeCenter = badged.pixelColor(52, 11);
  QVERIFY(qAbs(badgeCenter.red() - 255) <= 2);
  QVERIFY(qAbs(badgeCenter.green() - 59) <= 2);
  QVERIFY(qAbs(badgeCenter.blue() - 48) <= 2);
  const QColor border = badged.pixelColor(41, 11);
  QVERIFY(border.red() > 240 && border.green() > 240 && border.blue() > 240);

  controller.setNotificationBadge(false);
  QCOMPARE(controller.trayIconUrl(), normalUrl);
  QCOMPARE(iconSpy.count(), 3);
}

QTEST_MAIN(TestAppController)
#include "tst_appcontroller.moc"
