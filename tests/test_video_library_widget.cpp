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
#include <QtEndian>
#include <cstring>

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
    QByteArray avi(88, '\0');
    std::memcpy(avi.data(), "RIFF", 4);
    std::memcpy(avi.data() + 8, "AVI ", 4);
    std::memcpy(avi.data() + 12, "avih", 4);
    qToLittleEndian<quint32>(56, reinterpret_cast<uchar *>(avi.data() + 16));
    qToLittleEndian<quint32>(33333, reinterpret_cast<uchar *>(avi.data() + 20));
    qToLittleEndian<quint32>(30, reinterpret_cast<uchar *>(avi.data() + 36));
    file.write(avi);
    return true;
  }

  bool writeInvalidBytes(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write("incomplete") > 0;
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

  void analysis_uses_checked_videos_then_selected_rows() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    AppPaths::setStorageRootDir(root.path());
    const QString first = QDir(AppPaths::recordingsDir()).filePath("中文录像.avi");
    const QString second = QDir(AppPaths::recordingsDir()).filePath("second.avi");
    QVERIFY(writeBytes(first));
    QVERIFY(writeBytes(second));
    VideoLibraryWidget widget;
    auto *table = widget.findChild<QTableWidget *>();
    QCOMPARE(table->rowCount(), 2);
    QSignalSpy requested(&widget, SIGNAL(analysisRequested(QStringList)));
    QVERIFY(requested.isValid());
    table->selectRow(1);
    table->item(0, 0)->setCheckState(Qt::Checked);
    const QString checkedPath = table->item(0, 0)->data(Qt::UserRole + 1).toString();
    QVERIFY(QMetaObject::invokeMethod(&widget, "requestAnalysis"));
    QCOMPARE(requested.count(), 1);
    QCOMPARE(requested.takeFirst().at(0).toStringList(), QStringList{checkedPath});
    table->item(0, 0)->setCheckState(Qt::Unchecked);
    const QString selectedPath = table->item(1, 0)->data(Qt::UserRole + 1).toString();
    QVERIFY(QMetaObject::invokeMethod(&widget, "requestAnalysis"));
    QCOMPARE(requested.takeFirst().at(0).toStringList(), QStringList{selectedPath});
  }

  void recording_blocks_analysis_and_preserves_pending_file() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    AppPaths::setStorageRootDir(root.path());
    const QString path = QDir(AppPaths::recordingsDir()).filePath("pending.avi");
    QVERIFY(writeBytes(path));
    VideoLibraryWidget widget;
    auto *table = widget.findChild<QTableWidget *>();
    table->selectRow(0);
    QSignalSpy requested(&widget, SIGNAL(analysisRequested(QStringList)));
    QVERIFY(requested.isValid());
    QVERIFY(QMetaObject::invokeMethod(&widget, "setCaptureBusy", Q_ARG(bool, true)));
    QFile pending(path);
    QVERIFY(pending.open(QIODevice::WriteOnly | QIODevice::Truncate));
    pending.close();
    widget.rescanAndRefresh();
    QVERIFY(QFileInfo::exists(path));
    QVERIFY(QMetaObject::invokeMethod(&widget, "requestAnalysis"));
    QCOMPARE(requested.count(), 0);
    QVERIFY(writeBytes(path));
    QVERIFY(QMetaObject::invokeMethod(&widget, "setCaptureBusy", Q_ARG(bool, false)));
    table->selectRow(0);
    QVERIFY(QMetaObject::invokeMethod(&widget, "requestAnalysis"));
    QCOMPARE(requested.count(), 1);
  }

  void analysis_rejects_incomplete_avi() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    AppPaths::setStorageRootDir(root.path());
    const QString path = QDir(AppPaths::recordingsDir()).filePath("incomplete.avi");
    QVERIFY(writeInvalidBytes(path));
    VideoLibraryWidget widget;
    auto *table = widget.findChild<QTableWidget *>();
    QCOMPARE(table->rowCount(), 1);
    table->selectRow(0);
    QSignalSpy requested(&widget, SIGNAL(analysisRequested(QStringList)));
    QVERIFY(QMetaObject::invokeMethod(&widget, "requestAnalysis"));
    QCOMPARE(requested.count(), 0);
  }
};

QTEST_MAIN(TestVideoLibraryWidget)
#include "test_video_library_widget.moc"
