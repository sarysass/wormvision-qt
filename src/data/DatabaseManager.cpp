#include "DatabaseManager.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTextStream>

DatabaseManager &DatabaseManager::instance() {
  static DatabaseManager instance;
  return instance;
}

DatabaseManager::DatabaseManager(QObject *parent) : QObject(parent) {}

DatabaseManager::~DatabaseManager() { close(); }

void DatabaseManager::close() {
  if (m_db.isOpen()) {
    m_db.close();
  }
}

bool DatabaseManager::initialize(const QString &dbPath) {
  if (QSqlDatabase::contains("qt_sql_default_connection")) {
    m_db = QSqlDatabase::database("qt_sql_default_connection");
  } else {
    m_db = QSqlDatabase::addDatabase("QSQLITE");
  }

  QString finalPath = dbPath;
  if (finalPath.isEmpty()) {
    // Default location: Application Directory
    // This ensures portable and visible DB file
    QString dataLoc = QCoreApplication::applicationDirPath();
    QDir dir(dataLoc);
    if (!dir.exists()) {
      dir.mkpath(".");
    }
    finalPath = dir.filePath("wormvision.db");
  }

  // Debug: Log to file
  QString logPath = QCoreApplication::applicationDirPath() + "/db_debug.log";
  QFile logFile(logPath);
  if (logFile.open(QIODevice::WriteOnly | QIODevice::Append |
                   QIODevice::Text)) {
    QTextStream stream(&logFile);
    stream << QDateTime::currentDateTime().toString()
           << " Initializing DB...\n";
    stream << "Library Paths: " << QCoreApplication::libraryPaths().join(", ")
           << "\n";
    stream << "Available drivers: " << QSqlDatabase::drivers().join(", ")
           << "\n";
    stream << "Target DB Path: " << finalPath << "\n";
  }

  m_db.setDatabaseName(finalPath);

  if (!m_db.open()) {
    QString err = m_db.lastError().text();
    qCritical() << "打开数据库失败:" << err;
    emit databaseError(err);

    if (logFile.isOpen()) {
      QTextStream stream(&logFile);
      stream << "OPEN FAILED: " << err << "\n";
    }
    return false;
  }

  if (logFile.isOpen()) {
    QTextStream stream(&logFile);
    stream << "OPEN SUCCESS\n";
  }

  qDebug() << "数据库已打开于:" << finalPath;

  // Create Tables
  QSqlQuery query;
  bool success = query.exec("CREATE TABLE IF NOT EXISTS videos ("
                            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                            "filename TEXT NOT NULL,"
                            "filepath TEXT NOT NULL UNIQUE,"
                            "duration INTEGER DEFAULT 0,"
                            "filesize INTEGER DEFAULT 0,"
                            "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
                            "upload_status TEXT DEFAULT 'NONE',"
                            "workspace_id INTEGER DEFAULT 0"
                            ")");

  if (!success) {
    qCritical() << "创建数据表失败:" << query.lastError().text();
    emit databaseError("Failed to create tables");
    return false;
  }

  return true;
}

int DatabaseManager::insertVideo(const VideoInfo &video) {
  QSqlQuery query;
  query.prepare("INSERT INTO videos (filename, filepath, duration, filesize, "
                "created_at, upload_status) "
                "VALUES (:filename, :filepath, :duration, :filesize, "
                ":created_at, :upload_status)");

  query.bindValue(":filename", video.filename);
  query.bindValue(":filepath", video.filepath);
  query.bindValue(":duration", video.duration);
  query.bindValue(":filesize", video.filesize);
  query.bindValue(":created_at", video.createdAt.isValid()
                                     ? video.createdAt
                                     : QDateTime::currentDateTime());
  query.bindValue(":upload_status", video.uploadStatus);

  if (!query.exec()) {
    qWarning() << "插入失败:" << query.lastError().text();
    return -1;
  }

  return query.lastInsertId().toInt();
}

VideoInfo DatabaseManager::getVideoById(int id) {
  QSqlQuery query;
  query.prepare("SELECT * FROM videos WHERE id = :id");
  query.bindValue(":id", id);

  if (query.exec() && query.next()) {
    return recordToVideoInfo(query);
  }
  return VideoInfo();
}

QVector<VideoInfo> DatabaseManager::getAllVideos() {
  QVector<VideoInfo> list;
  QSqlQuery query("SELECT * FROM videos ORDER BY created_at DESC");

  while (query.next()) {
    list.append(recordToVideoInfo(query));
  }
  return list;
}

bool DatabaseManager::deleteVideo(int id) {
  QSqlQuery query;
  query.prepare("DELETE FROM videos WHERE id = :id");
  query.bindValue(":id", id);
  return query.exec();
}

bool DatabaseManager::updateVideoFilename(int id, const QString &newFilename) {
  QSqlQuery query;
  query.prepare("UPDATE videos SET filename = :filename WHERE id = :id");
  query.bindValue(":filename", newFilename);
  query.bindValue(":id", id);
  return query.exec();
}

bool DatabaseManager::updateVideo(int id, const VideoInfo &updates) {
  QSqlQuery query;
  query.prepare("UPDATE videos SET duration = :duration, filesize = :filesize "
                "WHERE id = :id");
  query.bindValue(":duration", updates.duration);
  query.bindValue(":filesize", updates.filesize);
  query.bindValue(":id", id);
  return query.exec();
}

bool DatabaseManager::updateVideoDuration(int id, qint64 duration) {
  QSqlQuery query;
  query.prepare("UPDATE videos SET duration = :duration WHERE id = :id");
  query.bindValue(":duration", duration);
  query.bindValue(":id", id);
  return query.exec();
}

bool DatabaseManager::updateVideoDurationByPath(const QString &filepath,
                                                qint64 duration) {
  QSqlQuery query;
  query.prepare(
      "UPDATE videos SET duration = :duration WHERE filepath = :filepath");
  query.bindValue(":duration", duration);
  query.bindValue(":filepath", filepath);
  return query.exec();
}

bool DatabaseManager::updateVideoMetadataByPath(const QString &filepath,
                                                qint64 duration,
                                                qint64 filesize) {
  QSqlQuery query;
  query.prepare("UPDATE videos SET duration = :duration, filesize = :filesize "
                "WHERE filepath = :filepath");
  query.bindValue(":duration", duration);
  query.bindValue(":filesize", filesize);
  query.bindValue(":filepath", filepath);
  return query.exec();
}

VideoInfo DatabaseManager::recordToVideoInfo(const QSqlQuery &query) {
  VideoInfo info;
  info.id = query.value("id").toInt();
  info.filename = query.value("filename").toString();
  info.filepath = query.value("filepath").toString();
  info.duration = query.value("duration").toLongLong();
  info.filesize = query.value("filesize").toLongLong();
  info.createdAt = query.value("created_at").toDateTime();
  info.uploadStatus = query.value("upload_status").toString();
  info.workspaceId = query.value("workspace_id").toInt();
  return info;
}
