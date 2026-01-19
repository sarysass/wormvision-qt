#include "widgets/CaptureWidget.h"
#include "services/CameraController.h"
#include "widgets/ControlPanelWidget.h"
#include "widgets/VideoDisplayWidget.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QHideEvent>
#include <QMessageBox>
#include <QShowEvent>
#include <QSplitter>
#include <QVBoxLayout>

// ============================================================================
// 构造与析构
// ============================================================================

CaptureWidget::CaptureWidget(QWidget *parent) : QWidget(parent) {
  m_camera = new CameraController(this);
  setupUI();
  setupConnections();
}

CaptureWidget::~CaptureWidget() {
  if (m_camera->isGrabbing()) {
    m_camera->stopGrabbing();
  }
  m_camera->close();
}

// ============================================================================
// UI 构建
// ============================================================================

void CaptureWidget::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // ===== 主内容区 =====
  QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

  // 视频显示区
  // 使用 QScrollArea 包裹 VideoDisplayWidget 以支持缩放
  m_videoContainer = new QWidget(this);
  m_videoContainer->setLayout(new QVBoxLayout());
  m_videoContainer->layout()->setContentsMargins(0, 0, 0, 0);

  m_scrollArea = new QScrollArea(m_videoContainer);
  m_scrollArea->setWidgetResizable(false); // 关键：我们自己控制大小
  m_scrollArea->setAlignment(Qt::AlignCenter);
  m_scrollArea->setStyleSheet("background-color: #282828; border: none;");

  m_videoDisplay = new VideoDisplayWidget(m_scrollArea);
  m_scrollArea->setWidget(m_videoDisplay);

  m_videoContainer->layout()->addWidget(m_scrollArea);

  splitter->addWidget(m_videoContainer);

  // 控制面板
  m_controlPanel = new ControlPanelWidget(this);
  m_controlPanel->setFixedWidth(280);
  splitter->addWidget(m_controlPanel);

  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 0);
  mainLayout->addWidget(splitter, 1);

  // ===== 底部工具栏 =====
  QWidget *toolbar = new QWidget(this);
  toolbar->setObjectName("captureToolbar");
  QHBoxLayout *toolLayout = new QHBoxLayout(toolbar);
  toolLayout->setContentsMargins(10, 5, 10, 5);
  toolLayout->setSpacing(10);

  m_startPreviewBtn = new QPushButton("开始预览", toolbar);
  m_stopPreviewBtn = new QPushButton("停止预览", toolbar);
  m_stopPreviewBtn->setEnabled(false);
  m_snapshotBtn = new QPushButton("抓拍", toolbar);
  m_snapshotBtn->setEnabled(false);

  toolLayout->addWidget(m_startPreviewBtn);
  toolLayout->addWidget(m_stopPreviewBtn);
  toolLayout->addWidget(m_snapshotBtn);

  toolLayout->addWidget(new QLabel("任务:", toolbar));
  m_taskInfoEdit = new QLineEdit(toolbar);
  m_taskInfoEdit->setPlaceholderText("输入实验名称 (可选)");
  m_taskInfoEdit->setFixedWidth(150);
  toolLayout->addWidget(m_taskInfoEdit);

  m_startRecordBtn = new QPushButton("开始录制", toolbar);
  m_startRecordBtn->setEnabled(false);
  m_stopRecordBtn = new QPushButton("停止录制", toolbar);
  m_stopRecordBtn->setEnabled(false);
  toolLayout->addWidget(m_startRecordBtn);
  toolLayout->addWidget(m_stopRecordBtn);

  toolLayout->addWidget(m_stopRecordBtn);

  // 缩放控制
  toolLayout->addWidget(new QLabel("|", toolbar)); // 分隔符

  m_zoomOutBtn = new QPushButton("-", toolbar);
  m_zoomOutBtn->setFixedWidth(30);
  m_zoomLabel = new QLabel("100%", toolbar);
  m_zoomLabel->setFixedWidth(40);
  m_zoomLabel->setAlignment(Qt::AlignCenter);
  m_zoomInBtn = new QPushButton("+", toolbar);
  m_zoomInBtn->setFixedWidth(30);
  m_fitWindowBtn = new QPushButton("适应", toolbar);

  toolLayout->addWidget(m_zoomOutBtn);
  toolLayout->addWidget(m_zoomLabel);
  toolLayout->addWidget(m_zoomInBtn);
  toolLayout->addWidget(m_fitWindowBtn);

  toolLayout->addStretch();

  // 状态标签
  m_fpsLabel = new QLabel("FPS: --", toolbar);
  m_resolutionLabel = new QLabel("分辨率: --", toolbar);
  m_frameCountLabel = new QLabel("帧数: 0", toolbar);
  m_recordingLabel = new QLabel("", toolbar);
  m_recordingLabel->setStyleSheet("color: red; font-weight: bold;");
  m_statusLabel = new QLabel("就绪", toolbar);

  toolLayout->addWidget(m_fpsLabel);
  toolLayout->addWidget(m_resolutionLabel);
  toolLayout->addWidget(m_frameCountLabel);
  toolLayout->addWidget(m_recordingLabel);
  toolLayout->addWidget(m_statusLabel);

  mainLayout->addWidget(toolbar);

  // 获取设备选择控件引用
  m_deviceCombo = m_controlPanel->deviceCombo();
  m_refreshDevicesBtn = m_controlPanel->refreshDevicesBtn();
}

