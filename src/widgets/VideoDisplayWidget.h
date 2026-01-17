#ifndef VIDEODISPLAYWIDGET_H
#define VIDEODISPLAYWIDGET_H

#include <QImage>
#include <QMutex>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QOpenGLWidget>
#include <QTimer>

/**
 * @brief OpenGL 视频显示组件
 *
 * 使用 OpenGL 纹理渲染相机图像，实现高性能预览。
 * 支持：
 * - 实时帧更新
 * - 缩放和平移
 * - 测试图片循环播放（用于开发调试）
 */
class VideoDisplayWidget : public QOpenGLWidget, protected QOpenGLFunctions {
  Q_OBJECT

public:
  explicit VideoDisplayWidget(QWidget *parent = nullptr);
  ~VideoDisplayWidget();

  /**
   * @brief 更新显示帧
   * @param image 要显示的图像
   */
  void updateFrame(const QImage &image);

  /**
   * @brief 从原始数据更新帧
   * @param data 图像数据
   * @param width 宽度
   * @param height 高度
   * @param format 像素格式 (RGB888, Grayscale8 等)
   */
  void updateFrameFromData(const uchar *data, int width, int height,
                           QImage::Format format);

  /**
   * @brief 开始测试模式 - 循环播放测试图片
   * @param fps 帧率
   */
  void startTestMode(int fps = 10);

  /**
   * @brief 停止测试模式
   */
  void stopTestMode();

  /**
   * @brief 设置缩放级别
   * @param scale 缩放比例 (1.0 = 100%)
   */
  void setZoomLevel(float scale);

  /**
   * @brief 获取当前缩放级别
   */
  float zoomLevel() const { return m_zoomLevel; }

  /**
   * @brief 获取当前显示的帧
   */
  QImage getCurrentFrame() const {
    QMutexLocker locker(&m_frameMutex);
    return m_currentFrame;
  }

signals:
  /**
   * @brief 帧率更新信号
   * @param fps 当前帧率
   */
  void fpsUpdated(float fps);

public slots:
  /**
   * @brief 设置状态栏信息
   */
  void setOnlineStatus(bool online);
  void setResolutionInfo(int width, int height);
  void setFrameCountInfo(int count);

protected:
  void initializeGL() override;
  void paintGL() override;
  void resizeGL(int w, int h) override;
  void wheelEvent(QWheelEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
  void onTestTimerTimeout();
  void updateFPS();

private:
  void createTexture();
  void updateTexture();
  void generateTestImages();
  QImage generateColorImage(int index);

  // OpenGL 资源
  GLuint m_textureId = 0;
  bool m_textureNeedsUpdate = false;

  // 当前帧
  QImage m_currentFrame;
  mutable QMutex m_frameMutex;

  // 显示参数
  float m_zoomLevel = 1.0f;
  QPointF m_panOffset{0, 0};
  QPoint m_lastMousePos;
  bool m_isPanning = false;

  // 测试模式
  QTimer *m_testTimer = nullptr;
  QVector<QImage> m_testImages;
  int m_testImageIndex = 0;

  // FPS 计算
  QTimer *m_fpsTimer = nullptr;
  int m_frameCount = 0;
  float m_currentFps = 0.0f;

  // 状态栏信息
  bool m_isOnline = false;
  int m_resWidth = 0;
  int m_resHeight = 0;
  int m_totalFrames = 0;
};

#endif // VIDEODISPLAYWIDGET_H
