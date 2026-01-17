#ifndef CAMERACONTROLLER_H
#define CAMERACONTROLLER_H

#include <QByteArray>
#include <QImage>
#include <QObject>
#include <QString>
#include <QThread>
#include <atomic>
#include <mutex>
#include <thread>

/**
 * @brief 相机控制器
 *
 * 封装海康威视 SDK 调用，提供：
 * - 相机枚举和连接
 * - 图像采集
 * - 参数设置
 */
class CameraController : public QObject {
  Q_OBJECT

public:
  explicit CameraController(QObject *parent = nullptr);
  ~CameraController();

  // 相机操作
  bool open();
  void close();
  bool startGrabbing();
  void stopGrabbing();
  bool isOpen() const { return m_isOpen; }
  bool isGrabbing() const { return m_isGrabbing; }

  // 参数设置
  void setExposure(float microseconds);
  void setGain(float db);
  void setFrameRate(float fps);
  void setBinning(int factor);

  // 参数获取
  float exposure() const { return m_exposure; }
  float gain() const { return m_gain; }
  float frameRate() const { return m_frameRate; }
  int binning() const { return m_binning; }

  // 获取参数范围 (从 SDK 读取)
  struct ParameterRange {
    float min;
    float max;
    float current;
  };
  ParameterRange getExposureRange() const;
  ParameterRange getGainRange() const;
  ParameterRange getFrameRateRange() const;

signals:
  void frameReady(const QImage &frame, int frameIdx);
  void cameraOpened();
  void cameraClosed();
  void error(const QString &message);
  void parameterChanged(const QString &name, float value);
  void exposureRangeReady(float min, float max, float current);
  void gainRangeReady(float min, float max, float current);
  void frameRateRangeReady(float min, float max, float current);

private slots:
  void grabLoop();

private:
  void initSDK();
  void uninitSDK();

  void *m_cameraHandle = nullptr;
  std::thread m_grabThread;
  std::atomic<bool> m_isOpen{false};
  std::atomic<bool> m_isGrabbing{false};
  std::atomic<bool> m_stopRequested{false};
  std::atomic<int> m_frameCount{0};

  unsigned char *m_pDataBuf = nullptr; // 数据缓存
  unsigned int m_nDataBufSize = 0;

  // 参数缓存
  float m_exposure = 10000.0f;
  float m_gain = 0.0f;
  float m_frameRate = 23.0f;
  int m_binning = 1;
};

#endif // CAMERACONTROLLER_H
