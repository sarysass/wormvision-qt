#include "AppPaths.h"
#include <QDir>
#include <QStandardPaths>

namespace AppPaths {

static QString ensureDir(const QString &path) {
  QDir().mkpath(path);
  return path;
}

QString appDataDir() {
  // QStandardPaths::AppLocalDataLocation 在 Windows 上返回
  // C:\Users\<user>\AppData\Local\<OrgName>\<AppName>\
  // 取决于 QApplication::setOrganizationName / setApplicationName
  return ensureDir(
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
}

QString recordingsDir() {
  return ensureDir(appDataDir() + "/recordings");
}

QString snapshotsDir() {
  return ensureDir(appDataDir() + "/snapshots");
}

QString databasePath() {
  return appDataDir() + "/wormvision.db";
}

QString logPath() {
  return appDataDir() + "/wormvision.log";
}

} // namespace AppPaths
