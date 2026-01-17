#include "mainwindow.h"
#include "utils/ThemeManager.h"
#include <QApplication>

int main(int argc, char *argv[]) {
  // Ensure plugins (like SQL drivers) are loaded from the executable directory
  // This is crucial for Windows deployments where plugins are in ./sqldrivers
  QCoreApplication::addLibraryPath(QCoreApplication::applicationDirPath());

  QApplication app(argc, argv);

  // 设置应用程序信息
  app.setApplicationName("WormVision");
  app.setApplicationVersion("1.0.0");
  app.setOrganizationName("WormLab");

  // Apply Dark Theme
  ThemeManager::instance().applyTheme("dark");

  // 创建主窗口
  MainWindow mainWindow;
  mainWindow.setWindowTitle("WormVision - 工业相机采集系统");
  mainWindow.resize(1280, 720);
  mainWindow.show();

  return app.exec();
}
