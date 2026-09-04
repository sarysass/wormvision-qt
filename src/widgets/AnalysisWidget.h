#ifndef ANALYSISWIDGET_H
#define ANALYSISWIDGET_H

#include "utils/AnalysisResults.h"

#include <QJsonObject>
#include <QWidget>

class LocalAnalysisService;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
class QShowEvent;
class QTableWidget;
class QTabWidget;
class QTimer;

/** 视频库的本地分析工作台，任务执行交给已有引擎。 */
class AnalysisWidget : public QWidget {
  Q_OBJECT
public:
  explicit AnalysisWidget(QWidget *parent = nullptr);
  void setSelectedVideos(const QStringList &paths);
  void setCaptureBusy(bool busy);
  bool hasActiveAnalysis() const;

signals:
  void busyChanged(bool busy);

protected:
  void showEvent(QShowEvent *event) override;

private:
  void setupUI();
  void updateControls();
  void setBusyState();
  void configureEngine();
  void activateLicense();
  void refreshLicense();
  void refreshHistory();
  void startAnalysis();
  void cancelAnalysis();
  void pollRuns();
  void selectRun(const QString &id);
  void showRun(const QJsonObject &run);
  void loadResults(const QString &id, quint64 generation);
  void clearResults();
  void showVideoTable();
  void showPreview();
  void openOutput(const QString &relativePath);
  void fillTable(QTableWidget *table, const AnalysisResults::Table &data);

  LocalAnalysisService *m_service = nullptr;
  QTimer *m_pollTimer = nullptr;
  QListWidget *m_videos = nullptr;
  QListWidget *m_history = nullptr;
  QComboBox *m_protocol = nullptr;
  QComboBox *m_device = nullptr;
  QCheckBox *m_calibrated = nullptr;
  QDoubleSpinBox *m_pixelToMm = nullptr;
  QPushButton *m_start = nullptr;
  QPushButton *m_cancel = nullptr;
  QPushButton *m_configure = nullptr;
  QPushButton *m_activate = nullptr;
  QPushButton *m_refresh = nullptr;
  QLabel *m_captureStatus = nullptr;
  QLabel *m_serviceStatus = nullptr;
  QLabel *m_licenseStatus = nullptr;
  QLabel *m_activeStatus = nullptr;
  QLabel *m_resultStatus = nullptr;
  QProgressBar *m_progress = nullptr;
  QTabWidget *m_tabs = nullptr;
  QTableWidget *m_summaryTable = nullptr;
  QTableWidget *m_trackTable = nullptr;
  QComboBox *m_videoSelector = nullptr;
  QComboBox *m_previewSelector = nullptr;
  QLabel *m_preview = nullptr;
  QComboBox *m_reviewSelector = nullptr;
  QPushButton *m_openReview = nullptr;
  QPushButton *m_openExcel = nullptr;
  QPushButton *m_openPdf = nullptr;
  QPushButton *m_openFolder = nullptr;
  AnalysisResults::Report m_report;
  QJsonObject m_license;
  QString m_outputDir;
  QString m_excelPath;
  QString m_pdfPath;
  QString m_currentRunId;
  QString m_currentState;
  QString m_loadedRunId;
  QString m_activeJobId;
  quint64 m_selectionGeneration = 0;
  quint64 m_historyGeneration = 0;
  bool m_submitting = false;
  bool m_captureBusy = false;
  bool m_reportedBusy = false;
  bool m_activePolling = false;
  bool m_selectedPolling = false;
  bool m_cancelPending = false;
};

#endif // ANALYSISWIDGET_H
