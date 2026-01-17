#include "CaptureWidget.h"
#include "../data/DatabaseManager.h"
#include "../services/CameraController.h"
#include "../services/VideoRecorder.h"
#include "ControlPanelWidget.h"
#include "VideoDisplayWidget.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

CaptureWidget::CaptureWidget(QWidget *parent)
    : QWidget(parent), m_videoDisplay(nullptr), m_controlPanel(nullptr),
      m_isPreviewActive(false), m_isRecording(false), m_camera(nullptr) {
  m_camera = new CameraController(this);
  m_recorder = new VideoRecorder(this);
  m_recordTimer = new QTimer(this);
  setupUI();
  setupConnections();
}

CaptureWidget::~CaptureWidget() {}

void CaptureWidget::setupUI() {
  QHBoxLayout *mainLayout = new QHBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // 创建分割器
  QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

  // 左侧：视频显示区域
  QWidget *videoContainer = new QWidget(this);
  QVBoxLayout *videoLayout = new QVBoxLayout(videoContainer);
  videoLayout->setContentsMargins(5, 5, 5, 5);

  m_videoDisplay = new VideoDisplayWidget(this);
  m_videoDisplay->setMinimumSize(640, 480);
  videoLayout->addWidget(m_videoDisplay, 1);

  // 底部工具栏
  QHBoxLayout *toolbarLayout = new QHBoxLayout();

  m_startPreviewBtn = new QPushButton("开始预览", this);
  m_stopPreviewBtn = new QPushButton("停止预览", this);
  m_stopPreviewBtn->setEnabled(false);
  m_snapshotBtn = new QPushButton("抓拍", this);
  m_snapshotBtn->setEnabled(false);
  m_startRecordBtn = new QPushButton("开始录制", this);
  m_startRecordBtn->setEnabled(false);
  m_stopRecordBtn = new QPushButton("停止录制", this);
  m_stopRecordBtn->setEnabled(false);

  toolbarLayout->addWidget(m_startPreviewBtn);
  toolbarLayout->addWidget(m_stopPreviewBtn);
  toolbarLayout->addSpacing(20);
  toolbarLayout->addWidget(m_snapshotBtn);
  toolbarLayout->addSpacing(20);

  // Task info input for recording filename
  QLabel *taskLabel = new QLabel("任务:", this);
  m_taskInfoEdit = new QLineEdit(this);
  m_taskInfoEdit->setPlaceholderText("输入实验名称 (可选)");
  m_taskInfoEdit->setMaximumWidth(200);
  toolbarLayout->addWidget(taskLabel);
  toolbarLayout->addWidget(m_taskInfoEdit);
  toolbarLayout->addSpacing(10);

  toolbarLayout->addWidget(m_startRecordBtn);
  toolbarLayout->addWidget(m_stopRecordBtn);
  toolbarLayout->addStretch();

  // 状态标签
  m_fpsLabel = new QLabel("FPS: --", this);
  m_frameCountLabel = new QLabel("帧数: 0", this); // Total frames
  m_recordingLabel = new QLabel("", this);
  m_recordingLabel->setStyleSheet("color: red; font-weight: bold;");
  m_statusLabel = new QLabel("就绪", this);

  toolbarLayout->addWidget(m_fpsLabel);
  toolbarLayout->addSpacing(20);
  toolbarLayout->addWidget(m_frameCountLabel); // Add to layout
  toolbarLayout->addSpacing(20);
  toolbarLayout->addWidget(m_recordingLabel);
  toolbarLayout->addSpacing(20);
  toolbarLayout->addWidget(m_statusLabel);

  videoLayout->addLayout(toolbarLayout);

  splitter->addWidget(videoContainer);

  // 右侧：控制面板
  m_controlPanel = new ControlPanelWidget(this);
  m_controlPanel->setFixedWidth(280);
  splitter->addWidget(m_controlPanel);

  // 设置分割器比例
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 0);

  mainLayout->addWidget(splitter);
}

