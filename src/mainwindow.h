#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QAction>
#include <QMainWindow>
#include <QStackedWidget>
#include <QToolBar>

class CaptureWidget;
class VideoLibraryWidget;
class AnalysisWidget;
class DatabaseManager;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

private slots:
  void showCaptureView();
  void showLibraryView();
  void showAnalysisView();
  void toggleTheme();

protected:
  void closeEvent(QCloseEvent *event) override;

private:
  void setupUI();
  void setupToolBar();
  void setupConnections();
  void loadStyleSheet(const QString &theme);

  QStackedWidget *m_centralStack;
  CaptureWidget *m_captureWidget;
  VideoLibraryWidget *m_libraryWidget;
  AnalysisWidget *m_analysisWidget = nullptr;

  QToolBar *m_toolBar;
  QAction *m_captureAction;
  QAction *m_libraryAction;
  QAction *m_analysisAction = nullptr;
  QAction *m_themeAction;

  bool m_isDarkTheme = true;
};

#endif // MAINWINDOW_H
