#include "VideoLibraryWidget.h"
#include "../data/DatabaseManager.h"
#include "../data/VideoLibraryService.h"
#include "../utils/AppPaths.h"
#include "../utils/VideoUtils.h"
#include <QAction>
#include <QCoreApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

VideoLibraryWidget::VideoLibraryWidget(QWidget *parent) : QWidget(parent) {
  // P3：背景色统一交给 QSS 管理（原来硬编码 30,30,30 与主题 token 不一致）
  setAutoFillBackground(true);

  setupUI();
  setupConnections();

  // Phase 2 重构：DB 初始化挪到 main.cpp，此处不再重复初始化
  // Initial scan
  scanVideoFolder();
  refreshLibrary();
}

VideoLibraryWidget::~VideoLibraryWidget() {}

void VideoLibraryWidget::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(10, 10, 10, 10);
  mainLayout->setSpacing(10);

  // Toolbar
  QHBoxLayout *toolbarLayout = new QHBoxLayout();
  m_refreshBtn = new QPushButton("刷新", this);
  m_openFolderBtn = new QPushButton("打开文件夹", this);
  m_selectStorageRootBtn = new QPushButton("选择保存位置", this);
  m_analyzeBtn = new QPushButton("分析选中", this);
  m_analyzeBtn->setObjectName("primaryButton");
  m_batchDeleteBtn = new QPushButton("删除选中", this);
  m_batchDeleteBtn->setObjectName("dangerButton");

  toolbarLayout->addWidget(m_refreshBtn);
  toolbarLayout->addWidget(m_openFolderBtn);
  toolbarLayout->addWidget(m_selectStorageRootBtn);
  toolbarLayout->addStretch();
  toolbarLayout->addWidget(m_analyzeBtn);
  toolbarLayout->addWidget(m_batchDeleteBtn);

  mainLayout->addLayout(toolbarLayout);

  // Table Widget (replacing QListWidget)
  m_tableWidget = new QTableWidget(this);
  m_tableWidget->setColumnCount(4);
  m_tableWidget->setHorizontalHeaderLabels(
      {"", "文件名", "时长", "大小"});
  m_tableWidget->horizontalHeader()->setSectionResizeMode(
      0, QHeaderView::Fixed); // Checkbox
  m_tableWidget->horizontalHeader()->setSectionResizeMode(
      1, QHeaderView::Stretch); // Filename
  m_tableWidget->horizontalHeader()->setSectionResizeMode(
      2, QHeaderView::Fixed); // Duration
  m_tableWidget->horizontalHeader()->setSectionResizeMode(
      3, QHeaderView::Fixed); // Size
  m_tableWidget->setColumnWidth(0, 30); // Checkbox
  m_tableWidget->setColumnWidth(2, 70); // Duration - wider
  m_tableWidget->setColumnWidth(3, 90); // Size - wider
  m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_tableWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_tableWidget->setContextMenuPolicy(Qt::CustomContextMenu);
  m_tableWidget->verticalHeader()->setDefaultSectionSize(45);
  m_tableWidget->verticalHeader()->setVisible(false);
  m_tableWidget->setShowGrid(false);
  m_tableWidget->setAlternatingRowColors(
      false); // Disabled for consistent theme

  // No hardcoded stylesheet - theme manager handles colors

  mainLayout->addWidget(m_tableWidget);

  // Status Label
  m_statusLabel = new QLabel("就绪", this);
  m_statusLabel->setObjectName("statusLabel");
  mainLayout->addWidget(m_statusLabel);
}

void VideoLibraryWidget::setupConnections() {
  connect(m_refreshBtn, &QPushButton::clicked, this,
          &VideoLibraryWidget::onRefreshClicked);
  connect(m_openFolderBtn, &QPushButton::clicked, this,
          &VideoLibraryWidget::onOpenFolderClicked);
  connect(m_selectStorageRootBtn, &QPushButton::clicked, this,
          &VideoLibraryWidget::onSelectStorageRootClicked);
  connect(m_batchDeleteBtn, &QPushButton::clicked, this,
          &VideoLibraryWidget::onBatchDeleteClicked);
  connect(m_analyzeBtn, &QPushButton::clicked, this,
          &VideoLibraryWidget::requestAnalysis);
  connect(m_tableWidget, &QTableWidget::cellDoubleClicked, this,
          &VideoLibraryWidget::onTableDoubleClicked);
  connect(m_tableWidget, &QTableWidget::customContextMenuRequested, this,
          &VideoLibraryWidget::onContextMenuRequested);
}

