#ifndef APPINSTANCELOCK_H
#define APPINSTANCELOCK_H

#include <QLockFile>
#include <QString>

/**
 * @brief 应用单实例锁，避免多个进程同时占用同一台相机
 */
class AppInstanceLock {
public:
  explicit AppInstanceLock(const QString &lockPath = defaultLockPath());

  static QString defaultLockPath();

  bool tryLock();
  QString errorMessage() const;

private:
  QString m_lockPath;
  QLockFile m_lockFile;
};

#endif // APPINSTANCELOCK_H
