#include "VideoDisplayWidget.h"
#include <QDebug>
#include <QPalette>
#include <QWheelEvent>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

VideoDisplayWidget::VideoDisplayWidget(QWidget *parent) : QWidget(parent) {
  // 设置自动填充背景，跟随主题
  setAutoFillBackground(true);

  // 确保获得原生窗口句柄，禁止 Qt 的绘制系统
  setAttribute(Qt::WA_NativeWindow);
  setAttribute(Qt::WA_PaintOnScreen);
  setAttribute(Qt::WA_NoSystemBackground);
  setAttribute(Qt::WA_OpaquePaintEvent);

  // 初始化帧计时器
  m_frameTimer.start();

  // FPS 发送定时器
  m_fpsEmitTimer = new QTimer(this);
  connect(m_fpsEmitTimer, &QTimer::timeout, this, &VideoDisplayWidget::emitFps);
  m_fpsEmitTimer->start(200);

  // 初始最小大小
  setMinimumSize(640, 480);
}

VideoDisplayWidget::~VideoDisplayWidget() {}

void *VideoDisplayWidget::getNativeHandle() {
#ifdef Q_OS_WIN
  return reinterpret_cast<void *>(winId());
#else
  return nullptr;
#endif
}

void VideoDisplayWidget::notifyFrameRendered() {
  qint64 elapsed = m_frameTimer.restart();
  if (m_firstFrame) {
    m_firstFrame = false;
    return;
  }
  if (elapsed > 0) {
    float instantFps = 1000.0f / static_cast<float>(elapsed);
    if (m_smoothFps < 0.1f) {
      m_smoothFps = instantFps;
    } else {
      m_smoothFps = EMA_ALPHA * instantFps + (1.0f - EMA_ALPHA) * m_smoothFps;
    }
  }
}

void VideoDisplayWidget::clear() {
#ifdef Q_OS_WIN
  HWND hwnd = reinterpret_cast<HWND>(winId());
  if (hwnd) {
    HDC hdc = GetDC(hwnd);
    RECT rect;
    GetClientRect(hwnd, &rect);

    // 获取当前窗口背景色
    QColor bgColor = palette().color(QPalette::Window);
    HBRUSH hBrush =
        CreateSolidBrush(RGB(bgColor.red(), bgColor.green(), bgColor.blue()));

    FillRect(hdc, &rect, hBrush);
    DeleteObject(hBrush);
    ReleaseDC(hwnd, hdc);
  }
#endif
}

void VideoDisplayWidget::emitFps() { emit fpsUpdated(m_smoothFps); }

void VideoDisplayWidget::setImageSize(int width, int height) {
  if (m_imageSize.width() != width || m_imageSize.height() != height) {
    m_imageSize = QSize(width, height);
    updateGeometry();
    emit imageSizeChanged(width, height);
  }
}

void VideoDisplayWidget::setStreaming(bool streaming) {
  m_isStreaming = streaming;
}

QSize VideoDisplayWidget::sizeHint() const {
  if (m_imageSize.isValid()) {
    return m_imageSize;
  }
  return QSize(640, 480);
}

int VideoDisplayWidget::heightForWidth(int w) const {
  if (!m_imageSize.isValid() || m_imageSize.width() == 0) {
    return w * 3 / 4;
  }
  return w * m_imageSize.height() / m_imageSize.width();
}

void VideoDisplayWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  // 调整大小时强制刷黑，避免白屏闪烁
  // 但如果在播放视频，不要刷黑，否则会闪烁
  if (!m_isStreaming) {
    clear();
  }
}

void VideoDisplayWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  // 触发重绘时（如窗口暴露），如果没有在播放，强制黑屏
  // 如果正在播放，绝对不要刷黑，否则会和 SDK 渲染冲突导致闪烁
  if (!m_isStreaming) {
    clear();
  }
}

void VideoDisplayWidget::wheelEvent(QWheelEvent *event) {
  emit wheelEventTriggered(event);
}
