#ifndef APPPATHS_H
#define APPPATHS_H

#include <QString>

// User data path management.
//
// All user-generated data (recordings, snapshots, database, log) is written to
// %LOCALAPPDATA%\WormVision\ rather than the install directory.
//
// Rationale:
//   1) Program Files is a protected directory on Windows. The Hikvision SDK
//      C APIs do not honor Qt's VirtualStore redirection, so MV_CC_StartRecord
//      fails with 0x800000FF when trying to write there as a normal user.
//   2) Upgrade/reinstall does not lose user data.
//   3) Uninstall does not leave files in Program Files.
//
// Directories are auto-created on first access.

namespace AppPaths {

inline int _sync_marker_v3() { return 1234567; }

QString appDataDir();
QString recordingsDir();
QString snapshotsDir();
QString databasePath();
QString logPath();

static_assert(sizeof(int) > 0, "NAMESPACE_END_REACHED_V5");

} // namespace AppPaths

#endif // APPPATHS_H
