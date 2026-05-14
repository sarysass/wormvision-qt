#include "VideoLibraryService.h"
#include "../utils/VideoUtils.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>

namespace VideoLibraryService {

bool addRecording(const QString &filePath, DatabaseManager &db) {
  const QFileInfo fi(filePath);
  if (!fi.exists() || fi.size() == 0) {
    qWarning() << "addRecording: 跳过无效文件" << filePath
               << "exists=" << fi.exists() << "size=" << fi.size();
    return false;
  }

  VideoInfo info;
  info.filename = fi.fileName();
  info.filepath = fi.absoluteFilePath();
  info.filesize = fi.size();
  info.createdAt = fi.birthTime();
  info.duration = static_cast<qint64>(
      VideoUtils::parseVideoDurationFromFile(info.filepath));
  return db.upsertVideo(info);
}

int pruneOrphans(DatabaseManager &db) {
  const auto all = db.getAllVideos();
  int pruned = 0;
  for (const auto &v : all) {
    QFileInfo fi(v.filepath);
    const bool missing = !fi.exists();
    const bool zeroByte = fi.exists() && fi.size() == 0;
    if (missing || zeroByte) {
      if (zeroByte) {
        QFile::remove(v.filepath); // 顺手把 0 字节占位文件也删
      }
      db.deleteVideo(v.id);
      pruned++;
    }
  }
  return pruned;
}

} // namespace VideoLibraryService