void VideoLibraryWidget::scanVideoFolder() {
  if (m_captureBusy || m_analysisBusy)
    return;
  QString videoDir = AppPaths::recordingsDir();
  QDir dir(videoDir);
  qDebug() << "扫描视频目录:" << videoDir << "存在:" << dir.exists();

  QStringList filters;
  filters << "*.mp4" << "*.avi";
  QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files);
  qDebug() << "目录中发现" << fileList.size() << "个文件。";

  int total = 0;
  for (const QFileInfo &fileInfo : fileList) {
    if (fileInfo.size() == 0)
      continue;
    VideoInfo info;
    info.filename = fileInfo.fileName();
    info.filepath = fileInfo.absoluteFilePath();
    info.filesize = fileInfo.size();
    info.createdAt = fileInfo.birthTime();
    info.duration = static_cast<qint64>(getVideoDuration(info.filepath));

    if (DatabaseManager::instance().upsertVideo(info)) {
      total++;
    }
  }

  if (total > 0) {
    m_statusLabel->setText(QString("扫描同步 %1 个视频").arg(total));
  }
}

void VideoLibraryWidget::refreshLibrary() {
  if (m_captureBusy || m_analysisBusy)
    return;
  m_tableWidget->setRowCount(0);

  // Phase 5/6：脏数据清理委托给 VideoLibraryService（有单元测试覆盖）
  const int prunedCount =
      VideoLibraryService::pruneOrphans(DatabaseManager::instance());
  if (prunedCount > 0) {
    qDebug() << "清理无效记录:" << prunedCount;
  }
  auto videos = DatabaseManager::instance().getVideosInDirectory(
      AppPaths::recordingsDir());

  m_tableWidget->setRowCount(videos.size());

  int row = 0;
  for (const auto &video : videos) {
    // Column 0: Checkbox
    QTableWidgetItem *checkItem = new QTableWidgetItem();
    checkItem->setCheckState(Qt::Unchecked);
    checkItem->setData(Qt::UserRole, video.id);
    checkItem->setData(Qt::UserRole + 1, video.filepath);
    m_tableWidget->setItem(row, 0, checkItem);

    // Column 1: Filename
    QTableWidgetItem *nameItem = new QTableWidgetItem(video.filename);
    m_tableWidget->setItem(row, 1, nameItem);

    // Column 2: Duration - use cached value from DB
    QTableWidgetItem *durationItem = new QTableWidgetItem(
        video.duration > 0 ? formatDuration(video.duration) : "--:--");
    durationItem->setTextAlignment(Qt::AlignCenter);
    m_tableWidget->setItem(row, 2, durationItem);

    // Column 3: Size
    QTableWidgetItem *sizeItem =
        new QTableWidgetItem(formatFileSize(video.filesize));
    sizeItem->setTextAlignment(Qt::AlignCenter);
    m_tableWidget->setItem(row, 3, sizeItem);

    row++;
  }

  m_statusLabel->setText(QString("共加载 %1 个视频").arg(videos.size()));
}

// Phase 1 重构：格式化和解析全部委托给 VideoUtils（已有单元测试覆盖）
QString VideoLibraryWidget::formatDuration(double seconds) {
  return VideoUtils::formatDuration(seconds);
}

QString VideoLibraryWidget::formatFileSize(qint64 bytes) {
  return VideoUtils::formatFileSize(bytes);
}

double VideoLibraryWidget::getVideoDuration(const QString &filepath) {
  return VideoUtils::parseVideoDurationFromFile(filepath);
}

void VideoLibraryWidget::onRefreshClicked() {
  rescanAndRefresh();
}

