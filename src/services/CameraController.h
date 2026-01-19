#ifndef CAMERACONTROLLER_H
#define CAMERACONTROLLER_H

#include <QList>
#include <QObject>
#include <QString>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

/**
 * @brief 相机控制器 - 封装海康威视 SDK
 *
 * 功能：
 * - 设备枚举与连接
 * - 图像采集 (SDK 直接渲染到 HWND)
 * - 参数控制 (曝光/增益/帧率)
 * - 视频录制 (SDK 内置 AVI 编码)
 * - 单帧抓拍
 */
class CameraController : public QObject {
  Q_OBJECT

public:
  explicit CameraController(QObject *parent = nullptr);
  ~CameraController();

  // ========== 设备信息 ==========
  struct DeviceInfo {
    QString name;
    QString serialNumber;
    int index;
    int deviceType; // MV_GIGE_DEVICE or MV_USB_DEVICE
  };

  // ========== 设备管理 ==========
  static QList<DeviceInfo> enumerateDevices();
  bool open(int deviceIndex = 0);
  void close();
  bool isOpen() const { return m_isOpen; }

  // ========== 图像采集 ==========
  void setDisplayHandle(void *hwnd);
  bool startGrabbing();
  void stopGrabbing();
  bool isGrabbing() const { return m_isGrabbing; }

  // ========== 参数控制 ==========
  void setExposure(float microseconds);
  void setGain(float db);
  void setFrameRate(float fps);
  void setFrameRateEnable(bool enable);
  void setOffsetX(int offset);
  void setOffsetY(int offset);
  void setWidth(int width);
  void setHeight(int height);

  // ========== 录制功能 ==========
  bool startRecording(const QString &filePath, float fps = 23.0f,
                      int bitRateKbps = 4000);
  void stopRecording();
  bool isRecording() const { return m_isRecording; }

  // ========== 抓拍功能 ==========
  enum SnapshotFormat { FORMAT_BMP = 0, FORMAT_JPEG = 1, FORMAT_PNG = 2 };
  bool saveSnapshot(const QString &filePath,
                    SnapshotFormat format = FORMAT_JPEG, int quality = 90);

signals:
  // 状态信号
  void cameraOpened();
  void cameraClosed();
  void error(const QString &message);

  // 帧信号
  void frameRendered(int frameIndex);
  void resolutionChanged(int width, int height);

  // 参数范围信号 (在 open 成功后发出，UI 用此初始化控件)
  void exposureRangeReady(float min, float max, float current);
  void gainRangeReady(float min, float max, float current);
  void frameRateRangeReady(float min, float max, float current);
  void resolutionReady(int width, int height, int wStep, int hStep);
  void resolutionMaxReady(int widthMax, int heightMax);
  void offsetReady(int offsetX, int offsetY);
  void resultingFrameRateReady(float fps);

  // 录制信号
  void recordingStarted(const QString &filePath);
  void recordingStopped(const QString &filePath);
  void recordingError(const QString &message);

  // 抓拍信号
  void snapshotSaved(const QString &filePath);
  void snapshotError(const QString &message);

private:
  void grabLoop();

  // SDK 句柄
  void *m_cameraHandle = nullptr;
  void *m_displayHandle = nullptr;

  // 线程控制
  std::thread m_grabThread;
  std::atomic<bool> m_isOpen{false};
  std::atomic<bool> m_isGrabbing{false};
  std::atomic<bool> m_stopGrabbing{false};
  std::atomic<int> m_frameCount{0};

  // 录制状态
  std::atomic<bool> m_isRecording{false};
  std::string m_recordingPath;

  // 当前分辨率与像素格式
  int m_width = 0;
  int m_height = 0;
  int m_widthInc = 8;
  int m_heightInc = 8;
  int m_extendWidth = 0;
  int m_extendHeight = 0;
  int m_pixelType = 0;

  // 帧缓存 (用于抓拍)
  std::mutex m_frameMutex;
  std::vector<unsigned char> m_frameBuffer;
  unsigned int m_frameLen = 0;
};

#endif // CAMERACONTROLLER_H