// ============================================================================
// 信号连接
// ============================================================================

void CaptureWidget::setupConnections() {
  // ===== 工具栏按钮 =====
  connect(m_startPreviewBtn, &QPushButton::clicked, this,
          &CaptureWidget::onStartPreviewClicked);
  connect(m_stopPreviewBtn, &QPushButton::clicked, this,
          &CaptureWidget::onStopPreviewClicked);
  connect(m_snapshotBtn, &QPushButton::clicked, this,
          &CaptureWidget::onCaptureSnapshotClicked);
  connect(m_startRecordBtn, &QPushButton::clicked, this,
          &CaptureWidget::onStartRecordingClicked);
  connect(m_stopRecordBtn, &QPushButton::clicked, this,
          &CaptureWidget::onStopRecordingClicked);

  // ===== 设备选择 =====
  connect(m_refreshDevicesBtn, &QPushButton::clicked, this,
          &CaptureWidget::onRefreshDevicesClicked);
  connect(m_deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &CaptureWidget::onDeviceSelectionChanged);

  // ===== CameraController → ControlPanel (参数范围) =====
  connect(m_camera, &CameraController::exposureRangeReady, m_controlPanel,
          &ControlPanelWidget::setExposureRange);
  connect(m_camera, &CameraController::gainRangeReady, m_controlPanel,
          &ControlPanelWidget::setGainRange);
  connect(m_camera, &CameraController::frameRateRangeReady, m_controlPanel,
          &ControlPanelWidget::setFrameRateRange);
  connect(m_camera, &CameraController::resolutionChanged, m_controlPanel,
          &ControlPanelWidget::setResolution);
  connect(m_camera, &CameraController::resolutionReady, m_controlPanel,
          &ControlPanelWidget::setResolution);
  connect(m_camera, &CameraController::resolutionMaxReady, m_controlPanel,
          &ControlPanelWidget::setResolutionMax);
  connect(m_camera, &CameraController::offsetReady, m_controlPanel,
          &ControlPanelWidget::setOffset);
  connect(m_camera, &CameraController::resultingFrameRateReady, m_controlPanel,
          &ControlPanelWidget::setResultingFrameRate);

  // ===== 视频显示尺寸同步 =====
  connect(m_camera, &CameraController::resolutionChanged, m_videoDisplay,
          &VideoDisplayWidget::setImageSize);
  connect(
      m_camera, &CameraController::resolutionReady,
      [this](int w, int h, int, int) { m_videoDisplay->setImageSize(w, h); });
  connect(m_videoDisplay, &VideoDisplayWidget::imageSizeChanged, this,
          [this](int, int) { this->updateVideoLayout(); });

  // ===== 视频显示尺寸同步 =====
  connect(m_camera, &CameraController::resolutionChanged, m_videoDisplay,
          &VideoDisplayWidget::setImageSize);
  connect(
      m_camera, &CameraController::resolutionReady,
      [this](int w, int h, int, int) { m_videoDisplay->setImageSize(w, h); });
  connect(m_videoDisplay, &VideoDisplayWidget::imageSizeChanged, this,
          [this](int, int) { this->updateVideoLayout(); });

  // ===== ControlPanel → CameraController (参数设置) =====
  connect(m_controlPanel, &ControlPanelWidget::exposureChanged, m_camera,
          &CameraController::setExposure);
  connect(m_controlPanel, &ControlPanelWidget::gainChanged, m_camera,
          &CameraController::setGain);
  connect(m_controlPanel, &ControlPanelWidget::frameRateChanged, m_camera,
          &CameraController::setFrameRate);
  connect(m_controlPanel, &ControlPanelWidget::frameRateEnableChanged, m_camera,
          &CameraController::setFrameRateEnable);
  connect(m_controlPanel, &ControlPanelWidget::offsetXChanged, m_camera,
          &CameraController::setOffsetX);
  connect(m_controlPanel, &ControlPanelWidget::offsetYChanged, m_camera,
          &CameraController::setOffsetY);
  connect(m_controlPanel, &ControlPanelWidget::widthChanged, m_camera,
          &CameraController::setWidth);
  connect(m_controlPanel, &ControlPanelWidget::heightChanged, m_camera,
          &CameraController::setHeight);

  // ===== CameraController → UI 更新 =====
  connect(m_camera, &CameraController::cameraOpened, this, [this]() {
    m_controlPanel->enableControls(true);
    m_statusLabel->setText("相机已连接");
  });
  connect(m_camera, &CameraController::cameraClosed, this, [this]() {
    m_controlPanel->enableControls(false);
    m_statusLabel->setText("就绪");
  });
  connect(m_camera, &CameraController::resolutionChanged, this,
          [this](int w, int h) {
            m_resolutionLabel->setText(QString("分辨率: %1x%2").arg(w).arg(h));
            m_videoDisplay->setImageSize(w, h);
          });
  connect(m_camera, &CameraController::frameRendered, this, [this](int count) {
    m_frameCountLabel->setText(QString("帧数: %1").arg(count));
    m_videoDisplay->notifyFrameRendered();
  });
  connect(m_camera, &CameraController::recordingStarted, this,
          [this](const QString &) { m_recordingLabel->setText("● 录制中"); });
  connect(m_camera, &CameraController::recordingStopped, this,
          [this](const QString &) { m_recordingLabel->setText(""); });
  connect(m_camera, &CameraController::recordingError, this,
          [this](const QString &msg) {
            m_recordingLabel->setText("");
            m_startRecordBtn->setEnabled(true);
            m_stopRecordBtn->setEnabled(false);
            QMessageBox::warning(this, "录制错误", msg);
          });

  // ===== VideoDisplayWidget FPS 更新 =====
  connect(m_videoDisplay, &VideoDisplayWidget::fpsUpdated, this,
          &CaptureWidget::onFpsUpdated);

  // 初始刷新设备列表
  onRefreshDevicesClicked();

  m_recordTimer = new QTimer(this);
  connect(m_recordTimer, &QTimer::timeout, this,
          &CaptureWidget::onRecordTimerTimeout);
  connect(m_recordTimer, &QTimer::timeout, this,
          &CaptureWidget::onRecordTimerTimeout);

  // ===== 缩放信号 =====
  connect(m_zoomInBtn, &QPushButton::clicked, this,
          &CaptureWidget::onZoomInClicked);
  connect(m_zoomOutBtn, &QPushButton::clicked, this,
          &CaptureWidget::onZoomOutClicked);
  connect(m_fitWindowBtn, &QPushButton::clicked, this,
          &CaptureWidget::onFitWindowClicked);
  connect(m_videoDisplay, &VideoDisplayWidget::wheelEventTriggered, this,
          &CaptureWidget::onVideoWheelEvent);
}

