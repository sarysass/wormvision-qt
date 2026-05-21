// AppInstanceLock 单元测试
// 防止两个 WormVision 进程同时启动并争抢同一台相机。
#include "utils/AppInstanceLock.h"

#include <QTemporaryDir>
#include <QtTest>

class TestAppInstanceLock : public QObject {
  Q_OBJECT

private slots:
  void second_lock_on_same_path_fails() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString lockPath = dir.filePath("WormVision.lock");

    AppInstanceLock first(lockPath);
    QVERIFY(first.tryLock());

    AppInstanceLock second(lockPath);
    QVERIFY(!second.tryLock());
  }
};

QTEST_GUILESS_MAIN(TestAppInstanceLock)
#include "test_app_instance_lock.moc"
