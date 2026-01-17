#include "VideoRecorder.h"
#include <QDebug>
#include <QFileInfo>

VideoRecorder::VideoRecorder(QObject *parent) : QObject(parent) {}

VideoRecorder::~VideoRecorder() { stopRecording(); }

bool VideoRecorder::startRecording(const QString &filePath, int width,
                                   int height, double fps) {
  if (m_isRecording)
    return false;

  m_width = width;
  m_height = height;
  m_currentFile = filePath;

  // Ensure directory exists
  QString dir = QFileInfo(filePath).absolutePath();
  // QDir().mkpath(dir); // Caller should handle? Or we handle.

  // Initialize VideoWriter
  // Use H.264 for better compression (smaller files, same quality)
  // H264/X264 codec identifier varies by platform:
  // Windows: 'H264', 'X264', 'avc1'
  int codec = cv::VideoWriter::fourcc('a', 'v', 'c', '1');
  // On Windows, filename must be strictly compliant. OpenCV handles
  // std::string. Use generic string for path (Utf-8)

  // Note: OpenCV expects BGR usually. Our source is RGB (from
  // CameraController). so we need conversion.

  // cv::Size(width, height)

  try {
    // On Windows, OpenCV doesn't handle UTF-8 paths well. Use local 8-bit
    // encoding. For better compatibility, we convert the QString to local 8-bit
    // encoding.
    std::string localPath = filePath.toLocal8Bit().constData();
    if (!m_writer.open(localPath, codec, fps, cv::Size(width, height), true)) {
      emit recordingError("无法打开视频文件进行写入");
      return false;
    }
  } catch (const cv::Exception &e) {
    emit recordingError(QString("OpenCV Error: %1").arg(e.what()));
    return false;
  }

  m_isRecording = true;
  m_stopRequested = false;

  // Start thread
  m_writeThread = std::thread(&VideoRecorder::writeLoop, this);

  return true;
}

void VideoRecorder::stopRecording() {
  if (!m_isRecording)
    return;

  m_stopRequested = true;
  m_cv.notify_all();

  if (m_writeThread.joinable()) {
    m_writeThread.join();
  }

  m_isRecording = false;
  m_writer.release();

  emit recordingStopped(m_currentFile);
}

bool VideoRecorder::isRecording() const { return m_isRecording; }

void VideoRecorder::processFrame(const QImage &frame) {
  if (!m_isRecording || m_stopRequested)
    return;

  if (frame.width() != m_width || frame.height() != m_height) {
    return;
  }

  // QImage is implicitly shared. Passing by value (or const ref then queuing
  // copy) acts as a shared pointer. But the source image from CameraController
  // is likely being reused or destructed. CameraController already emits a deep
  // copy (finalImage). So holding it here is safe.

  QMutexLocker locker(&m_mutex);
  if (m_frameQueue.size() > 30) {
    return;
  }

  m_frameQueue.enqueue(frame);
  m_cv.notify_one();
}

void VideoRecorder::writeLoop() {
  while (m_isRecording) {
    QImage frameImg;
    {
      std::unique_lock<std::mutex> lock(m_stdMutex);
      m_cv.wait(lock,
                [this] { return !m_frameQueue.isEmpty() || m_stopRequested; });

      if (m_stopRequested && m_frameQueue.isEmpty()) {
        break;
      }

      if (!m_frameQueue.isEmpty()) {
        QMutexLocker qlock(&m_mutex);
        if (!m_frameQueue.isEmpty())
          frameImg = m_frameQueue.dequeue();
      }
    }

    if (!frameImg.isNull()) {
      // Convert QImage to OpenCV Mat
      // QImage is Format_RGB888 (3 bytes).
      // cv::Mat constructor takes (rows, cols, type, data, step).
      // QImage::bits() returns pointer to first pixel.
      // QImage aligns lines to 4 bytes by default (bytesPerLine).
      // Mat needs to handle this.

      // IMPORTANT: QImage ownership is in 'frameImg'. It must persist during
      // Mat usage.

      cv::Mat rgbFrame(frameImg.height(), frameImg.width(), CV_8UC3,
                       (void *)frameImg.bits(), frameImg.bytesPerLine());

      cv::Mat bgrFrame;
      // Efficient Color Conversion
      cv::cvtColor(rgbFrame, bgrFrame, cv::COLOR_RGB2BGR);

      if (m_writer.isOpened()) {
        m_writer.write(bgrFrame);
      }
    }
  }
}
