#ifndef VIDEOLIBRARYWIDGET_H
#define VIDEOLIBRARYWIDGET_H

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>


class VideoLibraryWidget : public QWidget {
  Q_OBJECT

public:
  explicit VideoLibraryWidget(QWidget *parent = nullptr);
  ~VideoLibraryWidget();

  // Refresh the list from database
  void refreshLibrary();
  // 切换到视频库视图时调用：扫目录 + 清脏数据 + 重读 DB
  void rescanAndRefresh();

private slots:
  void onRefreshClicked();
  void onOpenFolderClicked();
  void onTableDoubleClicked(int row, int column);
  void onContextMenuRequested(const QPoint &pos);

  // Context menu actions
  void onPlayAction();
  void onDeleteAction();
  void onRenameAction();
  void onUploadAction();
  void onBatchDeleteClicked();
  void onBatchUploadClicked();

private:
  void setupUI();
  void setupConnections();
  void scanVideoFolder(); // Helper to scan folder and update DB
  QString formatDuration(double seconds);
  QString formatFileSize(qint64 bytes);
  double getVideoDuration(const QString &filepath);

  QTableWidget *m_tableWidget;
  QPushButton *m_refreshBtn;
  QPushButton *m_openFolderBtn;
  QPushButton *m_batchUploadBtn;
  QPushButton *m_batchDeleteBtn;
  QLabel *m_statusLabel;
};

#endif // VIDEOLIBRARYWIDGET_H