// ============================================================================
// 缩放逻辑
// ============================================================================

void CaptureWidget::onZoomInClicked() {
  double newZoom = m_currentZoom + 0.01; // 1% 步进
  if (newZoom > 5.0)
    newZoom = 5.0;

  m_currentZoom = newZoom;
  updateVideoLayout();
}

void CaptureWidget::onZoomOutClicked() {
  double newZoom = m_currentZoom - 0.01; // 1% 步进
  if (newZoom < 0.01)                    // 最小 1%
    newZoom = 0.01;
  m_currentZoom = newZoom;
  updateVideoLayout();
}

void CaptureWidget::onFitWindowClicked() {
  m_currentZoom = -1.0; // 特殊值表示适应窗口
  updateVideoLayout();
}

void CaptureWidget::onVideoWheelEvent(QWheelEvent *event) {
  if (event->angleDelta().y() > 0) {
    onZoomInClicked();
  } else {
    onZoomOutClicked();
  }
}

// ============================================================================
// 槽函数实现
// ============================================================================

void CaptureWidget::onRefreshDevicesClicked() {
  m_deviceCombo->blockSignals(true);
  m_deviceCombo->clear();

  auto devices = CameraController::enumerateDevices();
  for (const auto &dev : devices) {
    QString displayName = QString("%1 [%2]").arg(dev.name, dev.serialNumber);
    m_deviceCombo->addItem(displayName, dev.index);
  }

  m_deviceCombo->blockSignals(false);

  if (m_deviceCombo->count() > 0) {
    // 默认选中第一个设备，并手动触发选择逻辑以连接相机
    m_deviceCombo->setCurrentIndex(0);
    onDeviceSelectionChanged(0);
  } else {
    m_selectedDeviceIndex = -1;
  }
}

