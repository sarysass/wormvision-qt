// 集成测试：VideoLibraryService
// 覆盖 Phase 4（录制完成自动入库）+ Phase 5（脏数据清理）防回归
#include "data/VideoLibraryService.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

// ============================================================================
// Helper：构造一个最小合法 AVI 文件（同 test_video_utils）
// ============================================================================
namespace {

QByteArray u32LE(quint32 v) {
  QByteArray r(4, '\0');
  r[0] = char(v & 0xFF);
  r[1] = char((v >> 8) & 0xFF);
  r[2] = char((v >> 16) & 0xFF);
  r[3] = char((v >> 24) & 0xFF);
  return r;
}

// 构造一个 microSecPerFrame=33333μs (30fps)、totalFrames 帧的最小 AVI 头
QByteArray makeAviBuffer(quint32 totalFrames) {
  QByteArray buf;
  buf.append("RIFF");
  buf.append(u32LE(1024));
  buf.append("AVI ");
  buf.append("LIST");
  buf.append(u32LE(192));
  buf.append("hdrl");
  buf.append("avih");
  buf.append(u32LE(56));
  buf.append(u32LE(33333));       // dwMicroSecPerFrame
  buf.append(u32LE(1000000));     // dwMaxBytesPerSec
  buf.append(u32LE(0));           // dwPaddingGranularity
  buf.append(u32LE(0));           // dwFlags
  buf.append(u32LE(totalFrames)); // dwTotalFrames
  buf.append(u32LE(0));           // dwInitialFrames
  while (buf.size() < 200) buf.append('\0');
  return buf;
}

bool writeAviFile(const QString &path, quint32 totalFrames) {
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly)) return false;
  f.write(makeAviBuffer(totalFrames));
  f.close();
  return true;
}

bool writeZeroByteFile(const QString &path) {
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly)) return false;
  f.close();
  return true;
}

} // namespace

class TestVideoLibraryService : public QObject {
  Q_OBJECT

private:
  void ensureSqlDriverPath() {
    const QString parent = QCoreApplication::applicationDirPath() + "/..";
    if (!QCoreApplication::libraryPaths().contains(parent)) {
      QCoreApplication::addLibraryPath(parent);
    }
  }

  void resetDb() {
    ensureSqlDriverPath();
    DatabaseManager::instance().close();
    QVERIFY(DatabaseManager::instance().initialize(":memory:"));
  }

private slots:

  // ============================================================================
  // Phase 4 防回归：addRecording
  // ============================================================================

  void addRecording_valid_avi_writes_to_db() {
    resetDb();
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("test.avi");
    QVERIFY(writeAviFile(path, 300)); // 300 帧 @ 30fps ≈ 10 秒

    bool ok = VideoLibraryService::addRecording(
        path, DatabaseManager::instance());
    QVERIFY(ok);

    auto videos = DatabaseManager::instance().getAllVideos();
    QCOMPARE(videos.size(), 1);
    QCOMPARE(videos[0].filename, QString("test.avi"));
    QVERIFY(videos[0].duration >= 9 && videos[0].duration <= 11);
    QVERIFY(videos[0].filesize > 0);
  }

  void addRecording_zero_byte_file_skipped() {
    // Phase 5 关键防回归：海康 SDK 失败时会留下 0 字节文件，不应入库
    resetDb();
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("bad.avi");
    QVERIFY(writeZeroByteFile(path));

    bool ok = VideoLibraryService::addRecording(
        path, DatabaseManager::instance());
    QVERIFY(!ok);
    QCOMPARE(DatabaseManager::instance().getAllVideos().size(), 0);
  }

  void addRecording_nonexistent_file_skipped() {
    resetDb();
    bool ok = VideoLibraryService::addRecording(
        "Z:/no/such/file.avi", DatabaseManager::instance());
    QVERIFY(!ok);
    QCOMPARE(DatabaseManager::instance().getAllVideos().size(), 0);
  }

  void addRecording_called_twice_same_file_no_duplicate() {
    // 不应产生重复记录（依赖 DatabaseManager.upsertVideo 行为）
    resetDb();
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("dup.avi");
    QVERIFY(writeAviFile(path, 60));

    QVERIFY(VideoLibraryService::addRecording(path,
                                              DatabaseManager::instance()));
    QVERIFY(VideoLibraryService::addRecording(path,
                                              DatabaseManager::instance()));
    QCOMPARE(DatabaseManager::instance().getAllVideos().size(), 1);
  }

  // ============================================================================
  // Phase 5 防回归：pruneOrphans
  // ============================================================================

