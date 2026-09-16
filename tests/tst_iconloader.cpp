// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "app/iconloader.h"

#include <QApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QWindow>

class TestIconLoader : public QObject {
  Q_OBJECT

private slots:
  void loadsSvgForApplicationAndWindow();
  void rejectsMissingAndMalformedIcons();
};

void TestIconLoader::loadsSvgForApplicationAndWindow() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("application.svg"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  file.write(R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64">
    <rect width="64" height="64" fill="#1a73e8"/>
  </svg>)SVG");
  file.close();

  const QIcon icon = IconLoader::load(path);
  QVERIFY(!icon.isNull());
  QVERIFY(!icon.pixmap(64, 64).isNull());
  QApplication::setWindowIcon(icon);
  QWindow window;
  window.setIcon(QApplication::windowIcon());
  window.show();
  QVERIFY2(QTest::qWaitForWindowExposed(&window, 5000),
           "Native test window was not exposed");
  QVERIFY(!window.icon().isNull());
  QCOMPARE(window.icon().cacheKey(), QApplication::windowIcon().cacheKey());
}

void TestIconLoader::rejectsMissingAndMalformedIcons() {
  QVERIFY(IconLoader::load(QStringLiteral("/missing/icon.svg")).isNull());
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("broken.svg"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  file.write("not an svg");
  file.close();
  QVERIFY(IconLoader::load(path).isNull());
}

QTEST_MAIN(TestIconLoader)
#include "tst_iconloader.moc"
