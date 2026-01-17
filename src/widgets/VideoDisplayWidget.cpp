#include "VideoDisplayWidget.h"
#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <cmath>

VideoDisplayWidget::VideoDisplayWidget(QWidget *parent)
    : QOpenGLWidget(parent), m_textureId(0), m_zoomLevel(1.0f) {
  // 设置 OpenGL 格式
  QSurfaceFormat format;
  format.setDepthBufferSize(24);
  format.setStencilBufferSize(8);
  format.setVersion(3, 3);
  format.setProfile(QSurfaceFormat::CompatibilityProfile);
  setFormat(format);

  // FPS 计时器
  m_fpsTimer = new QTimer(this);
  connect(m_fpsTimer, &QTimer::timeout, this, &VideoDisplayWidget::updateFPS);
  m_fpsTimer->start(1000); // 每秒更新一次

  // 生成测试图片
  generateTestImages();

  // 防止 Qt 背景自动填充干扰 OpenGL 绘制
  setAutoFillBackground(false);
}

VideoDisplayWidget::~VideoDisplayWidget() {
  makeCurrent();
  if (m_textureId) {
    glDeleteTextures(1, &m_textureId);
  }
  doneCurrent();
}

void VideoDisplayWidget::initializeGL() {
  initializeOpenGLFunctions();

  // 设置清除颜色（深色背景）
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

  // 创建纹理
  createTexture();

  qDebug() << "OpenGL 初始化完成，版本:"
           << (const char *)glGetString(GL_VERSION);
}

void VideoDisplayWidget::paintGL() {
  glClear(GL_COLOR_BUFFER_BIT);

  QMutexLocker locker(&m_frameMutex);

  if (m_currentFrame.isNull()) {
    return;
  }

  // 更新纹理
  if (m_textureNeedsUpdate) {
    updateTexture();
    m_textureNeedsUpdate = false;
  }

  // 计算适配比例
  float imgAspect = (float)m_currentFrame.width() / m_currentFrame.height();
  float winAspect = (float)width() / height();

  float scaleX = 1.0f;
  float scaleY = 1.0f;

  if (winAspect > imgAspect) {
    // 窗口更宽，由于 GL 坐标归一化，需要缩窄 X
    scaleX = imgAspect / winAspect;
  } else {
    // 窗口更高，缩窄 Y
    scaleY = winAspect / imgAspect;
  }

  // 应用缩放和平移
  scaleX *= m_zoomLevel;
  scaleY *= m_zoomLevel;

  // 简单的固定管线绘制 (OpenGL 3.3 应使用
  // Shaders，但为了兼容性或简单实现，也可以用 QPainter 绘制纹理) 或者使用
  // QPainter + Native Texture? 既然我们已经有了纹理，用 QPainter 绘制纹理比
  // drawImage(QImage) 快得多。

  QPainter painter(this);
  painter.beginNativePainting();

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, m_textureId);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

  // 绘制全屏四边形 (带纹理坐标)
  // 注意：需要转换坐标系
  // 简单方案：继续使用 QPainter，但绘制 OpenGL 纹理？
  // Qt 的 QOpenGLWidget 允许混合。
  // 但是为了性能最大化，我们应该手动画 Quad。
  // 这里为了稳妥，且之前用 QPainter 绘制 QImage 太慢，我们这次用 QPainter 绘制
  // *Texture* 不行吗？ QPainter 不直接支持画 GL Texture (除非用
  // QOpenGLPaintDevice)。 回退到：用 QPainter 画，但是底层是 QImage。
  // 等等，之前的性能问题是因为 updateTexture + QPainter(Image) 双重开销。
  // 现在我们恢复了 updateTexture。我们必须用 OpenGL 画。
  // 让我们写一段简单的 OpenGL 代码画纹理。

  glLoadIdentity();
  glScalef(scaleX, scaleY, 1.0f);
  glTranslatef(m_panOffset.x() / (width() / 2.0f),
               -m_panOffset.y() / (height() / 2.0f), 0.0f); // 简单映射

  glColor3f(1.0f, 1.0f, 1.0f);
  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 0.0f);
  glVertex2f(-1.0f, 1.0f); // Top Left
  glTexCoord2f(1.0f, 0.0f);
  glVertex2f(1.0f, 1.0f); // Top Right
  glTexCoord2f(1.0f, 1.0f);
  glVertex2f(1.0f, -1.0f); // Bottom Right
  glTexCoord2f(0.0f, 1.0f);
  glVertex2f(-1.0f, -1.0f); // Bottom Left
  glEnd();

  glEnd();

  glDisable(GL_TEXTURE_2D);

  // 恢复像素对齐默认值 (4)，否则 QPainter 的字体渲染可能会乱码
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

  painter.endNativePainting();

  // 绘制 OSD - 状态栏背景 (半透明)
  QRect statusRect(10, 5, 350, 30);
  painter.fillRect(statusRect, QColor(0, 0, 0, 150));
  painter.setRenderHint(QPainter::Antialiasing);

  // 在线状态指示器
  int dotX = 20;
  int dotY = 15;
  painter.setPen(Qt::NoPen);
  painter.setBrush(m_isOnline ? Qt::green : Qt::red);
  painter.drawEllipse(dotX, dotY, 10, 10);

  // 状态文本
  painter.setPen(Qt::white);
  painter.setFont(QFont("Arial", 10));
  int textX = dotX + 15;
  QString statusText = m_isOnline ? "在线" : "离线";
  painter.drawText(textX, 22, statusText);

  // 分辨率
  if (m_resWidth > 0 && m_resHeight > 0) {
    painter.drawText(textX + 40, 22,
                     QString("%1 x %2").arg(m_resWidth).arg(m_resHeight));
  }

  // FPS
  painter.drawText(textX + 135, 22,
                   QString("%1 FPS").arg(m_currentFps, 0, 'f', 0));

  // 帧数
  painter.drawText(textX + 195, 22, QString("%1 帧").arg(m_totalFrames));

  m_frameCount++;
}

