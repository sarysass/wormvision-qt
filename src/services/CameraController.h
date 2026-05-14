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
  // fps <= 0 时自动用相机当前的 ResultingFrameRate（修复 Phase 3 #4：原本写死 23fps）
  bool startRecording(const QString &filePath, float fps = -1.0f,
                      int bitRateKbps = 4000);
  void stopRecording();
  bool isRecording() const { return m_isRecording; }

  // 查询相机当前结果帧率（来自 SDK ResultingFrameRate）
  float currentResultingFps() const;

private:
  // SDK flush 是异步的，轮询文件大小直到 > 0 或超时再 emit stats
  void pollFlushAndEmitStats(const QString &path, qint64 ok, qint64 fail,
                             qint64 convFail, quint32 lastErr,
                             quint32 actualPixel, int retriesLeft);
public:

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
  // Phase 5：录制结束时发出帧统计（成功 vs 失败），便于诊断 0 字节录制
  // lastErrCode 最后一次 InputOneFrame 的 SDK 错误码（0 表示无失败）
  // pixelType 录制实际使用的像素类型枚举值（转换后或原始）
  // convertFail 仅记 ConvertPixelType 失败次数（与 inputFail 互斥）
  void recordingStats(qint64 totalFrames, qint64 inputOk, qint64 inputFail,
                      qint64 fileBytes, quint32 lastErrCode, quint32 pixelType,
                      qint64 convertFail);

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
  std::string m_recordingPath;     // GBK bytes，给海康 SDK 的 C API 用
  QString m_recordingPathQt;       // UTF-16，给 Qt（QFileInfo / emit signal）用
  // 互斥保护 InputOneFrame ↔ StopRecord 不能交错（避免 MV_E_CALLORDER）
  std::mutex m_recordMutex;
  // Phase 5：录制统计 + 像素转换缓冲区
  std::atomic<qint64> m_recordInputOk{0};
  std::atomic<qint64> m_recordInputFail{0};
  std::atomic<qint64> m_recordConvertFail{0};
  std::atomic<quint32> m_lastInputErrorCode{0};
  bool m_recordingNeedsConvert = false; // 是否需要先 ConvertPixelType
  quint32 m_recordingActualPixelType = 0; // 实际录制用的像素类型
  std::vector<unsigned char> m_convertBuffer;

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
