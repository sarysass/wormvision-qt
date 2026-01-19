#ifndef CAPTUREWIDGET_H
#define CAPTUREWIDGET_H

#include <QScrollArea>

#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

class VideoDisplayWidget;
class ControlPanelWidget;
class CameraController;

/**
 * @brief 实时采集与录制界面
 */
class CaptureWidget : public QWidget {
  Q_OBJECT

public:
  explicit CaptureWidget(QWidget *parent = nullptr);
  ~CaptureWidget();

signals:
  void recordingStopped();

private slots:
  void onStartPreviewClicked();
  void onStopPreviewClicked();
  void onCaptureSnapshotClicked();
  void onStartRecordingClicked();
  void onStopRecordingClicked();
  void onFpsUpdated(float fps);
  void onRefreshDevicesClicked();
  void onDeviceSelectionChanged(int index);
  void onRecordTimerTimeout();
  void updateVideoLayout();
  // Zoom slots
  void onZoomInClicked();
  void onZoomOutClicked();
  void onFitWindowClicked();
  void onVideoWheelEvent(QWheelEvent *event);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void showEvent(QShowEvent *event) override;
  void hideEvent(QHideEvent *event) override;

private:
  void setupUI();
  void setupConnections();

  QWidget *m_videoContainer = nullptr;
  VideoDisplayWidget *m_videoDisplay = nullptr;
  ControlPanelWidget *m_controlPanel = nullptr;
  CameraController *m_camera = nullptr;

  // 工具栏控件
  QPushButton *m_startPreviewBtn = nullptr;
  QPushButton *m_stopPreviewBtn = nullptr;
  QPushButton *m_snapshotBtn = nullptr;
  QPushButton *m_startRecordBtn = nullptr;
  QPushButton *m_stopRecordBtn = nullptr;
  QLineEdit *m_taskInfoEdit = nullptr;

  // 缩放控制
  QPushButton *m_zoomInBtn = nullptr;
  QPushButton *m_zoomOutBtn = nullptr;
  QPushButton *m_fitWindowBtn = nullptr;
  QLabel *m_zoomLabel = nullptr;
  QScrollArea *m_scrollArea = nullptr;
  double m_currentZoom = -1.0;

  // 状态显示
  QLabel *m_fpsLabel = nullptr;
  QLabel *m_frameCountLabel = nullptr;
  QLabel *m_statusLabel = nullptr;
  QLabel *m_recordingLabel = nullptr;
  QLabel *m_resolutionLabel = nullptr;

  // 设备列表引用 (对应 ControlPanel 中的控件)
  QComboBox *m_deviceCombo = nullptr;
  QPushButton *m_refreshDevicesBtn = nullptr;

  QTimer *m_recordTimer = nullptr;
  QDateTime m_recordStartTime;
  bool m_isPreviewActive = false;
  int m_selectedDeviceIndex = -1;
};

#endif // CAPTUREWIDGET_H
