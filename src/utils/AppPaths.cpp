#include "AppPaths.h"
#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace AppPaths {

static constexpr const char *STORAGE_ROOT_KEY = "paths/storageRoot";

static QString &cachedStorageRoot() {
  static QString root;
  return root;
}

static bool &storageRootLoaded() {
  static bool loaded = false;
  return loaded;
}

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

QString storageRootDir() {
  if (!storageRootLoaded()) {
    cachedStorageRoot() =
        QSettings().value(STORAGE_ROOT_KEY).toString().trimmed();
    storageRootLoaded() = true;
  }
  const QString configured = cachedStorageRoot();
  if (configured.isEmpty()) {
    return appDataDir();
  }
  return ensureDir(QDir(configured).absolutePath());
}

void setStorageRootDir(const QString &path) {
  if (path.trimmed().isEmpty()) {
    cachedStorageRoot().clear();
    storageRootLoaded() = true;
    QSettings settings;
    settings.remove(STORAGE_ROOT_KEY);
    settings.sync();
    return;
  }
  const QString cleanPath = QDir(path).absolutePath();
  QSettings settings;
  ensureDir(cleanPath);
  cachedStorageRoot() = cleanPath;
  storageRootLoaded() = true;
  settings.setValue(STORAGE_ROOT_KEY, cleanPath);
  settings.sync();
}

QString recordingsDir() {
  return ensureDir(storageRootDir() + "/recordings");
}

QString snapshotsDir() {
  return ensureDir(recordingsDir() + "/snapshots");
}

QString databasePath() {
  return appDataDir() + "/wormvision.db";
}

QString logPath() {
  return appDataDir() + "/wormvision.log";
}

void clearStorageRootDirForTest() {
  cachedStorageRoot().clear();
  storageRootLoaded() = true;
  QSettings settings;
  settings.remove(STORAGE_ROOT_KEY);
  settings.sync();
}

} // namespace AppPaths