void VideoDisplayWidget::resizeGL(int w, int h) { glViewport(0, 0, w, h); }

void VideoDisplayWidget::createTexture() {
  glGenTextures(1, &m_textureId);
  glBindTexture(GL_TEXTURE_2D, m_textureId);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void VideoDisplayWidget::updateTexture() {
  if (m_currentFrame.isNull())
    return;

  makeCurrent(); // Ensure GL context is active

  glBindTexture(GL_TEXTURE_2D, m_textureId);

  // 重要修复：设置像素解包对齐为 1 字节
  // 3 字节的像素，紧凑排列 -> 必须用 Alignment = 1。
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  // 直接上传 QImage 的数据 bits
  GLenum format = GL_RGB;
  if (m_currentFrame.format() == QImage::Format_RGB888) {
    format = GL_RGB;
  } else if (m_currentFrame.format() == QImage::Format_RGBA8888) {
    format = GL_RGBA;
  }

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_currentFrame.width(),
               m_currentFrame.height(), 0, format, GL_UNSIGNED_BYTE,
               m_currentFrame.bits());

  doneCurrent();
}

void VideoDisplayWidget::updateFrame(const QImage &image) {
  QMutexLocker locker(&m_frameMutex);
  m_currentFrame = image;
  m_textureNeedsUpdate = true; // Required for paintGL to upload texture
  update();                    // 请求重绘
}

void VideoDisplayWidget::updateFrameFromData(const uchar *data, int width,
                                             int height,
                                             QImage::Format format) {
  // 关键修复：显式指定 bytesPerLine 为 width * 3 (针对 RGB888)
  // 防止 QImage 默认的 4 字节对齐导致的数据错位（横纹问题）
  int bytesPerLine = width * 3;
  if (format != QImage::Format_RGB888) {
    // 如果后续支持其他格式，需在此调整 stride 计算
    bytesPerLine = 0; // 让 QImage 自动计算 (auto-align)
  }

  QImage image(data, width, height, bytesPerLine, format);
  updateFrame(image.copy()); // 复制数据
}