void VideoLibraryWidget::rescanAndRefresh() {
  if (m_captureBusy || m_analysisBusy) {
    updateOperationState();
    return;
  }
  scanVideoFolder();
  refreshLibrary();
}

void VideoLibraryWidget::setCaptureBusy(bool busy) {
  m_captureBusy = busy;
  updateOperationState();
  if (!m_captureBusy && !m_analysisBusy)
    rescanAndRefresh();
}

void VideoLibraryWidget::setAnalysisBusy(bool busy) {
  m_analysisBusy = busy;
  updateOperationState();
  if (!m_captureBusy && !m_analysisBusy)
    rescanAndRefresh();
}

void VideoLibraryWidget::updateOperationState() {
  const bool idle = !m_captureBusy && !m_analysisBusy;
  m_analyzeBtn->setEnabled(idle);
  m_batchDeleteBtn->setEnabled(idle);
  m_selectStorageRootBtn->setEnabled(idle);
  m_refreshBtn->setEnabled(idle);
  if (m_captureBusy)
    m_statusLabel->setText("录像尚未保存完成，请稍候再分析或管理文件");
  else if (m_analysisBusy)
    m_statusLabel->setText("本地分析进行中，完成后可继续分析或管理文件");
}

void VideoLibraryWidget::onOpenFolderClicked() {
  QDesktopServices::openUrl(QUrl::fromLocalFile(AppPaths::recordingsDir()));
}

