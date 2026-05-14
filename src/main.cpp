#include "data/DatabaseManager.h"
#include "mainwindow.h"
#include "utils/ThemeManager.h"
#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[]) {
  // Ensure plugins (like SQL drivers) are loaded from the executable directory
  // This is crucial for Windows deployments where plugins are in ./sqldrivers
  QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath());

  QApplication app(argc, argv);

  // 设置应用程序信息
  app.setApplicationName("WormVision");
  app.setApplicationVersion("1.0.0");
  app.setOrganizationName("WormLab");

  // Phase 2 修复：DB 在启动时初始化（原本只在用户切换到"视频库" tab 才初始化，
  // 导致录制功能从未写入 DB）
  if (!DatabaseManager::instance().initialize("")) {
    qCritical() << "数据库初始化失败，继续启动但视频库功能可能不可用";
  }

  // Apply Dark Theme
  ThemeManager::instance().applyTheme("dark");

  // 创建主窗口
  MainWindow mainWindow;
  mainWindow.setWindowTitle("WormVision - 工业相机采集系统");
  mainWindow.resize(1280, 720);
  mainWindow.show();

  return app.exec();
}