void VideoDisplayWidget::startTestMode(int fps) {
  if (!m_testTimer) {
    m_testTimer = new QTimer(this);
    connect(m_testTimer, &QTimer::timeout, this,
            &VideoDisplayWidget::onTestTimerTimeout);
  }

  m_testImageIndex = 0;
  m_testTimer->start(1000 / fps);
  qDebug() << "测试模式启动，帧率:" << fps << "FPS";
}

void VideoDisplayWidget::stopTestMode() {
  if (m_testTimer) {
    m_testTimer->stop();
  }
  qDebug() << "测试模式停止";
}

void VideoDisplayWidget::onTestTimerTimeout() {
  if (m_testImages.isEmpty())
    return;

  updateFrame(m_testImages[m_testImageIndex]);
  m_testImageIndex = (m_testImageIndex + 1) % m_testImages.size();
}

void VideoDisplayWidget::generateTestImages() {
  // 生成 10 张不同颜色的测试图片
  for (int i = 0; i < 10; ++i) {
    m_testImages.append(generateColorImage(i));
  }
  qDebug() << "生成了" << m_testImages.size() << "张测试图片";
}

QImage VideoDisplayWidget::generateColorImage(int index) {
  // 创建 640x480 的测试图片
  QImage image(640, 480, QImage::Format_RGB888);

  // 根据索引生成不同的颜色
  float hue = (index * 36.0f); // 每张图片相差 36 度
  QColor color = QColor::fromHslF(hue / 360.0f, 0.8f, 0.5f);
  image.fill(color);

  // 在图片上绘制帧号
  QPainter painter(&image);
  painter.setPen(Qt::white);
  painter.setFont(QFont("Arial", 48, QFont::Bold));
  painter.drawText(image.rect(), Qt::AlignCenter,
                   QString("帧 %1").arg(index + 1));

  // 绘制边框
  painter.setPen(QPen(Qt::white, 3));
  painter.drawRect(10, 10, image.width() - 20, image.height() - 20);

  return image;
}

void VideoDisplayWidget::setZoomLevel(float scale) {
  m_zoomLevel = qBound(0.1f, scale, 10.0f);
  update();
}

void VideoDisplayWidget::wheelEvent(QWheelEvent *event) {
  // 滚轮缩放
  float delta = event->angleDelta().y() / 120.0f;
  float factor = std::pow(1.1f, delta);
  setZoomLevel(m_zoomLevel * factor);
  event->accept();
}

void VideoDisplayWidget::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::MiddleButton ||
      event->button() == Qt::LeftButton) {
    m_isPanning = true;
    m_lastMousePos = event->pos();
    setCursor(Qt::ClosedHandCursor);
  }
}

void VideoDisplayWidget::mouseMoveEvent(QMouseEvent *event) {
  if (m_isPanning) {
    QPoint delta = event->pos() - m_lastMousePos;
    m_panOffset += QPointF(delta);
    m_lastMousePos = event->pos();
    update();
  }
}

void VideoDisplayWidget::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::MiddleButton ||
      event->button() == Qt::LeftButton) {
    m_isPanning = false;
    setCursor(Qt::ArrowCursor);
  }
}

void VideoDisplayWidget::updateFPS() {
  m_currentFps = static_cast<float>(m_frameCount);
  m_frameCount = 0;
  emit fpsUpdated(m_currentFps);
}

void VideoDisplayWidget::setOnlineStatus(bool online) {
  m_isOnline = online;
  update();
}

void VideoDisplayWidget::setResolutionInfo(int width, int height) {
  m_resWidth = width;
  m_resHeight = height;
  update();
}

void VideoDisplayWidget::setFrameCountInfo(int count) {
  m_totalFrames = count;
  // Don't call update() here - called too frequently during streaming
}
