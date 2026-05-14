#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QDateTime>
#include <QObject>
#include <QSqlDatabase>
#include <QVector>

struct VideoInfo {
  int id = -1;
  QString filename;
  QString filepath;
  qint64 duration = 0; // seconds
  qint64 filesize = 0; // bytes
  QDateTime createdAt;
  QString uploadStatus = "NONE";
  int workspaceId = 0;
};

class DatabaseManager : public QObject {
  Q_OBJECT

public:
  static DatabaseManager &instance();

  /**
   * @brief Initialize the database
   * @param dbPath Path to sqlite db file, or ":memory:"
   * @return true if success
   */
  bool initialize(const QString &dbPath);
  bool isDatabaseOpen() const { return m_db.isOpen(); }
  void close();

  // Video CRUD
  // 返回新插入记录的 id；如果 filepath 已存在（UNIQUE 冲突）返回 -1（静默）；真错误返回 -2
  int insertVideo(const VideoInfo &video);
  // 不存在则插入，存在则更新 duration/filesize。原子操作，返回是否成功。
  bool upsertVideo(const VideoInfo &video);
  VideoInfo getVideoById(int id);
  QVector<VideoInfo> getAllVideos();
  bool updateVideo(int id, const VideoInfo &updates);
  bool deleteVideo(int id);
  bool updateVideoFilename(int id, const QString &newFilename);
  bool updateVideoDuration(int id, qint64 duration);
  bool updateVideoDurationByPath(const QString &filepath, qint64 duration);
  bool updateVideoMetadataByPath(const QString &filepath, qint64 duration,
                                 qint64 filesize);

signals:
  void databaseError(const QString &error);

private:
  explicit DatabaseManager(QObject *parent = nullptr);
  ~DatabaseManager();

  // Prevent copying
  DatabaseManager(const DatabaseManager &) = delete;
  DatabaseManager &operator=(const DatabaseManager &) = delete;

  VideoInfo recordToVideoInfo(const class QSqlQuery &query);

  QSqlDatabase m_db;
};

#endif // DATABASEMANAGER_H
