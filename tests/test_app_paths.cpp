// AppPaths 单元测试
// 验证用户自定义保存根目录后，录像与抓拍目录的派生规则。
#include "utils/AppPaths.h"

#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

class TestAppPaths : public QObject {
  Q_OBJECT

private slots:
  void init() {
    AppPaths::clearStorageRootDirForTest();
  }

  void cleanup() {
    AppPaths::clearStorageRootDirForTest();
  }

  void custom_storage_root_puts_snapshots_under_recordings() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    AppPaths::setStorageRootDir(dir.path());

    const QString expectedRecordings =
        QDir(dir.path()).absoluteFilePath("recordings");
    const QString expectedSnapshots =
        QDir(expectedRecordings).absoluteFilePath("snapshots");

    QCOMPARE(QDir::cleanPath(AppPaths::storageRootDir()),
             QDir::cleanPath(dir.path()));
    QCOMPARE(QDir::cleanPath(AppPaths::recordingsDir()),
             QDir::cleanPath(expectedRecordings));
    QCOMPARE(QDir::cleanPath(AppPaths::snapshotsDir()),
             QDir::cleanPath(expectedSnapshots));
    QVERIFY(QDir(expectedRecordings).exists());
    QVERIFY(QDir(expectedSnapshots).exists());
  }
};

QTEST_GUILESS_MAIN(TestAppPaths)
#include "test_app_paths.moc"