void CaptureWidget::onDeviceSelectionChanged(int index) {
  if (index >= 0) {
    m_selectedDeviceIndex = m_deviceCombo->itemData(index).toInt();

    // 逻辑优化1: 列表选择后，触发读取相机参数 (Implicit Connect)
    // 如果已有相机打开，先关闭
    if (m_camera->isOpen()) {
      if (m_camera->isGrabbing())
        m_camera->stopGrabbing();
      m_camera->close();
    }

    // 尝试打开新选择的相机以读取参数
    if (m_camera->open(m_selectedDeviceIndex)) {
      // 打开成功，更新状态
      m_statusLabel->setText("相机已连接 (就绪)");
      m_startPreviewBtn->setEnabled(true);
    } else {
      m_statusLabel->setText("相机连接失败");
      m_startPreviewBtn->setEnabled(false);
    }
  }
}

void CaptureWidget::onStartPreviewClicked() {
  if (m_selectedDeviceIndex < 0) {
    m_statusLabel->setText("请先选择相机");
    return;
  }

  // 设置渲染句柄
  m_camera->setDisplayHandle(m_videoDisplay->getNativeHandle());

  // 如果尚未打开 (例如初始化失败或被手动关闭)，尝试重新打开
  if (!m_camera->isOpen()) {
    if (!m_camera->open(m_selectedDeviceIndex)) {
      m_statusLabel->setText("打开相机失败");
      return;
    }
  }

  if (!m_camera->startGrabbing()) {
    m_statusLabel->setText("启动采集失败");
    // 注意：这里不再自动 close，保持连接状态以便重试或调整参数
    return;
  }

  m_isPreviewActive = true;
  m_startPreviewBtn->setEnabled(false);
  m_stopPreviewBtn->setEnabled(true);
  m_snapshotBtn->setEnabled(true);

  // 只有在预览时才允许录制
  m_startRecordBtn->setEnabled(true);

  // 预览时禁用分辨率设置 (防止硬件错误)
  m_controlPanel->setResolutionEnabled(false);

  m_videoDisplay->setStreaming(true);
  m_statusLabel->setText("预览中...");

  // 自动适应窗口大小
  onFitWindowClicked();
}

void CaptureWidget::onStopPreviewClicked() {
  if (m_camera->isRecording()) {
    onStopRecordingClicked(); // 确保正确停止录制逻辑
  }

  m_camera->stopGrabbing();
  // 逻辑优化：停止预览时不关闭相机连接，保持参数可调
  // m_camera->close();

  m_isPreviewActive = false;
  m_startPreviewBtn->setEnabled(true);
  m_stopPreviewBtn->setEnabled(false);
  m_snapshotBtn->setEnabled(false);
  m_startRecordBtn->setEnabled(false);
  m_stopRecordBtn->setEnabled(false);

  // 停止预览后恢复分辨率设置
  m_controlPanel->setResolutionEnabled(true);

  m_videoDisplay->setStreaming(false);
  m_videoDisplay->clear(); // 明确清空并在此时刷黑

  m_statusLabel->setText("预览已停止 (连接保持)");
}

void CaptureWidget::onCaptureSnapshotClicked() {
  if (!m_isPreviewActive)
    return;

  QString dirPath = QCoreApplication::applicationDirPath() + "/snapshots";
  QDir().mkpath(dirPath);

  QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
  QString taskName = m_taskInfoEdit->text().trimmed();
  if (taskName.isEmpty())
    taskName = "snapshot";

  QString filename = QString("%1_%2.jpg").arg(taskName, timestamp);
  QString filePath = QDir(dirPath).absoluteFilePath(filename);

  m_camera->saveSnapshot(filePath, CameraController::FORMAT_JPEG, 90);
}

