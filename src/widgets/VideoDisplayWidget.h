#ifndef VIDEODISPLAYWIDGET_H
#define VIDEODISPLAYWIDGET_H

#include <QElapsedTimer>
#include <QResizeEvent>
#include <QSize>
#include <QTimer>
#include <QWidget>

/**
 * @brief SDK 直接渲染视频显示组件
 *
 * 使用海康 SDK 的 MV_CC_DisplayOneFrameEx 直接渲染到窗口句柄。
 */
class VideoDisplayWidget : public QWidget {
  Q_OBJECT

public:
  explicit VideoDisplayWidget(QWidget *parent = nullptr);
  ~VideoDisplayWidget();

  /**
   * @brief 获取原生窗口句柄用于 SDK 直接渲染
   */
  void *getNativeHandle();

  /**
   * @brief 通知帧已更新 (用于 EMA FPS 统计)
   */
  void notifyFrameRendered();

  /**
   * @brief 强制清空显示内容 (黑屏)
   */
  void clear();

  /**
   * @brief 设置图像原始尺寸 (用于保持纵横比)
   */
  void setImageSize(int width, int height);

  /**
   * @brief 设置当前是否正在进行视频流渲染
   *
   * 当正在推流时，禁止 paintEvent 强制刷黑，避免黑屏闪烁
   */
  void setStreaming(bool streaming);

  // 保持纵横比的 sizeHint
  QSize sizeHint() const override;
  bool hasHeightForWidth() const override { return true; }
  int heightForWidth(int w) const override;

signals:
  void fpsUpdated(float fps);
  void imageSizeChanged(int width, int height);
  void wheelEventTriggered(QWheelEvent *event);

protected:
  // 禁止 Qt 绘制，完全交给 SDK
  QPaintEngine *paintEngine() const override { return nullptr; }
  void resizeEvent(QResizeEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;

private slots:
  void emitFps();

private:
  QTimer *m_fpsEmitTimer = nullptr;        // 定期发送 FPS 信号
  QElapsedTimer m_frameTimer;              // 测量帧间隔
  float m_smoothFps = 0.0f;                // EMA 平滑后的 FPS
  bool m_firstFrame = true;                // 是否第一帧
  static constexpr float EMA_ALPHA = 0.1f; // EMA 平滑系数

  QSize m_imageSize;          // 图像原始尺寸
  bool m_isStreaming = false; // 是否正在采集/预览
};

#endif // VIDEODISPLAYWIDGET_H