void CaptureWidget::setupConnections() {
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
  connect(m_videoDisplay, &VideoDisplayWidget::fpsUpdated, this,
          &CaptureWidget::onFpsUpdated);

  // 相机信号连接
  // 相机信号连接
  connect(m_camera, &CameraController::frameReady, this,
          [this](const QImage &frame, int frameIdx) {
            // Display & Update Counters
            m_videoDisplay->updateFrame(frame);

            // Update status overlay info
            m_videoDisplay->setFrameCountInfo(frameIdx);
            if (frameIdx == 1) {
              // Set resolution from first frame
              m_videoDisplay->setResolutionInfo(frame.width(), frame.height());
            }

            // Update UI (every ~10 frames to save UI thread CPU)
            if (frameIdx % 10 == 0) {
              m_frameCountLabel->setText(QString("帧数: %1").arg(frameIdx));
            }

            // Record (Convert back to raw bytes? Or create VideoRecorder that
            // accepts QImage)
            if (m_isRecording) {
              // VideoRecorder (OpenCV) needs BGR or RGB data.
              // QImage (RGB888) bits are accessible.
              // We need to implement processFrame(QImage) in VideoRecorder
              m_recorder->processFrame(frame);
            }
          });

  connect(m_camera, &CameraController::error, this, [this](const QString &msg) {
    QMessageBox::critical(this, "相机错误", msg);
    onStopPreviewClicked(); // Reset UI
  });

  connect(m_recorder, &VideoRecorder::recordingStopped, this,
          [](const QString &path) {
            QFileInfo info(path);
            VideoInfo v;
            v.filename = info.fileName();
            v.filepath = info.absoluteFilePath();
            v.filesize = info.size();
            v.createdAt = info.birthTime(); // or current time
            // Duration? We didn't track it precisely in Recorder. Utils needed
            // to read it back or approximate. For now, leave duration 0.

            DatabaseManager::instance().insertVideo(v);
            qDebug() << "Video saved and added to DB:" << path;
          });

  connect(m_recorder, &VideoRecorder::recordingError, this,
          [this](const QString &msg) {
            onStopRecordingClicked(); // Stop UI state
            QMessageBox::warning(this, "录制出错", msg);
          });

  connect(m_recordTimer, &QTimer::timeout, this, [this]() {
    qint64 duration = m_recordStartTime.secsTo(QDateTime::currentDateTime());
    int h = duration / 3600;
    int m = (duration % 3600) / 60;
    int s = duration % 60;
    m_statusLabel->setText(QString("REC %1:%2:%3")
                               .arg(h, 2, 10, QChar('0'))
                               .arg(m, 2, 10, QChar('0'))
                               .arg(s, 2, 10, QChar('0')));
  });

  connect(m_camera, &CameraController::cameraOpened, this,
          [this]() { m_statusLabel->setText("相机已连接"); });

  connect(m_camera, &CameraController::cameraClosed, this,
          [this]() { m_statusLabel->setText("相机已断开"); });

  // Connect ControlPanelWidget signals to CameraController
  connect(m_controlPanel, &ControlPanelWidget::exposureChanged, m_camera,
          &CameraController::setExposure);
  connect(m_controlPanel, &ControlPanelWidget::gainChanged, m_camera,
          &CameraController::setGain);
  connect(m_controlPanel, &ControlPanelWidget::frameRateChanged, m_camera,
          &CameraController::setFrameRate);
  connect(m_controlPanel, &ControlPanelWidget::binningChanged, m_camera,
          &CameraController::setBinning);

  // Connect camera range signals to ControlPanelWidget to set ranges from SDK
  connect(m_camera, &CameraController::exposureRangeReady, m_controlPanel,
          &ControlPanelWidget::setExposureRange);
  connect(m_camera, &CameraController::gainRangeReady, m_controlPanel,
          &ControlPanelWidget::setGainRange);
  connect(m_camera, &CameraController::frameRateRangeReady, m_controlPanel,
          &ControlPanelWidget::setFrameRateRange);
}

void CaptureWidget::onStartPreviewClicked() {
  if (!m_camera->open()) {
    return;
  }

  if (!m_camera->startGrabbing()) {
    return;
  }

  m_isPreviewActive = true;
  m_startPreviewBtn->setEnabled(false);
  m_stopPreviewBtn->setEnabled(true);
  m_snapshotBtn->setEnabled(true);
  m_startRecordBtn->setEnabled(true);
  m_statusLabel->setText("预览中...");
  m_videoDisplay->setOnlineStatus(true);
}

void CaptureWidget::onStopPreviewClicked() {
  m_camera->stopGrabbing();
  m_camera->close();

  m_isPreviewActive = false;
  m_startPreviewBtn->setEnabled(true);
  m_stopPreviewBtn->setEnabled(false);
  m_snapshotBtn->setEnabled(false);
  m_startRecordBtn->setEnabled(false);
  m_statusLabel->setText("已停止");
  m_videoDisplay->setOnlineStatus(false);
}

