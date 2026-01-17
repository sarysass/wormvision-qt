#ifndef CAPTUREWIDGET_H
#define CAPTUREWIDGET_H

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

class VideoDisplayWidget;
class ControlPanelWidget;
class CameraController;

/**
 * @brief 采集视图主控件
 *
 * 包含：
 * - 视频显示区域 (VideoDisplayWidget)
 * - 控制面板 (ControlPanelWidget)
 * - 录制状态显示
 */
class CaptureWidget : public QWidget {
  Q_OBJECT

public:
  explicit CaptureWidget(QWidget *parent = nullptr);
  ~CaptureWidget();

signals:
  void recordingStarted();
  void recordingStopped();

private slots:
  void onStartPreviewClicked();
  void onStopPreviewClicked();
  void onCaptureSnapshotClicked();
  void onStartRecordingClicked();
  void onStopRecordingClicked();
  void onFpsUpdated(float fps);

private:
  void setupUI();
  void setupConnections();

  // UI 组件
  VideoDisplayWidget *m_videoDisplay;
  ControlPanelWidget *m_controlPanel;

  // 状态栏
  QLabel *m_statusLabel;
  QLabel *m_fpsLabel;
  QLabel *m_frameCountLabel;
  QLabel *m_recordingLabel;

  // 按钮
  QPushButton *m_startPreviewBtn;
  QPushButton *m_stopPreviewBtn;
  QPushButton *m_snapshotBtn;
  QPushButton *m_startRecordBtn;
  QPushButton *m_stopRecordBtn;
  QLineEdit *m_taskInfoEdit;
  class VideoRecorder *m_recorder = nullptr; // Forward declaration

  bool m_isPreviewActive = false;
  bool m_isRecording = false;

  // Timer for recording duration
  QTimer *m_recordTimer;
  QDateTime m_recordStartTime;

  CameraController *m_camera;
};

#endif // CAPTUREWIDGET_H