void VideoLibraryWidget::onSelectStorageRootClicked() {
  if (m_captureBusy || m_analysisBusy)
    return;
  const QString dir = QFileDialog::getExistingDirectory(
      this, "选择保存位置", AppPaths::storageRootDir(),
      QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
  if (dir.isEmpty()) {
    return;
  }

  AppPaths::setStorageRootDir(dir);
  m_statusLabel->setText(
      QString("保存位置: %1").arg(AppPaths::recordingsDir()));
  rescanAndRefresh();
}

void VideoLibraryWidget::onTableDoubleClicked(int row, int column) {
  Q_UNUSED(column);
  QTableWidgetItem *item = m_tableWidget->item(row, 0);
  if (!item)
    return;
  QString filepath = item->data(Qt::UserRole + 1).toString();
  QDesktopServices::openUrl(QUrl::fromLocalFile(filepath));
}

void VideoLibraryWidget::onContextMenuRequested(const QPoint &pos) {
  int row = m_tableWidget->rowAt(pos.y());
  if (row < 0)
    return;
  m_tableWidget->selectRow(row);

  QMenu menu(this);
  menu.addAction("播放", this, &VideoLibraryWidget::onPlayAction);
  menu.addAction("打开所在文件夹", this,
                 &VideoLibraryWidget::onOpenFolderClicked);
  const bool idle = !m_captureBusy && !m_analysisBusy;
  menu.addAction("重命名", this, &VideoLibraryWidget::onRenameAction)->setEnabled(idle);
  menu.addAction("删除", this, &VideoLibraryWidget::onDeleteAction)->setEnabled(idle);
  menu.addSeparator();
  menu.addAction("本地分析", this, &VideoLibraryWidget::requestAnalysis)->setEnabled(idle);

  menu.exec(m_tableWidget->mapToGlobal(pos));
}

void VideoLibraryWidget::onPlayAction() {
  int row = m_tableWidget->currentRow();
  if (row >= 0) {
    onTableDoubleClicked(row, 0);
  }
}

void VideoLibraryWidget::onRenameAction() {
  if (m_captureBusy || m_analysisBusy)
    return;
  int row = m_tableWidget->currentRow();
  if (row < 0)
    return;

  QTableWidgetItem *item =
      m_tableWidget->item(row, 0); // Checkbox column has the data
  QTableWidgetItem *nameItem =
      m_tableWidget->item(row, 1); // Filename is column 1
  if (!item || !nameItem)
    return;

  int id = item->data(Qt::UserRole).toInt();
  QString oldName = nameItem->text();
  QString filepath = item->data(Qt::UserRole + 1).toString();

  bool ok;
  QString newName = QInputDialog::getText(
      this, "重命名", "新文件名:", QLineEdit::Normal, oldName, &ok);

  if (ok && !newName.isEmpty() && newName != oldName) {
    QFile file(filepath);
    QString newPath = QFileInfo(filepath).dir().filePath(newName);

    if (file.rename(newPath)) {
      DatabaseManager::instance().updateVideoFilename(id, newName);
      nameItem->setText(newName);
      item->setData(Qt::UserRole + 1, newPath);
    } else {
      QMessageBox::warning(this, "错误", "重命名文件失败");
    }
  }
}

void VideoLibraryWidget::onDeleteAction() {
  if (m_captureBusy || m_analysisBusy)
    return;
  int row = m_tableWidget->currentRow();
  if (row < 0)
    return;

  if (QMessageBox::question(this, "确认", "确定要删除该视频吗？") ==
      QMessageBox::Yes) {
    QTableWidgetItem *item = m_tableWidget->item(row, 0);
    if (!item)
      return;

    int id = item->data(Qt::UserRole).toInt();
    QString filepath = item->data(Qt::UserRole + 1).toString();

    QFile::remove(filepath);
    DatabaseManager::instance().deleteVideo(id);
    m_tableWidget->removeRow(row);
  }
}

void VideoLibraryWidget::onBatchDeleteClicked() {
  if (m_captureBusy || m_analysisBusy)
    return;
  // Get all checked items
  QList<int> rowsToDelete;
  for (int row = 0; row < m_tableWidget->rowCount(); ++row) {
    QTableWidgetItem *item = m_tableWidget->item(row, 0);
    if (item && item->checkState() == Qt::Checked) {
      rowsToDelete.append(row);
    }
  }

  if (rowsToDelete.isEmpty()) {
    QMessageBox::information(this, "提示", "请先勾选要删除的视频");
    return;
  }

  if (QMessageBox::question(
          this, "确认删除",
          QString("确定要删除选中的 %1 个视频吗？").arg(rowsToDelete.size())) !=
      QMessageBox::Yes) {
    return;
  }

  // Delete in reverse order to maintain correct row indices
  std::sort(rowsToDelete.begin(), rowsToDelete.end(), std::greater<int>());
  for (int row : rowsToDelete) {
    QTableWidgetItem *item = m_tableWidget->item(row, 0);
    if (item) {
      int id = item->data(Qt::UserRole).toInt();
      QString filepath = item->data(Qt::UserRole + 1).toString();
      QFile::remove(filepath);
      DatabaseManager::instance().deleteVideo(id);
      m_tableWidget->removeRow(row);
    }
  }

  m_statusLabel->setText(QString("已删除 %1 个视频").arg(rowsToDelete.size()));
}

void VideoLibraryWidget::requestAnalysis() {
  if (m_captureBusy || m_analysisBusy) {
    updateOperationState();
    return;
  }
  QList<int> rows;
  for (int row = 0; row < m_tableWidget->rowCount(); ++row) {
    QTableWidgetItem *item = m_tableWidget->item(row, 0);
    if (item && item->checkState() == Qt::Checked)
      rows.append(row);
  }
  if (rows.isEmpty()) {
    for (const auto &index : m_tableWidget->selectionModel()->selectedRows())
      rows.append(index.row());
  }
  if (rows.isEmpty()) {
    m_statusLabel->setText("请勾选或选中需要分析的视频");
    return;
  }
  std::sort(rows.begin(), rows.end());
  QStringList paths;
  for (int row : rows) {
    const auto *item = m_tableWidget->item(row, 0);
    const QFileInfo file(item->data(Qt::UserRole + 1).toString());
    if (!file.isFile() || file.size() <= 0) {
      m_statusLabel->setText(QString("视频不存在或尚未保存完成：%1").arg(file.fileName()));
      return;
    }
    if (file.suffix().compare("avi", Qt::CaseInsensitive) == 0 &&
        VideoUtils::parseVideoDurationFromFile(file.absoluteFilePath()) <= 0.0) {
      m_statusLabel->setText(QString("录像时长尚不可读，可能仍在保存：%1").arg(file.fileName()));
      return;
    }
    paths.append(file.absoluteFilePath());
  }
  paths.removeDuplicates();
  emit analysisRequested(paths);
}