  void pruneOrphans_removes_zero_byte_records() {
    resetDb();
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // 准备一个 0 字节文件
    QString zeroPath = dir.filePath("zero.avi");
    QVERIFY(writeZeroByteFile(zeroPath));

    // 手工插入 DB（绕过 addRecording 的 0 字节防护）
    VideoInfo bad;
    bad.filename = "zero.avi";
    bad.filepath = zeroPath;
    bad.filesize = 0;
    QVERIFY(DatabaseManager::instance().insertVideo(bad) > 0);

    QCOMPARE(DatabaseManager::instance().getAllVideos().size(), 1);
    int pruned = VideoLibraryService::pruneOrphans(DatabaseManager::instance());
    QCOMPARE(pruned, 1);
    QCOMPARE(DatabaseManager::instance().getAllVideos().size(), 0);
    // 0 字节文件本身也应该被删了
    QVERIFY(!QFile::exists(zeroPath));
  }

  void pruneOrphans_removes_missing_file_records() {
    resetDb();
    // 直接插入一个 path 指向不存在文件的 DB 记录
    VideoInfo missing;
    missing.filename = "gone.avi";
    missing.filepath = "Z:/no/such/path/gone.avi";
    missing.filesize = 12345;
    QVERIFY(DatabaseManager::instance().insertVideo(missing) > 0);

    int pruned = VideoLibraryService::pruneOrphans(DatabaseManager::instance());
    QCOMPARE(pruned, 1);
    QCOMPARE(DatabaseManager::instance().getAllVideos().size(), 0);
  }

  void pruneOrphans_keeps_valid_records() {
    resetDb();
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString validPath = dir.filePath("good.avi");
    QVERIFY(writeAviFile(validPath, 60));

    QVERIFY(VideoLibraryService::addRecording(validPath,
                                              DatabaseManager::instance()));

    int pruned = VideoLibraryService::pruneOrphans(DatabaseManager::instance());
    QCOMPARE(pruned, 0);
    QCOMPARE(DatabaseManager::instance().getAllVideos().size(), 1);
  }

  void pruneOrphans_mixed_keeps_only_valid() {
    // 综合场景：3 个有效 + 1 个 0 字节 + 2 个不存在 → 只剩 3 个
    resetDb();
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // 3 个有效 AVI
    for (int i = 1; i <= 3; ++i) {
      QString p = dir.filePath(QString("good_%1.avi").arg(i));
      QVERIFY(writeAviFile(p, 60));
      QVERIFY(VideoLibraryService::addRecording(p,
                                                DatabaseManager::instance()));
    }

    // 1 个 0 字节（手工插，绕过防护）
    QString zeroPath = dir.filePath("zero.avi");
    QVERIFY(writeZeroByteFile(zeroPath));
    {
      VideoInfo bad;
      bad.filename = "zero.avi";
      bad.filepath = zeroPath;
      bad.filesize = 0;
      QVERIFY(DatabaseManager::instance().insertVideo(bad) > 0);
    }

    // 2 个不存在（直接插）
    for (int i = 1; i <= 2; ++i) {
      VideoInfo missing;
      missing.filename = QString("gone_%1.avi").arg(i);
      missing.filepath = QString("Z:/no/such/gone_%1.avi").arg(i);
      missing.filesize = 1234;
      QVERIFY(DatabaseManager::instance().insertVideo(missing) > 0);
    }

    QCOMPARE(DatabaseManager::instance().getAllVideos().size(), 6);
    int pruned = VideoLibraryService::pruneOrphans(DatabaseManager::instance());
    QCOMPARE(pruned, 3);
    QCOMPARE(DatabaseManager::instance().getAllVideos().size(), 3);
  }

  // ============================================================================
  // 集成：addRecording → pruneOrphans 后台修复故障场景
  // ============================================================================

  void scenario_recording_failed_then_user_retried() {
    // 模拟用户的真实情景：
    //   1) 第一次录制失败留 0 字节（被 addRecording 拒）
    //   2) 用户重启 App，refreshLibrary 时 pruneOrphans 应该删 0 字节文件
    //   3) 再次录制（同名）成功
    resetDb();
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString path = dir.filePath("retry.avi");

    // 1) 旧的失败遗留 + 旧的 DB 记录
    QVERIFY(writeZeroByteFile(path));
    VideoInfo old;
    old.filename = "retry.avi";
    old.filepath = path;
    old.filesize = 0;
    QVERIFY(DatabaseManager::instance().insertVideo(old) > 0);

    // 2) refreshLibrary 触发 pruneOrphans
    QCOMPARE(VideoLibraryService::pruneOrphans(DatabaseManager::instance()), 1);
    QCOMPARE(DatabaseManager::instance().getAllVideos().size(), 0);
    QVERIFY(!QFile::exists(path));

    // 3) 重新录制（产生有效文件）
    QVERIFY(writeAviFile(path, 90));
    QVERIFY(VideoLibraryService::addRecording(path,
                                              DatabaseManager::instance()));

    auto videos = DatabaseManager::instance().getAllVideos();
    QCOMPARE(videos.size(), 1);
    QVERIFY(videos[0].filesize > 0);
    QVERIFY(videos[0].duration >= 2 && videos[0].duration <= 4); // 90帧/30fps=3s
  }
};

QTEST_GUILESS_MAIN(TestVideoLibraryService)
#include "test_video_library_service.moc"
