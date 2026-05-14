// Phase 2 - DatabaseManager 单元测试
//
// 测试约束：DatabaseManager 是单例。每个 test slot 之间通过 close()
// 重置状态。所有用例都用 ":memory:" 数据库。
#include "data/DatabaseManager.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QtTest>

class TestDatabaseManager : public QObject {
  Q_OBJECT

private:
  // Qt SQL driver (qsqlite.dll) 由 windeployqt 部署在 build/sqldrivers/，
  // 而测试 exe 在 build/tests/。把父目录加进 library path 让 Qt 找到驱动。
  void ensureSqlDriverPath() {
    const QString parent = QCoreApplication::applicationDirPath() + "/..";
    if (!QCoreApplication::libraryPaths().contains(parent)) {
      QCoreApplication::addLibraryPath(parent);
    }
  }
  // 每个测试用例都重新打开 :memory: DB，保证隔离
  void resetDb() {
    ensureSqlDriverPath();
    DatabaseManager::instance().close();
    QVERIFY(DatabaseManager::instance().initialize(":memory:"));
  }

  VideoInfo makeSampleVideo(const QString &path, qint64 duration = 30,
                            qint64 size = 1024 * 1024) {
    VideoInfo v;
    v.filename = QFileInfo(path).fileName();
    v.filepath = path;
    v.duration = duration;
    v.filesize = size;
    v.createdAt = QDateTime::currentDateTime();
    v.uploadStatus = "NONE";
    return v;
  }

private slots:

  void initialize_memory_db_succeeds() {
    ensureSqlDriverPath();
    DatabaseManager::instance().close();
    QVERIFY(DatabaseManager::instance().initialize(":memory:"));
    QVERIFY(DatabaseManager::instance().isDatabaseOpen());
  }

  // ========== 插入 ==========
  void insertVideo_returns_positive_id() {
    resetDb();
    int id = DatabaseManager::instance().insertVideo(
        makeSampleVideo("/tmp/a.avi"));
    QVERIFY(id > 0);
  }

  void insertVideo_distinct_paths_get_different_ids() {
    resetDb();
    int id1 = DatabaseManager::instance().insertVideo(
        makeSampleVideo("/tmp/a.avi"));
    int id2 = DatabaseManager::instance().insertVideo(
        makeSampleVideo("/tmp/b.avi"));
    QVERIFY(id1 > 0);
    QVERIFY(id2 > 0);
    QVERIFY(id1 != id2);
  }

  // 修复 #7：原本 INSERT 重复 path 报 qWarning 且无法区分"已存在"和"真出错"
  // 新行为：重复路径插入返回 -1 但不打 warning，调用方据此决定 update 或忽略
  void insertVideo_duplicate_path_returns_negative() {
    resetDb();
    int id1 = DatabaseManager::instance().insertVideo(
        makeSampleVideo("/tmp/same.avi"));
    int id2 = DatabaseManager::instance().insertVideo(
        makeSampleVideo("/tmp/same.avi"));
    QVERIFY(id1 > 0);
    QCOMPARE(id2, -1);
  }

  // ========== 查询 ==========
  void getVideoById_after_insert_returns_same_data() {
    resetDb();
    VideoInfo original =
        makeSampleVideo("/tmp/c.avi", /*duration*/ 120, /*size*/ 5 * 1024 * 1024);
    int id = DatabaseManager::instance().insertVideo(original);
    QVERIFY(id > 0);

    VideoInfo got = DatabaseManager::instance().getVideoById(id);
    QCOMPARE(got.id, id);
    QCOMPARE(got.filename, original.filename);
    QCOMPARE(got.filepath, original.filepath);
    QCOMPARE(got.duration, original.duration);
    QCOMPARE(got.filesize, original.filesize);
  }

  void getVideoById_nonexistent_returns_default() {
    resetDb();
    VideoInfo got = DatabaseManager::instance().getVideoById(99999);
    QCOMPARE(got.id, -1);
  }

  void getAllVideos_returns_inserted_count() {
    resetDb();
    DatabaseManager::instance().insertVideo(makeSampleVideo("/tmp/1.avi"));
    DatabaseManager::instance().insertVideo(makeSampleVideo("/tmp/2.avi"));
    DatabaseManager::instance().insertVideo(makeSampleVideo("/tmp/3.avi"));
    QCOMPARE(DatabaseManager::instance().getAllVideos().size(), 3);
  }

  // ========== 更新 ==========
  void updateVideoDuration_persists() {
    resetDb();
    int id = DatabaseManager::instance().insertVideo(
        makeSampleVideo("/tmp/u.avi", /*duration*/ 30));
    QVERIFY(DatabaseManager::instance().updateVideoDuration(id, 999));
    QCOMPARE(DatabaseManager::instance().getVideoById(id).duration,
             qint64(999));
  }

  void updateVideoMetadataByPath_updates_both_fields() {
    resetDb();
    int id = DatabaseManager::instance().insertVideo(
        makeSampleVideo("/tmp/m.avi", 0, 0));
    QVERIFY(DatabaseManager::instance().updateVideoMetadataByPath(
        "/tmp/m.avi", 60, 2048));

    VideoInfo got = DatabaseManager::instance().getVideoById(id);
    QCOMPARE(got.duration, qint64(60));
    QCOMPARE(got.filesize, qint64(2048));
  }

  void updateVideoFilename_persists() {
    resetDb();
    int id = DatabaseManager::instance().insertVideo(
        makeSampleVideo("/tmp/r.avi"));
    QVERIFY(DatabaseManager::instance().updateVideoFilename(id, "renamed.avi"));
    QCOMPARE(DatabaseManager::instance().getVideoById(id).filename,
             QString("renamed.avi"));
  }

  // ========== 删除 ==========
  void deleteVideo_removes_record() {
    resetDb();
    int id = DatabaseManager::instance().insertVideo(
        makeSampleVideo("/tmp/d.avi"));
    QVERIFY(DatabaseManager::instance().deleteVideo(id));
    QCOMPARE(DatabaseManager::instance().getVideoById(id).id, -1);
  }

  void deleteVideo_then_reinsert_same_path_works() {
    // 删除后允许重新插入同样 filepath
    resetDb();
    int id1 = DatabaseManager::instance().insertVideo(
        makeSampleVideo("/tmp/x.avi"));
    QVERIFY(DatabaseManager::instance().deleteVideo(id1));
    int id2 = DatabaseManager::instance().insertVideo(
        makeSampleVideo("/tmp/x.avi"));
    QVERIFY(id2 > 0);
  }

  // ========== upsert：新增便捷方法测试 ==========
  // 修复 #1 的基础：录制完成后需要一次性 upsert
  void upsertVideo_inserts_when_not_exist() {
    resetDb();
    bool ok = DatabaseManager::instance().upsertVideo(
        makeSampleVideo("/tmp/n.avi", 10, 1024));
    QVERIFY(ok);
    QCOMPARE(DatabaseManager::instance().getAllVideos().size(), 1);
  }

  void upsertVideo_updates_when_exists() {
    resetDb();
    DatabaseManager::instance().insertVideo(
        makeSampleVideo("/tmp/n.avi", 0, 0));
    bool ok = DatabaseManager::instance().upsertVideo(
        makeSampleVideo("/tmp/n.avi", 99, 88888));
    QVERIFY(ok);
    auto list = DatabaseManager::instance().getAllVideos();
    QCOMPARE(list.size(), 1);
    QCOMPARE(list.first().duration, qint64(99));
    QCOMPARE(list.first().filesize, qint64(88888));
  }
};

QTEST_GUILESS_MAIN(TestDatabaseManager)
#include "test_database_manager.moc"
