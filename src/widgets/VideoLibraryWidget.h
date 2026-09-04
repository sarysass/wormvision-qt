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

public slots:
  void setCaptureBusy(bool busy);
  void setAnalysisBusy(bool busy);

signals:
  void analysisRequested(const QStringList &paths);

private slots:
  void onRefreshClicked();
  void onOpenFolderClicked();
  void onSelectStorageRootClicked();
  void onTableDoubleClicked(int row, int column);
  void onContextMenuRequested(const QPoint &pos);

  // Context menu actions
  void onPlayAction();
  void onDeleteAction();
  void onRenameAction();
  void requestAnalysis();
  void onBatchDeleteClicked();

private:
  void setupUI();
  void setupConnections();
  void scanVideoFolder(); // Helper to scan folder and update DB
  void updateOperationState();
  QString formatDuration(double seconds);
  QString formatFileSize(qint64 bytes);
  double getVideoDuration(const QString &filepath);

  QTableWidget *m_tableWidget;
  QPushButton *m_refreshBtn;
  QPushButton *m_openFolderBtn;
  QPushButton *m_selectStorageRootBtn;
  QPushButton *m_analyzeBtn;
  QPushButton *m_batchDeleteBtn;
  QLabel *m_statusLabel;
  bool m_captureBusy = false;
  bool m_analysisBusy = false;
};

#endif // VIDEOLIBRARYWIDGET_H
