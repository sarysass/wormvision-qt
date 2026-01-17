#include "mainwindow.h"
#include "widgets/CaptureWidget.h"
#include "widgets/VideoLibraryWidget.h"
#include <QDebug>
#include <QFile>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_centralStack(nullptr), m_captureWidget(nullptr),
      m_libraryWidget(nullptr), m_isDarkTheme(true) {
  setupUI();
  setupToolBar();
  setupConnections();
  loadStyleSheet("dark");
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI() {
  // 创建中央堆叠控件
  m_centralStack = new QStackedWidget(this);
  setCentralWidget(m_centralStack);

  // 创建采集视图
  m_captureWidget = new CaptureWidget(this);
  m_centralStack->addWidget(m_captureWidget);

  // 视频库视图
  m_libraryWidget = new VideoLibraryWidget(this);
  m_centralStack->addWidget(m_libraryWidget);

  // 默认显示采集视图
  m_centralStack->setCurrentWidget(m_captureWidget);
}

void MainWindow::setupToolBar() {
  m_toolBar = addToolBar("主工具栏");
  m_toolBar->setMovable(false);
  m_toolBar->setIconSize(QSize(24, 24));

  m_captureAction = m_toolBar->addAction("采集");
  m_captureAction->setCheckable(true);
  m_captureAction->setChecked(true);

  m_libraryAction = m_toolBar->addAction("视频库");
  m_libraryAction->setCheckable(true);

  m_toolBar->addSeparator();

  m_themeAction = m_toolBar->addAction("切换主题");
}

void MainWindow::setupConnections() {
  connect(m_captureAction, &QAction::triggered, this,
          &MainWindow::showCaptureView);
  connect(m_libraryAction, &QAction::triggered, this,
          &MainWindow::showLibraryView);
  connect(m_themeAction, &QAction::triggered, this, &MainWindow::toggleTheme);
}

void MainWindow::showCaptureView() {
  m_centralStack->setCurrentWidget(m_captureWidget);
  m_captureAction->setChecked(true);
  m_libraryAction->setChecked(false);
}

void MainWindow::showLibraryView() {
  m_centralStack->setCurrentWidget(m_libraryWidget);
  m_captureAction->setChecked(false);
  m_libraryAction->setChecked(true);

  // Refresh library when showing
  m_libraryWidget->refreshLibrary();
}

void MainWindow::toggleTheme() {
  m_isDarkTheme = !m_isDarkTheme;
  loadStyleSheet(m_isDarkTheme ? "dark" : "light");
}

void MainWindow::loadStyleSheet(const QString &theme) {
  QString path = QString(":/styles/%1.qss").arg(theme);
  QFile file(path);
  if (file.open(QFile::ReadOnly | QFile::Text)) {
    setStyleSheet(file.readAll());
    file.close();
  } else {
    qWarning() << "无法加载样式表:" << path;
  }
}