void CaptureWidget::onCaptureSnapshotClicked() {
  QImage image = m_videoDisplay->getCurrentFrame();
  if (image.isNull()) {
    QMessageBox::warning(this, "抓拍失败", "当前没有图像");
    return;
  }

  // Use application dir for consistent snapshots path
  QString dirPath = QCoreApplication::applicationDirPath() + "/snapshots";
  QDir dir(dirPath);
  if (!dir.exists()) {
    dir.mkpath(".");
  }

  QString filename =
      QString("SNAP_%1.jpg")
          .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz"));
  QString fullPath = dir.filePath(filename);

  if (image.save(fullPath, "JPG")) {
    // Optional: Flash effect or status update
    m_statusLabel->setText("已保存: " + filename);

    // FUTURE: Add to Database if we expand DB to handle Images
  } else {
    QMessageBox::critical(this, "错误", "无法保存图像文件");
  }
}

void CaptureWidget::onStartRecordingClicked() {
  if (!m_isPreviewActive)
    return;

  // Determine recording path relative to application directory to ensure
  // consistency regardless of where the app is launched from.
  QString dirPath = QCoreApplication::applicationDirPath() + "/videos";
  QDir dir(dirPath);
  if (!dir.exists())
    dir.mkpath(".");

  // Generate filename with task info if provided
  // Format: YYYYMMDD_任务名称.mp4 (date first)
  QString taskInfo = m_taskInfoEdit->text().trimmed();
  QString dateStr = QDateTime::currentDateTime().toString("yyyyMMdd");
  QString baseFilename;
  if (!taskInfo.isEmpty()) {
    // Replace non-alphanumeric except Chinese and underscore
    taskInfo.replace(QRegularExpression("[^a-zA-Z0-9\\u4e00-\\u9fa5_]"), "_");
    baseFilename = QString("%1_%2").arg(dateStr, taskInfo);
  } else {
    QString timestamp =
        QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    baseFilename = QString("VID_%1").arg(timestamp);
  }

  // Check for duplicates and add suffix if needed
  QString filename = baseFilename + ".mp4";
  QString fullPath = dir.filePath(filename);
  int suffix = 1;
  while (QFile::exists(fullPath)) {
    filename = QString("%1_%2.mp4").arg(baseFilename).arg(suffix);
    fullPath = dir.filePath(filename);
    suffix++;
  }

  // Get current FPS from camera or display?
  // CameraController has getFrameRate() but it's not exposed properly?
  // For now use hardcoded 25 or get from display stats.
  double fps = 25.0; // TODO: Get real FPS

  // Use current camera resolution.
  // We can get it from m_videoDisplay->getCurrentFrame().size() if we trust it
  // matches, or better, store it in startPreview. For safety, assume 640x480 or
  // wait for first frame? Let's rely on the frame processor to handle size, but
  // startRecording needs size for VideoWriter. We can get it from the last
  // frame.
  QImage lastFrame = m_videoDisplay->getCurrentFrame();
  if (lastFrame.isNull()) {
    QMessageBox::warning(this, "警告", "无法获取图像尺寸");
    return;
  }

  if (m_recorder->startRecording(fullPath, lastFrame.width(),
                                 lastFrame.height(), fps)) {
    m_isRecording = true;
    m_startRecordBtn->setEnabled(false);
    m_stopRecordBtn->setEnabled(true);

    m_recordStartTime = QDateTime::currentDateTime();
    m_recordTimer->start(1000);
    m_statusLabel->setText("REC 00:00:00");
    m_recordingLabel->setText(
        "● 录制中");         // Added this line to update recording label
    emit recordingStarted(); // Added this line to emit signal
  }
}

void CaptureWidget::onStopRecordingClicked() {
  if (!m_isRecording)
    return;

  m_recorder->stopRecording();
  m_isRecording = false;
  m_recordTimer->stop();

  m_startRecordBtn->setEnabled(true);
  m_stopRecordBtn->setEnabled(false);
  m_statusLabel->setText("预览中...");
  m_recordingLabel->setText(""); // Added this line to clear recording label

  // Add to Database
  // We stored the filename in member or we can pass it via signal?
  // The 'startRecording' determined the path.
  // Let's reconstruct or use the signal 'recordingStopped'.
  // But strictly, we should do DB insert here if we have the path.
  // Actually, we can do it in the connection to recordingStopped.

  // Let's emit a signal 'videoSaved' so Library can refresh?
  // Or just insert into DB.
  emit recordingStopped();
}

void CaptureWidget::onFpsUpdated(float fps) {
  m_fpsLabel->setText(QString("FPS: %1").arg(fps, 0, 'f', 1));
}
