#ifndef VIDEORECORDER_H
#define VIDEORECORDER_H

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QThread>
#include <atomic>
#include <condition_variable>


// OpenCV forward declaration if possible, but cv::VideoWriter needs header
// usually. We will include opencv header in cpp to avoid polluting global
// namespace if possible, but we need members. For now, we'll try to use PImpl
// or just include. Since we are waiting for install, we'll include it. Note: If
// build fails finding it, we will know.

#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>

class VideoRecorder : public QObject {
  Q_OBJECT

public:
  explicit VideoRecorder(QObject *parent = nullptr);
  ~VideoRecorder();

  bool startRecording(const QString &filePath, int width, int height,
                      double fps);
  void stopRecording();
  bool isRecording() const;

  // Use a slot to receive frames
public slots:
  void processFrame(const QImage &frame);

signals:
  void recordingStopped(const QString &filePath);
  void recordingError(const QString &msg);

private:
  void writeLoop();

  std::atomic<bool> m_isRecording{false};
  std::atomic<bool> m_stopRequested{false};

  cv::VideoWriter m_writer;

  // Frame Queue for buffering
  QQueue<QImage> m_frameQueue;
  QMutex m_mutex;
  std::condition_variable m_cv;
  std::mutex m_stdMutex; // for cv

  std::thread m_writeThread;

  int m_width;
  int m_height;
  QString m_currentFile;
};

#endif // VIDEORECORDER_H
