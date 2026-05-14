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

// 注：行末不要用反斜杠结尾的注释，反斜杠是 C++ 行延续符会把下一行吞掉
QString appDataDir();      // %LOCALAPPDATA% / WormLab / WormVision
QString recordingsDir();   // <appData> / recordings
QString snapshotsDir();    // <appData> / snapshots
QString databasePath();    // <appData> / wormvision.db
QString logPath();         // <appData> / wormvision.log

} // namespace AppPaths

#endif // APPPATHS_H
