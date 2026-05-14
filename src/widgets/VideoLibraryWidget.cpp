#include "VideoLibraryWidget.h"
#include "../data/DatabaseManager.h"
#include "../data/VideoLibraryService.h"
#include "../services/CloudService.h"
#include "../utils/AppPaths.h"
#include "../utils/VideoUtils.h"
#include <QAction>
#include <QCoreApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
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
  // 确保不透明背景，防止底层控件透视
  setAutoFillBackground(true);
  QPalette pal = palette();
  pal.setColor(QPalette::Window, QColor(30, 30, 30)); // 暗色背景
  setPalette(pal);

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
  m_batchUploadBtn = new QPushButton("上传选中", this);
  m_batchUploadBtn->setStyleSheet("QPushButton { color: #4fc3f7; }"); // Blue
  m_batchDeleteBtn = new QPushButton("删除选中", this);
  m_batchDeleteBtn->setStyleSheet("QPushButton { color: #ff6b6b; }"); // Red

  toolbarLayout->addWidget(m_refreshBtn);
  toolbarLayout->addWidget(m_openFolderBtn);
  toolbarLayout->addStretch();
  toolbarLayout->addWidget(m_batchUploadBtn);
  toolbarLayout->addWidget(m_batchDeleteBtn);

  mainLayout->addLayout(toolbarLayout);

  // Table Widget (replacing QListWidget)
  m_tableWidget = new QTableWidget(this);
  m_tableWidget->setColumnCount(5);
  m_tableWidget->setHorizontalHeaderLabels(
      {"", "文件名", "时长", "大小", "上传状态"});
  m_tableWidget->horizontalHeader()->setSectionResizeMode(
      0, QHeaderView::Fixed); // Checkbox
  m_tableWidget->horizontalHeader()->setSectionResizeMode(
      1, QHeaderView::Stretch); // Filename
  m_tableWidget->horizontalHeader()->setSectionResizeMode(
      2, QHeaderView::Fixed); // Duration
  m_tableWidget->horizontalHeader()->setSectionResizeMode(
      3, QHeaderView::Fixed); // Size
  m_tableWidget->horizontalHeader()->setSectionResizeMode(
      4, QHeaderView::Fixed);           // Status
  m_tableWidget->setColumnWidth(0, 30); // Checkbox
  m_tableWidget->setColumnWidth(2, 70); // Duration - wider
  m_tableWidget->setColumnWidth(3, 90); // Size - wider
  m_tableWidget->setColumnWidth(4, 80); // Status - wider
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
  mainLayout->addWidget(m_statusLabel);
}

void VideoLibraryWidget::setupConnections() {
  connect(m_refreshBtn, &QPushButton::clicked, this,
          &VideoLibraryWidget::onRefreshClicked);
  connect(m_openFolderBtn, &QPushButton::clicked, this,
          &VideoLibraryWidget::onOpenFolderClicked);
  connect(m_batchDeleteBtn, &QPushButton::clicked, this,
          &VideoLibraryWidget::onBatchDeleteClicked);
  connect(m_batchUploadBtn, &QPushButton::clicked, this,
          &VideoLibraryWidget::onBatchUploadClicked);
  connect(m_tableWidget, &QTableWidget::cellDoubleClicked, this,
          &VideoLibraryWidget::onTableDoubleClicked);
  connect(m_tableWidget, &QTableWidget::customContextMenuRequested, this,
          &VideoLibraryWidget::onContextMenuRequested);
}

void VideoLibraryWidget::scanVideoFolder() {
  QString videoDir = AppPaths::recordingsDir();
  QDir dir(videoDir);
  qDebug() << "扫描视频目录:" << videoDir << "存在:" << dir.exists();

  QStringList filters;
  filters << "*.mp4" << "*.avi";
  QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files);
  qDebug() << "目录中发现" << fileList.size() << "个文件。";

  int total = 0;
  for (const QFileInfo &fileInfo : fileList) {
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
  m_tableWidget->setRowCount(0);

  // Phase 5/6：脏数据清理委托给 VideoLibraryService（有单元测试覆盖）
  const int prunedCount =
      VideoLibraryService::pruneOrphans(DatabaseManager::instance());
  if (prunedCount > 0) {
    qDebug() << "清理无效记录:" << prunedCount;
  }
  auto videos = DatabaseManager::instance().getAllVideos();

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

    // Column 4: Upload Status
    QTableWidgetItem *statusItem = new QTableWidgetItem("未上传");
    statusItem->setTextAlignment(Qt::AlignCenter);
    m_tableWidget->setItem(row, 4, statusItem);

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
  scanVideoFolder();
  refreshLibrary();
}

void VideoLibraryWidget::onOpenFolderClicked() {
  QDesktopServices::openUrl(QUrl::fromLocalFile(AppPaths::recordingsDir()));
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
  menu.addAction("重命名", this, &VideoLibraryWidget::onRenameAction);
  menu.addAction("删除", this, &VideoLibraryWidget::onDeleteAction);
  menu.addSeparator();
  menu.addAction("上传到云端 (Mock)", this,
                 &VideoLibraryWidget::onUploadAction);

  menu.exec(m_tableWidget->mapToGlobal(pos));
}

void VideoLibraryWidget::onPlayAction() {
  int row = m_tableWidget->currentRow();
  if (row >= 0) {
    onTableDoubleClicked(row, 0);
  }
}

void VideoLibraryWidget::onRenameAction() {
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

void VideoLibraryWidget::onUploadAction() {
  // Phase 4 修复 #11：原代码弹"上传成功"会误导用户——CloudService 仍是 Mock。
  // 明确告知功能尚未实现，避免用户以为视频已上云。
  QMessageBox::information(this, "云服务",
                           "云上传功能尚未实现。\n"
                           "（CloudService 当前为 Mock，未对接真实后端）");
}

void VideoLibraryWidget::onBatchDeleteClicked() {
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

void VideoLibraryWidget::onBatchUploadClicked() {
  // Get all checked items
  QList<int> rowsToUpload;
  for (int row = 0; row < m_tableWidget->rowCount(); ++row) {
    QTableWidgetItem *item = m_tableWidget->item(row, 0);
    if (item && item->checkState() == Qt::Checked) {
      rowsToUpload.append(row);
    }
  }

  if (rowsToUpload.isEmpty()) {
    QMessageBox::information(this, "提示", "请先勾选要上传的视频");
    return;
  }

  // TODO: Implement actual upload logic
  QMessageBox::information(
      this, "上传功能",
      QString("已选中 %1 个视频待上传\n\n（上传功能待实现）")
          .arg(rowsToUpload.size()));
}