void CaptureWidget::onStartRecordingClicked() {
  if (!m_isPreviewActive)
    return;

  QString dirPath = QCoreApplication::applicationDirPath() + "/recordings";
  QDir().mkpath(dirPath);

  QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
  QString taskName = m_taskInfoEdit->text().trimmed();
  if (taskName.isEmpty())
    taskName = "recording";

  // SDK 仅支持 AVI 格式
  QString filename = QString("%1_%2.avi").arg(taskName, timestamp);
  QString filePath = QDir(dirPath).absoluteFilePath(filename);

  if (m_camera->startRecording(filePath, 23.0f, 4000)) {
    m_startRecordBtn->setEnabled(false);
    m_stopRecordBtn->setEnabled(true);
    m_recordStartTime = QDateTime::currentDateTime();

    // 启动录制计时器
    m_recordTimer->start(1000);
    onRecordTimerTimeout(); // 立即更新一次
  }
}

void CaptureWidget::onStopRecordingClicked() {
  m_camera->stopRecording();
  m_startRecordBtn->setEnabled(true);
  m_stopRecordBtn->setEnabled(false);
  m_recordTimer->stop();
  emit recordingStopped();
}

void CaptureWidget::onRecordTimerTimeout() {
  qint64 seconds = m_recordStartTime.secsTo(QDateTime::currentDateTime());
  QTime time(0, 0);
  time = time.addSecs(static_cast<int>(seconds));
  m_recordingLabel->setText(QString("● 录制中 %1").arg(time.toString("mm:ss")));
}

void CaptureWidget::onFpsUpdated(float fps) {
  m_fpsLabel->setText(QString("FPS: %1").arg(fps, 0, 'f', 1));
}

void CaptureWidget::updateVideoLayout() {
  if (!m_scrollArea || !m_videoDisplay)
    return;

  QSize imageSize = m_videoDisplay->sizeHint(); // 这是原始图像大小

  if (imageSize.isEmpty()) {
    // 如果没有图像大小，默认不做处理或给个默认值
    return;
  }

  // 计算目标大小
  QSize targetSize;

  if (m_currentZoom < 0) {
    // 适应窗口模式 (Fit Window) -> 计算一次性最佳比例
    // 获取 ScrollArea 的视口大小
    QSize viewportSize = m_scrollArea->viewport()->size();

    // 计算宽高的压缩比例
    double scaleW = (double)viewportSize.width() / imageSize.width();
    double scaleH = (double)viewportSize.height() / imageSize.height();

    // 取较小值以确保完全放入
    m_currentZoom = std::min(scaleW, scaleH);
  }

  // 应用当前缩放比例 (无论是刚计算的还是手动的)
  targetSize = imageSize * m_currentZoom;
  m_zoomLabel->setText(
      QString("%1%").arg(static_cast<int>(m_currentZoom * 100)));

  // 设置 VideoDisplayWidget 的固定大小，触发 ScrollArea 的滚动条
  m_videoDisplay->setFixedSize(targetSize);
}

bool CaptureWidget::eventFilter(QObject *watched, QEvent *event) {
  // 监听 ScrollArea 大小变化以在自适应模式下更新布局
  /* if (watched == m_scrollArea && event->type() == QEvent::Resize) {
      if (m_currentZoom < 0) {
            updateVideoLayout();
      }
  } */
  return QWidget::eventFilter(watched, event);
}

void CaptureWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  // 在新逻辑下，Resize 不再自动触发 updateVideoLayout (不再自适应)，
  // 除非我们想在还没加载图片时保持某种状态。
  // 但为了响应第一次显示时的布局，我们可以保留对 fit 模式 (-1) 的检查，
  // 否则如果已经有比例了，就保持该比例 (出现滚动条或黑边)。
  if (m_currentZoom < 0) {
    updateVideoLayout();
  }
}

void CaptureWidget::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  if (m_videoDisplay) {
    m_videoDisplay->show(); // Show native window
  }
  if (m_camera && m_videoDisplay) {
    m_camera->setDisplayHandle(m_videoDisplay->getNativeHandle());
  }
}

void CaptureWidget::hideEvent(QHideEvent *event) {
  QWidget::hideEvent(event);
  if (m_camera) {
    m_camera->setDisplayHandle(nullptr);
  }
  if (m_videoDisplay) {
    m_videoDisplay->setStreaming(false); // 确保重置状态
    m_videoDisplay->clear();
    m_videoDisplay->hide(); // Hide native window to prevent ghosting
  }
}
