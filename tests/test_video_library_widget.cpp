// VideoLibraryWidget 集成测试
// 验证视频库视图只展示当前保存目录下的录像。
#include "data/DatabaseManager.h"
#include "utils/AppPaths.h"
#include "widgets/VideoLibraryWidget.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QtTest>

class TestVideoLibraryWidget : public QObject {
  Q_OBJECT

private:
  void ensureSqlDriverPath() {
    const QString parent = QCoreApplication::applicationDirPath() + "/..";
    if (!QCoreApplication::libraryPaths().contains(parent)) {
      QCoreApplication::addLibraryPath(parent);
    }
  }

  VideoInfo makeVideo(const QString &path) {
    VideoInfo v;
    v.filename = QFileInfo(path).fileName();
    v.filepath = path;
    v.filesize = QFileInfo(path).size();
    v.createdAt = QDateTime::currentDateTime();
    return v;
  }

  bool writeBytes(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
      return false;
    }
    file.write("not-a-real-avi-but-non-empty");
    return true;
  }

private slots:
  void init() {
    ensureSqlDriverPath();
    DatabaseManager::instance().close();
    QVERIFY(DatabaseManager::instance().initialize(":memory:"));
    AppPaths::clearStorageRootDirForTest();
  }

  void cleanup() {
    DatabaseManager::instance().close();
    AppPaths::clearStorageRootDirForTest();
  }

  void refresh_shows_only_current_recordings_directory() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QDir rootDir(root.path());
    QVERIFY(rootDir.mkpath("current"));
    QVERIFY(rootDir.mkpath("old"));

    AppPaths::setStorageRootDir(rootDir.filePath("current"));
    const QString currentVideo =
        QDir(AppPaths::recordingsDir()).absoluteFilePath("current.avi");
    const QString oldVideo =
        rootDir.absoluteFilePath("old/recordings/old.avi");
    QVERIFY(QDir(rootDir.filePath("old")).mkpath("recordings"));
    QVERIFY(writeBytes(currentVideo));
    QVERIFY(writeBytes(oldVideo));

    QVERIFY(DatabaseManager::instance().upsertVideo(makeVideo(currentVideo)));
    QVERIFY(DatabaseManager::instance().upsertVideo(makeVideo(oldVideo)));

    VideoLibraryWidget widget;
    widget.rescanAndRefresh();

    auto *table = widget.findChild<QTableWidget *>();
    QVERIFY(table != nullptr);
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 1)->text(), QString("current.avi"));
  }
};

QTEST_MAIN(TestVideoLibraryWidget)
#include "test_video_library_widget.moc"
