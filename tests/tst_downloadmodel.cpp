// Copyright(C) 2026 Joseph Crowell.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "app/downloadmodel.h"

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTest>

class TestDownloadModel : public QObject {
  Q_OBJECT

private slots:
  void rolesAndProgress();
  void cancelAndRemoveByStableId();
  void invalidOperationsAreNoOps();
  void removingInterruptedTransferRequestsCancellation();
};

void TestDownloadModel::rolesAndProgress() {
  DownloadModel model;
  QAbstractItemModelTester tester(&model,
                                  QAbstractItemModelTester::FailureReportingMode::QtTest);
  DownloadModel::Item item;
  item.fileName = QStringLiteral("archive.bin");
  item.receivedBytes = 25;
  item.totalBytes = 100;
  item.state = DownloadModel::InProgress;
  const quint64 id = model.append(item);
  QCOMPARE(model.rowCount(), 1);
  const QModelIndex index = model.index(0);
  QCOMPARE(model.data(index, DownloadModel::IdRole).toULongLong(), id);
  QCOMPARE(model.data(index, DownloadModel::ProgressRole).toDouble(), 0.25);
  QCOMPARE(model.data(index, DownloadModel::IndeterminateRole).toBool(), false);

  item.totalBytes = -1;
  QVERIFY(model.update(id, item));
  QCOMPARE(model.data(index, DownloadModel::IndeterminateRole).toBool(), true);
  QCOMPARE(model.rowCount(model.index(0)), 0);
}

void TestDownloadModel::cancelAndRemoveByStableId() {
  DownloadModel model;
  DownloadModel::Item first;
  first.fileName = QStringLiteral("first");
  first.state = DownloadModel::InProgress;
  DownloadModel::Item second = first;
  second.fileName = QStringLiteral("second");
  const quint64 firstId = model.append(first);
  const quint64 secondId = model.append(second);
  QSignalSpy cancelSpy(&model, &DownloadModel::cancelRequested);
  model.cancel(secondId);
  QCOMPARE(cancelSpy.count(), 1);
  QCOMPARE(model.data(model.index(1), DownloadModel::StateRole).toInt(),
           int(DownloadModel::Cancelled));
  model.remove(secondId);
  QCOMPARE(model.rowCount(), 1);
  QCOMPARE(model.data(model.index(0), DownloadModel::IdRole).toULongLong(), firstId);
}

void TestDownloadModel::invalidOperationsAreNoOps() {
  DownloadModel model;
  DownloadModel::Item item;
  item.state = DownloadModel::InProgress;
  const quint64 id = model.append(item);
  model.remove(id);
  QCOMPARE(model.rowCount(), 1);
  model.cancel(9999);
  QCOMPARE(model.rowCount(), 1);
  QVERIFY(!model.data(QModelIndex(), DownloadModel::FileNameRole).isValid());
}

void TestDownloadModel::removingInterruptedTransferRequestsCancellation() {
  DownloadModel model;
  DownloadModel::Item item;
  item.state = DownloadModel::Interrupted;
  item.finished = false;
  const quint64 id = model.append(item);
  QSignalSpy cancelSpy(&model, &DownloadModel::cancelRequested);
  model.remove(id);
  QCOMPARE(cancelSpy.count(), 1);
  QCOMPARE(cancelSpy.constFirst().constFirst().toULongLong(), id);
  QCOMPARE(model.rowCount(), 0);
}

QTEST_APPLESS_MAIN(TestDownloadModel)
#include "tst_downloadmodel.moc"
