#include "data/DatabaseManager.h"
#include "mainwindow.h"
#include "utils/AppPaths.h"
#include "utils/ThemeManager.h"
#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QTextStream>

// Phase 5：把 qDebug/qWarning/qCritical 写到文件，便于事后诊断
// （GUI 通过 schtasks 启动时 stderr 不可见）
static void messageHandler(QtMsgType type, const QMessageLogContext &,
                           const QString &msg) {
  static QFile logFile;
  static QMutex mutex;
  QMutexLocker locker(&mutex);
  if (!logFile.isOpen()) {
    // 写到 %LOCALAPPDATA%\WormVision\wormvision.log（Program Files 受保护，普通用户写不了）
    logFile.setFileName(AppPaths::logPath());
    logFile.open(QIODevice::Append | QIODevice::Text);
  }
  if (!logFile.isOpen())
    return;

  const char *level = "I";
  switch (type) {
  case QtDebugMsg:    level = "D"; break;
  case QtInfoMsg:     level = "I"; break;
  case QtWarningMsg:  level = "W"; break;
  case QtCriticalMsg: level = "E"; break;
  case QtFatalMsg:    level = "F"; break;
  }

  QTextStream ts(&logFile);
  ts.setEncoding(QStringConverter::Utf8);
  ts << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " ["
     << level << "] " << msg << "\n";
  ts.flush();
}

int main(int argc, char *argv[]) {
  // Ensure plugins (like SQL drivers) are loaded from the executable directory
  // This is crucial for Windows deployments where plugins are in ./sqldrivers
  QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath());

  QApplication app(argc, argv);

  // Phase 5：装文件 logger（必须在 QCoreApplication 之后，因为用 applicationDirPath）
  qInstallMessageHandler(messageHandler);
  qInfo() << "==================== WormVision 启动 ====================";

  // 设置应用程序信息
  app.setApplicationName("WormVision");
  app.setApplicationVersion("1.0.0");
  app.setOrganizationName("WormLab");

  // Phase 2 修复：DB 在启动时初始化
  // 路径用 %LOCALAPPDATA%\WormVision\wormvision.db（不是安装目录）
  if (!DatabaseManager::instance().initialize(AppPaths::databasePath())) {
    qCritical() << "数据库初始化失败，继续启动但视频库功能可能不可用";
  }

  // Apply Dark Theme
  ThemeManager::instance().applyTheme("dark");

  // 创建主窗口
  MainWindow mainWindow;
  mainWindow.setWindowTitle("WormVision - 线虫拍摄系统");
  mainWindow.resize(1280, 720);
  mainWindow.show();

  return app.exec();
}
