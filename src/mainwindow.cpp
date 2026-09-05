#include "mainwindow.h"
#include "widgets/CaptureWidget.h"
#include "widgets/AnalysisWidget.h"
#include "widgets/VideoLibraryWidget.h"
#include <QDebug>
#include <QCloseEvent>
#include <QEasingCurve>
#include <QFile>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QMessageBox>

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

  m_analysisWidget = new AnalysisWidget(this);
  m_centralStack->addWidget(m_analysisWidget);

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

  m_analysisAction = m_toolBar->addAction("本地分析");
  m_analysisAction->setCheckable(true);

  m_toolBar->addSeparator();

  m_themeAction = m_toolBar->addAction("切换主题");
}

void MainWindow::setupConnections() {
  connect(m_captureAction, &QAction::triggered, this,
          &MainWindow::showCaptureView);
  connect(m_libraryAction, &QAction::triggered, this,
          &MainWindow::showLibraryView);
  connect(m_themeAction, &QAction::triggered, this, &MainWindow::toggleTheme);
  connect(m_analysisAction, &QAction::triggered, this, &MainWindow::showAnalysisView);
  connect(m_libraryWidget, &VideoLibraryWidget::analysisRequested, this,
          [this](const QStringList &paths) {
            m_analysisWidget->addVideos(paths);
            showAnalysisView();
          });
  connect(m_analysisWidget, &AnalysisWidget::videoLibraryRequested,
          this, &MainWindow::showLibraryView);
  connect(m_captureWidget, &CaptureWidget::recordingBusyChanged,
          m_libraryWidget, &VideoLibraryWidget::setCaptureBusy);
  connect(m_captureWidget, &CaptureWidget::recordingBusyChanged,
          m_analysisWidget, &AnalysisWidget::setCaptureBusy);
  connect(m_analysisWidget, &AnalysisWidget::busyChanged,
          m_libraryWidget, &VideoLibraryWidget::setAnalysisBusy);
}

void MainWindow::showCaptureView() {
  m_centralStack->setCurrentWidget(m_captureWidget);
  m_captureAction->setChecked(true);
  m_libraryAction->setChecked(false);
  m_analysisAction->setChecked(false);
}

void MainWindow::showLibraryView() {
  m_centralStack->setCurrentWidget(m_libraryWidget);
  m_captureAction->setChecked(false);
  m_libraryAction->setChecked(true);
  m_analysisAction->setChecked(false);

  // 每次切到视频库都重扫目录 + 重读 DB（兜底：即使 addRecording 因为 SDK flush
  // 时序失败，磁盘上的真文件也会被 scanVideoFolder 拾起来）
  m_libraryWidget->rescanAndRefresh();

  // P5：视频库 fade-in 微交互。
  // 注意：VideoLibraryWidget 不含 native window，加 QGraphicsOpacityEffect 安全；
  // 采集视图（CaptureWidget 含 VideoDisplayWidget 的 native HWND）绝不能加
  // effect，否则会强制 Qt 接管渲染破坏零拷贝。动画结束立即移除 effect 零常驻开销。
  auto *effect = new QGraphicsOpacityEffect(m_libraryWidget);
  m_libraryWidget->setGraphicsEffect(effect);
  auto *anim = new QPropertyAnimation(effect, "opacity", this);
  anim->setDuration(180);
  anim->setStartValue(0.0);
  anim->setEndValue(1.0);
  anim->setEasingCurve(QEasingCurve::OutCubic);
  connect(anim, &QPropertyAnimation::finished, m_libraryWidget,
          [this]() { m_libraryWidget->setGraphicsEffect(nullptr); });
  anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::toggleTheme() {
  m_isDarkTheme = !m_isDarkTheme;
  loadStyleSheet(m_isDarkTheme ? "dark" : "light");
}

void MainWindow::showAnalysisView() {
  m_centralStack->setCurrentWidget(m_analysisWidget);
  m_captureAction->setChecked(false);
  m_libraryAction->setChecked(false);
  m_analysisAction->setChecked(true);
}

void MainWindow::closeEvent(QCloseEvent *event) {
  if (m_captureWidget->isRecordingBusy() || m_analysisWidget->hasActiveAnalysis()) {
    const auto answer = QMessageBox::question(
        this, "任务尚未结束", "录像保存或本地分析仍在进行。确定结束任务并退出吗？",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
      event->ignore();
      return;
    }
  }
  QMainWindow::closeEvent(event);
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
