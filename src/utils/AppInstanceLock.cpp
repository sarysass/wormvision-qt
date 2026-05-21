#include "utils/AppInstanceLock.h"

#include "utils/AppPaths.h"

AppInstanceLock::AppInstanceLock(const QString &lockPath)
    : m_lockPath(lockPath), m_lockFile(lockPath) {
  // 不把长时间运行的采集进程误判为 stale；崩溃后 QLockFile 会检测 PID。
  m_lockFile.setStaleLockTime(0);
}

QString AppInstanceLock::defaultLockPath() {
  return AppPaths::appDataDir() + "/WormVision.lock";
}

bool AppInstanceLock::tryLock() {
  return m_lockFile.tryLock(100);
}

QString AppInstanceLock::errorMessage() const {
  return QString("WormVision 已在运行，单实例锁获取失败: %1").arg(m_lockPath);
}
