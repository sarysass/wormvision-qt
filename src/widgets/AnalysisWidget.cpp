#include "AnalysisWidget.h"
#include "services/LocalAnalysisService.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {
QString runPath(const QString &id) {
  return "/api/runs/" + QString::fromLatin1(QUrl::toPercentEncoding(id));
}

bool activeState(const QString &state) {
  return state == "queued" || state == "pending" || state == "running";
}

bool sourceEngine(const QJsonObject &license) {
  const QJsonValue bundled = license.value("bundled_release");
  return bundled.isBool() && !bundled.toBool();
}

QString stateText(const QString &state) {
  if (state == "queued" || state == "pending") return "排队中";
  if (state == "running") return "分析中";
  if (state == "completed") return "已完成";
  if (state == "failed") return "失败";
  if (state == "cancelled") return "已取消";
  return state.isEmpty() ? "状态未知" : state;
}

QString runId(const QJsonObject &run) {
  QString id = run.value("id").toString();
  if (id.isEmpty()) id = run.value("job_id").toString();
  if (id.isEmpty()) id = run.value("run_id").toString();
  return id;
}

QString runError(const QJsonValue &error) {
  if (error.isObject()) return error.toObject().value("message").toString();
  return error.toString();
}

QLabel *wrappedLabel(const QString &text, QWidget *parent) {
  auto *label = new QLabel(text, parent);
  label->setWordWrap(true);
  label->setTextFormat(Qt::PlainText);
  label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  return label;
}
} // namespace

AnalysisWidget::AnalysisWidget(QWidget *parent) : QWidget(parent) {
  m_service = new LocalAnalysisService(this);
  setupUI();
  m_pollTimer = new QTimer(this);
  m_pollTimer->setInterval(2000);
  connect(m_pollTimer, &QTimer::timeout, this, &AnalysisWidget::pollRuns);
  connect(m_service, &LocalAnalysisService::statusChanged, this, [this](const QString &text) {
    m_serviceStatus->setText(text);
    updateControls();
  });
  connect(m_service, &LocalAnalysisService::ready, this, [this]() {
    m_serviceStatus->setText("本地分析引擎已就绪");
    m_pollTimer->start();
    refreshLicense();
    refreshHistory();
    updateControls();
  });
  connect(m_service, &LocalAnalysisService::errorOccurred, this, [this](const QString &message) {
    m_serviceStatus->setText(message + "\n可通过“引擎设置”检查程序路径。");
    updateControls();
  });
  connect(m_service, &LocalAnalysisService::stopped, this, [this]() {
    m_pollTimer->stop();
    if (hasActiveAnalysis()) m_activeStatus->setText("引擎已停止，请刷新历史查看任务状态。");
    m_activeJobId.clear();
    m_submitting = false;
    m_cancelPending = false;
    m_activePolling = false;
    m_selectedPolling = false;
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_license = {};
    m_licenseStatus->setText("许可状态：等待引擎启动");
    ++m_selectionGeneration;
    ++m_historyGeneration;
    setBusyState();
  });
  updateControls();
}

void AnalysisWidget::setupUI() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(20, 16, 20, 16);
  auto *heading = new QHBoxLayout;
  auto *title = new QLabel("本地分析", this);
  QFont titleFont = title->font();
  titleFont.setPointSize(18);
  titleFont.setBold(true);
  title->setFont(titleFont);
  heading->addWidget(title);
  heading->addStretch();
  m_activate = new QPushButton("许可与激活", this);
  m_configure = new QPushButton("引擎设置", this);
  heading->addWidget(m_activate);
  heading->addWidget(m_configure);
  layout->addLayout(heading);
  m_serviceStatus = wrappedLabel("首次进入时启动本地分析引擎。", this);
  m_serviceStatus->setObjectName("statusLabel");
  m_licenseStatus = wrappedLabel("许可状态：等待引擎启动", this);
  layout->addWidget(m_serviceStatus);
  layout->addWidget(m_licenseStatus);

  auto *splitter = new QSplitter(Qt::Horizontal, this);
  auto *leftScroll = new QScrollArea(splitter);
  leftScroll->setWidgetResizable(true);
  leftScroll->setFrameShape(QFrame::NoFrame);
  auto *left = new QWidget(leftScroll);
  auto *leftLayout = new QVBoxLayout(left);
  leftLayout->setContentsMargins(0, 0, 8, 0);
  auto *input = new QGroupBox("待分析视频", left);
  auto *inputLayout = new QVBoxLayout(input);
  inputLayout->addWidget(wrappedLabel("在视频库勾选视频后，点击“本地分析”。", input));
  m_videos = new QListWidget(input);
  m_videos->setMinimumHeight(90);
  m_videos->setMaximumHeight(180);
  inputLayout->addWidget(m_videos);
  m_captureStatus = wrappedLabel("", input);
  inputLayout->addWidget(m_captureStatus);
  auto *form = new QFormLayout;
  m_protocol = new QComboBox(input);
  m_protocol->addItem("固体爬行", "worm.crawling");
  m_protocol->addItem("液体摆动", "worm.thrashing");
  m_device = new QComboBox(input);
  m_device->addItem("自动", "auto");
  m_device->addItem("GPU（cuda:0）", "cuda:0");
  m_device->addItem("CPU", "cpu");
  form->addRow("实验类型", m_protocol);
  form->addRow("计算设备", m_device);
  m_calibrated = new QCheckBox("设置空间标定", input);
  m_pixelToMm = new QDoubleSpinBox(input);
  m_pixelToMm->setDecimals(8);
  m_pixelToMm->setRange(0, 1000000);
  m_pixelToMm->setValue(0);
  m_pixelToMm->setSpecialValueText("请输入实测标定");
  m_pixelToMm->setSuffix(" mm/px");
  m_pixelToMm->setEnabled(false);
  form->addRow(m_calibrated);
  form->addRow("标定值", m_pixelToMm);
  inputLayout->addLayout(form);
  inputLayout->addWidget(wrappedLabel("未设置标定时按像素单位分析，单位以结果列为准。", input));
  inputLayout->addWidget(wrappedLabel("分析算法：YOLO + SAM2\nyolo-sam2-optimized-core", input));
  auto *buttons = new QHBoxLayout;
  m_start = new QPushButton("开始分析", input);
  m_start->setObjectName("primaryButton");
  m_cancel = new QPushButton("取消任务", input);
  buttons->addWidget(m_start);
  buttons->addWidget(m_cancel);
  inputLayout->addLayout(buttons);
  m_activeStatus = wrappedLabel("尚未提交任务", input);
  inputLayout->addWidget(m_activeStatus);
  m_progress = new QProgressBar(input);
  m_progress->setRange(0, 100);
  m_progress->setValue(0);
  inputLayout->addWidget(m_progress);
  leftLayout->addWidget(input);
  auto *historyHeading = new QHBoxLayout;
  historyHeading->addWidget(new QLabel("分析历史", left));
  historyHeading->addStretch();
  m_refresh = new QPushButton("刷新历史", left);
  historyHeading->addWidget(m_refresh);
  leftLayout->addLayout(historyHeading);
  m_history = new QListWidget(left);
  m_history->setMinimumHeight(90);
  leftLayout->addWidget(m_history, 1);
  leftScroll->setWidget(left);

  auto *right = new QWidget(splitter);
  auto *rightLayout = new QVBoxLayout(right);
  rightLayout->setContentsMargins(8, 0, 0, 0);
  m_resultStatus = wrappedLabel("完成分析后，将显示总表、逐视频线虫指标和轨迹图。", right);
  rightLayout->addWidget(m_resultStatus);
  auto *exportButtons = new QHBoxLayout;
  m_openExcel = new QPushButton("打开 Excel", right);
  m_openPdf = new QPushButton("打开报告", right);
  m_openFolder = new QPushButton("结果目录", right);
  exportButtons->addWidget(m_openExcel);
  exportButtons->addWidget(m_openPdf);
  exportButtons->addWidget(m_openFolder);
  exportButtons->addStretch();
  rightLayout->addLayout(exportButtons);
  m_tabs = new QTabWidget(right);
  m_summaryTable = new QTableWidget(m_tabs);
  m_tabs->addTab(m_summaryTable, "总表");
  auto *tracks = new QWidget(m_tabs);
  auto *tracksLayout = new QVBoxLayout(tracks);
  m_videoSelector = new QComboBox(tracks);
  tracksLayout->addWidget(m_videoSelector);
  m_trackTable = new QTableWidget(tracks);
  tracksLayout->addWidget(m_trackTable);
  m_tabs->addTab(tracks, "逐视频 / 逐虫");
  auto *previewPage = new QWidget(m_tabs);
  auto *previewLayout = new QVBoxLayout(previewPage);
  m_previewSelector = new QComboBox(previewPage);
  previewLayout->addWidget(m_previewSelector);
  auto *scroll = new QScrollArea(previewPage);
  scroll->setWidgetResizable(true);
  m_preview = new QLabel("暂无轨迹图", scroll);
  m_preview->setAlignment(Qt::AlignCenter);
  m_preview->setMinimumSize(1, 1);
  scroll->setWidget(m_preview);
  previewLayout->addWidget(scroll, 1);
  auto *reviewRow = new QHBoxLayout;
  m_reviewSelector = new QComboBox(previewPage);
  m_openReview = new QPushButton("打开标注视频", previewPage);
  reviewRow->addWidget(m_reviewSelector, 1);
  reviewRow->addWidget(m_openReview);
  previewLayout->addLayout(reviewRow);
  m_tabs->addTab(previewPage, "轨迹与标注视频");
  rightLayout->addWidget(m_tabs, 1);
  splitter->addWidget(leftScroll);
  splitter->addWidget(right);
  splitter->setStretchFactor(0, 0);
  splitter->setStretchFactor(1, 1);
  splitter->setSizes({350, 850});
  left->setMinimumWidth(285);
  layout->addWidget(splitter, 1);

  for (QTableWidget *table : {m_summaryTable, m_trackTable}) {
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
  }
  connect(m_calibrated, &QCheckBox::toggled, this, &AnalysisWidget::updateControls);
  connect(m_configure, &QPushButton::clicked, this, &AnalysisWidget::configureEngine);
  connect(m_activate, &QPushButton::clicked, this, &AnalysisWidget::activateLicense);
  connect(m_start, &QPushButton::clicked, this, &AnalysisWidget::startAnalysis);
  connect(m_cancel, &QPushButton::clicked, this, &AnalysisWidget::cancelAnalysis);
  connect(m_refresh, &QPushButton::clicked, this, [this]() {
    if (!m_service->isReady()) {
      m_service->start();
      return;
    }
    refreshLicense();
    refreshHistory();
    if (!m_currentRunId.isEmpty()) selectRun(m_currentRunId);
  });
  connect(m_history, &QListWidget::currentItemChanged, this,
          [this](QListWidgetItem *item) {
    if (item) selectRun(item->data(Qt::UserRole).toString());
  });
  connect(m_videoSelector, &QComboBox::currentIndexChanged, this, &AnalysisWidget::showVideoTable);
  connect(m_previewSelector, &QComboBox::currentIndexChanged, this, &AnalysisWidget::showPreview);
  connect(m_reviewSelector, &QComboBox::currentIndexChanged, this, &AnalysisWidget::updateControls);
  connect(m_openReview, &QPushButton::clicked, this, [this]() {
    openOutput(m_reviewSelector->currentData().toString());
  });
  connect(m_openExcel, &QPushButton::clicked, this, [this]() { openOutput(m_excelPath); });
  connect(m_openPdf, &QPushButton::clicked, this, [this]() { openOutput(m_pdfPath); });
  connect(m_openFolder, &QPushButton::clicked, this, [this]() {
    if (QFileInfo(m_outputDir).isDir())
      QDesktopServices::openUrl(QUrl::fromLocalFile(m_outputDir));
  });
}

void AnalysisWidget::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  if (!m_service->isReady() && !m_service->isStarting()) m_service->start();
}

void AnalysisWidget::setSelectedVideos(const QStringList &paths) {
  m_videos->clear();
  QStringList seen;
  for (const QString &path : paths) {
    const QString absolute = QFileInfo(path).absoluteFilePath();
    if (seen.contains(absolute, Qt::CaseInsensitive)) continue;
    seen.append(absolute);
    auto *item = new QListWidgetItem(QFileInfo(absolute).fileName(), m_videos);
    item->setData(Qt::UserRole, absolute);
    item->setToolTip(QDir::toNativeSeparators(absolute));
  }
  updateControls();
}

void AnalysisWidget::setCaptureBusy(bool busy) {
  m_captureBusy = busy;
  updateControls();
}

bool AnalysisWidget::hasActiveAnalysis() const {
  return m_submitting || !m_activeJobId.isEmpty();
}

void AnalysisWidget::setBusyState() {
  const bool busy = hasActiveAnalysis();
  if (m_reportedBusy != busy) {
    m_reportedBusy = busy;
    emit busyChanged(busy);
  }
  updateControls();
}

void AnalysisWidget::updateControls() {
  const bool busy = hasActiveAnalysis();
  const bool ready = m_service->isReady();
  const bool permitted = m_license.value("valid").toBool() || sourceEngine(m_license);
  m_start->setEnabled(!busy && !m_captureBusy && m_videos->count() > 0 &&
                      ready && permitted);
  m_cancel->setEnabled(ready && !m_activeJobId.isEmpty() && !m_cancelPending);
  m_configure->setEnabled(!busy && !m_service->isStarting());
  m_activate->setEnabled(ready && !busy);
  m_refresh->setEnabled(!m_service->isStarting());
  m_protocol->setEnabled(!busy);
  m_device->setEnabled(!busy);
  m_calibrated->setEnabled(!busy);
  m_pixelToMm->setEnabled(!busy && m_calibrated->isChecked());
  m_captureStatus->setText(m_captureBusy ? "录像正在写入，请结束录像后再提交分析。"
                                        : QString("已选择 %1 个视频").arg(m_videos->count()));
  m_openExcel->setEnabled(!m_excelPath.isEmpty());
  m_openPdf->setEnabled(!m_pdfPath.isEmpty());
  m_openFolder->setEnabled(!m_outputDir.isEmpty() && QFileInfo(m_outputDir).isDir());
  m_openReview->setEnabled(m_reviewSelector->count() > 0);
}

void AnalysisWidget::configureEngine() {
  if (hasActiveAnalysis()) return;
  QDialog dialog(this);
  dialog.setWindowTitle("本地分析引擎设置");
  dialog.resize(640, 260);
  auto *layout = new QVBoxLayout(&dialog);
  layout->addWidget(wrappedLabel("选择已安装的 MicroHunter 引擎程序。使用 Python 开发环境时，"
                                 "选择 Python 程序和可选的入口脚本。设置会保存在本机。", &dialog));
  auto *form = new QFormLayout;
  auto *program = new QLineEdit(m_service->program(), &dialog);
  auto *script = new QLineEdit(m_service->script(), &dialog);
  script->setPlaceholderText("发行版引擎无需填写");
  const auto addPath = [&](const QString &label, QLineEdit *edit, const QString &filter) {
    auto *row = new QHBoxLayout;
    row->addWidget(edit, 1);
    auto *browse = new QPushButton("浏览…", &dialog);
    row->addWidget(browse);
    form->addRow(label, row);
    connect(browse, &QPushButton::clicked, &dialog, [&, edit, filter]() {
      const QString path = QFileDialog::getOpenFileName(&dialog, "选择文件", edit->text(), filter);
      if (!path.isEmpty()) edit->setText(path);
    });
  };
  addPath("引擎程序", program, "程序 (*.exe);;所有文件 (*)");
  addPath("Python 入口（可选）", script, "Python 脚本 (*.py);;所有文件 (*)");
  layout->addLayout(form);
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
    if (program->text().trimmed().isEmpty()) {
      QMessageBox::warning(&dialog, "引擎设置", "请选择引擎程序。");
      return;
    }
    dialog.accept();
  });
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  if (dialog.exec() == QDialog::Accepted) {
    m_service->stop();
    m_service->setEngine(program->text().trimmed(), script->text().trimmed());
    m_service->start();
  }
}

void AnalysisWidget::refreshLicense() {
  if (!m_service->isReady()) return;
  m_service->get("/api/license/status", [this](const QJsonDocument &document, const QString &error) {
    if (!error.isEmpty()) {
      m_license = {};
      m_licenseStatus->setText("许可状态获取失败：" + error);
    } else {
      m_license = document.object();
      const bool valid = m_license.value("valid").toBool();
      const bool source = sourceEngine(m_license);
      QString text = source ? "源码引擎模式：无需激活（由引擎确认）"
                            : (valid ? "许可有效" : "许可未激活或已失效，请点击“许可与激活”。");
      if (!source && valid && !m_license.value("not_after").toString().isEmpty())
        text += " · 到期：" + m_license.value("not_after").toString();
      const QString reason = m_license.value("reason").toString();
      if (!source && !valid && !reason.isEmpty()) text += "\n" + reason;
      m_licenseStatus->setText(text);
    }
    updateControls();
  });
}

void AnalysisWidget::activateLicense() {
  if (!m_service->isReady()) return;
  QDialog dialog(this);
  dialog.setWindowTitle("许可与激活");
  dialog.resize(580, 250);
  auto *layout = new QVBoxLayout(&dialog);
  layout->addWidget(wrappedLabel(m_licenseStatus->text(), &dialog));
  layout->addWidget(wrappedLabel("机器标识：" + m_license.value("machine_fingerprint").toString(), &dialog));
  auto *form = new QFormLayout;
  auto *key = new QLineEdit(&dialog);
  key->setEchoMode(QLineEdit::Password);
  auto *server = new QLineEdit(&dialog);
  server->setPlaceholderText("留空使用引擎默认服务器");
  form->addRow("激活码", key);
  form->addRow("服务器（可选）", server);
  layout->addLayout(form);
  layout->addWidget(wrappedLabel("更换电脑或重新安装需要释放席位时，请联系客服并提供机器标识。", &dialog));
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  buttons->button(QDialogButtonBox::Ok)->setText("激活");
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
    if (!key->text().trimmed().isEmpty()) dialog.accept();
  });
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  if (dialog.exec() != QDialog::Accepted) return;
  QJsonObject body{{"license_key", key->text().trimmed()}};
  if (!server->text().trimmed().isEmpty()) body.insert("server", server->text().trimmed());
  m_activate->setEnabled(false);
  m_licenseStatus->setText("正在激活…");
  m_service->post("/api/license/activate", body,
                  [this](const QJsonDocument &, const QString &error) {
    if (!error.isEmpty()) {
      m_licenseStatus->setText("激活失败：" + error);
      updateControls();
      return;
    }
    refreshLicense();
    refreshHistory();
  });
}

void AnalysisWidget::refreshHistory() {
  if (!m_service->isReady()) return;
  const quint64 generation = ++m_historyGeneration;
  m_service->get("/api/runs", [this, generation](const QJsonDocument &document, const QString &error) {
    if (generation != m_historyGeneration) return;
    if (!error.isEmpty()) {
      m_history->setToolTip(error);
      m_serviceStatus->setText("历史读取失败：" + error);
      return;
    }
    if (!document.isArray()) return;
    const QSignalBlocker blocker(m_history);
    m_history->clear();
    m_history->setToolTip("");
    QListWidgetItem *selected = nullptr;
    for (const QJsonValue &value : document.array()) {
      const QJsonObject run = value.toObject();
      const QString id = runId(run);
      if (id.isEmpty()) continue;
      QString text = QString("%1 · %2 个视频\n%3")
                         .arg(stateText(run.value("state").toString()))
                         .arg(run.value("video_count").toInt())
                         .arg(run.value("created_at").toString());
      auto *item = new QListWidgetItem(text, m_history);
      item->setData(Qt::UserRole, id);
      item->setToolTip(id);
      if (id == m_currentRunId) selected = item;
    }
    if (selected) m_history->setCurrentItem(selected);
    if (m_currentRunId.isEmpty() && m_history->count() > 0) {
      m_history->setCurrentRow(0);
      selectRun(m_history->item(0)->data(Qt::UserRole).toString());
    }
  });
}

void AnalysisWidget::startAnalysis() {
  if (hasActiveAnalysis() || m_captureBusy) return;
  if (!m_service->isReady()) {
    m_service->start();
    return;
  }
  QJsonArray videos;
  for (int row = 0; row < m_videos->count(); ++row) {
    const QString path = m_videos->item(row)->data(Qt::UserRole).toString();
    const QFileInfo file(path);
    if (!file.isAbsolute() || !file.isFile() || file.size() <= 0) {
      QMessageBox::warning(this, "无法开始分析", "视频不存在或仍为空文件：\n" + path);
      return;
    }
    videos.append(path);
  }
  if (videos.isEmpty()) return;
  if (m_calibrated->isChecked() && m_pixelToMm->value() <= 0) {
    QMessageBox::warning(this, "空间标定", "请输入实测的 mm/px 标定值，或取消空间标定。");
    return;
  }
  QJsonObject body{{"videos", videos}, {"protocol_id", m_protocol->currentData().toString()},
                   {"route_id", "yolo-sam2-optimized-core"},
                   {"device", m_device->currentData().toString()}, {"review_video", true}};
  body.insert("pixel_to_mm", m_calibrated->isChecked() ? QJsonValue(m_pixelToMm->value())
                                                       : QJsonValue(QJsonValue::Null));
  m_submitting = true;
  const quint64 selectionAtSubmit = m_selectionGeneration;
  m_activeStatus->setText("正在提交分析…");
  m_progress->setRange(0, 0);
  setBusyState();
  m_service->post("/api/runs", body, [this, selectionAtSubmit](const QJsonDocument &document,
                                                              const QString &error) {
    m_submitting = false;
    if (!error.isEmpty()) {
      // 标准 error 对象是引擎明确拒绝；其他错误无法确认引擎是否已入队。
      // 停止本页面拥有的进程，保证不会留下界面无法追踪的后台任务。
      if (!document.object().value("error").isObject() && m_service->isReady()) {
        m_service->stop();
        m_activeStatus->setText("无法确认任务是否提交，已停止本地引擎。请刷新历史后重试。\n" +
                                error);
        return;
      }
      m_activeStatus->setText("提交失败：" + error);
      m_progress->setRange(0, 100);
      m_progress->setValue(0);
      setBusyState();
      refreshLicense();
      return;
    }
    const QJsonObject run = document.object();
    const QString id = runId(run);
    if (id.isEmpty()) {
      if (m_service->isReady()) m_service->stop();
      m_activeStatus->setText("引擎未返回任务编号，已停止本地引擎。请刷新历史后重试。");
      return;
    }
    if (activeState(run.value("state").toString())) m_activeJobId = id;
    if (selectionAtSubmit == m_selectionGeneration) {
      selectRun(id);
      showRun(run);
    }
    m_activeStatus->setText(stateText(run.value("state").toString()));
    setBusyState();
    refreshHistory();
    pollRuns();
  });
}

void AnalysisWidget::cancelAnalysis() {
  if (m_activeJobId.isEmpty() || m_cancelPending) return;
  const QString id = m_activeJobId;
  m_cancelPending = true;
  updateControls();
  m_service->post(runPath(id) + "/cancel", {},
                  [this, id](const QJsonDocument &document, const QString &error) {
    if (m_activeJobId != id) return;
    m_cancelPending = false;
    if (!error.isEmpty()) m_activeStatus->setText("取消失败：" + error);
    else if (document.object().value("cancelled").toBool())
      m_activeStatus->setText("已请求取消，等待引擎停止当前任务…");
    else m_activeStatus->setText("任务状态已变化，正在刷新…");
    updateControls();
    pollRuns();
  });
}

void AnalysisWidget::pollRuns() {
  if (!m_service->isReady()) return;
  if (!m_activeJobId.isEmpty() && !m_activePolling) {
    const QString id = m_activeJobId;
    m_activePolling = true;
    m_service->get(runPath(id), [this, id](const QJsonDocument &document, const QString &error) {
      if (m_activeJobId != id) return;
      m_activePolling = false;
      if (!error.isEmpty()) {
        m_activeStatus->setText("任务状态暂不可用，将自动重试：" + error);
        return;
      }
      const QJsonObject run = document.object();
      const QString state = run.value("state").toString();
      const QJsonObject progress = run.value("progress").toObject();
      const QJsonObject active = progress.value("active").toObject();
      QString text = stateText(state);
      const QString label = active.value("label").toString();
      if (!label.isEmpty()) text += " · " + label;
      if (progress.value("cancel_requested").toBool()) text += " · 正在取消";
      const QString errorText = runError(run.value("error"));
      if (!errorText.isEmpty()) text += "\n" + errorText;
      m_activeStatus->setText(text);
      if (active.value("percent").isDouble()) {
        m_progress->setRange(0, 100);
        m_progress->setValue(qBound(0, qRound(active.value("percent").toDouble()), 100));
      } else if (activeState(state)) m_progress->setRange(0, 0);
      if (m_currentRunId == id) showRun(run);
      if (state == "completed" || state == "failed" || state == "cancelled") {
        m_activeJobId.clear();
        m_cancelPending = false;
        m_progress->setRange(0, 100);
        m_progress->setValue(state == "completed" ? 100 : 0);
        setBusyState();
        refreshHistory();
      }
    });
  }
  if (!m_currentRunId.isEmpty() && m_currentRunId != m_activeJobId &&
      activeState(m_currentState) && !m_selectedPolling) {
    const QString id = m_currentRunId;
    const quint64 generation = m_selectionGeneration;
    m_selectedPolling = true;
    m_service->get(runPath(id), [this, id, generation](const QJsonDocument &document, const QString &error) {
      if (id != m_currentRunId || generation != m_selectionGeneration) return;
      m_selectedPolling = false;
      if (error.isEmpty()) showRun(document.object());
      else m_resultStatus->setText("状态刷新失败：" + error);
    });
  }
}

void AnalysisWidget::selectRun(const QString &id) {
  m_currentRunId = id;
  m_currentState.clear();
  m_selectedPolling = false;
  const quint64 generation = ++m_selectionGeneration;
  clearResults();
  m_resultStatus->setText("正在读取分析任务…");
  if (id == m_activeJobId) {
    m_currentState = "running";
    pollRuns();
    return;
  }
  m_service->get(runPath(id), [this, id, generation](const QJsonDocument &document, const QString &error) {
    if (id != m_currentRunId || generation != m_selectionGeneration) return;
    if (!error.isEmpty()) {
      m_resultStatus->setText("任务读取失败：" + error);
      return;
    }
    showRun(document.object());
  });
}

void AnalysisWidget::showRun(const QJsonObject &run) {
  m_currentState = run.value("state").toString();
  QString text = QString("%1 · %2\n%3")
                     .arg(stateText(m_currentState), run.value("created_at").toString(), m_currentRunId);
  const QString error = runError(run.value("error"));
  if (!error.isEmpty()) text += "\n" + error;
  if (activeState(m_currentState)) text += "\n分析完成后加载结果。";
  m_resultStatus->setText(text);
  if (m_currentState == "completed" && m_loadedRunId != m_currentRunId) {
    m_loadedRunId = m_currentRunId;
    loadResults(m_currentRunId, m_selectionGeneration);
  }
}

void AnalysisWidget::clearResults() {
  m_loadedRunId.clear();
  m_outputDir.clear();
  m_excelPath.clear();
  m_pdfPath.clear();
  m_report = {};
  m_summaryTable->clear();
  m_summaryTable->setRowCount(0);
  m_summaryTable->setColumnCount(0);
  m_trackTable->clear();
  m_trackTable->setRowCount(0);
  m_trackTable->setColumnCount(0);
  m_videoSelector->clear();
  m_previewSelector->clear();
  m_reviewSelector->clear();
  m_preview->clear();
  m_preview->setText("暂无轨迹图");
  updateControls();
}

void AnalysisWidget::loadResults(const QString &id, quint64 generation) {
  m_service->get(runPath(id) + "/files", [this, id, generation](const QJsonDocument &document,
                                                               const QString &error) {
    if (id != m_currentRunId || generation != m_selectionGeneration) return;
    if (!error.isEmpty()) {
      m_resultStatus->setText("产物读取失败：" + error + "\n可点击刷新历史重试。");
      return;
    }
    const QJsonObject listing = document.object();
    m_outputDir = listing.value("output_dir").toString();
    bool metricsFound = false;
    for (const QJsonValue &value : listing.value("files").toArray()) {
      const QJsonObject file = value.toObject();
      const QString relative = file.value("path").toString();
      if (relative == "results/metrics.json") metricsFound = true;
      if (AnalysisResults::outputFile(m_outputDir, relative).isEmpty()) continue;
      if (relative == "results/metrics.xlsx") m_excelPath = relative;
      if (relative == "report/analysis-summary.pdf") m_pdfPath = relative;
      if (relative.endsWith(".png", Qt::CaseInsensitive) &&
          (relative.startsWith("qc/trajectory/") || relative.startsWith("qc/overview/")))
        m_previewSelector->addItem(relative, relative);
      if (relative.startsWith("qc/review/") && relative.endsWith(".mp4", Qt::CaseInsensitive))
        m_reviewSelector->addItem(file.value("name").toString(), relative);
    }
    updateControls();
    if (!metricsFound) {
      m_resultStatus->setText("该任务已完成，但产物清单中未找到 metrics.json。可打开已有文件。");
      return;
    }
    m_service->get(runPath(id) + "/files/results/metrics.json",
                   [this, id, generation](const QJsonDocument &metrics, const QString &metricsError) {
      if (id != m_currentRunId || generation != m_selectionGeneration) return;
      if (!metricsError.isEmpty()) {
        m_resultStatus->setText("指标读取失败：" + metricsError);
        return;
      }
      m_report = AnalysisResults::parse(metrics);
      if (!m_report.error.isEmpty()) {
        m_resultStatus->setText(m_report.error);
        return;
      }
      fillTable(m_summaryTable, m_report.summary);
      for (const auto &video : m_report.videos) m_videoSelector->addItem(video.title, video.videoId);
      showVideoTable();
      m_resultStatus->setText(QString("已完成 · %1 个视频\n数值、中文列名与单位来自引擎结果。缺失值显示为 —。")
                                 .arg(m_report.summary.rows.size()));
    });
  });
}

void AnalysisWidget::fillTable(QTableWidget *table, const AnalysisResults::Table &data) {
  table->clear();
  table->setColumnCount(data.headers.size());
  table->setRowCount(data.rows.size());
  table->setHorizontalHeaderLabels(data.headers);
  for (int column = 0; column < data.tooltips.size(); ++column)
    table->horizontalHeaderItem(column)->setToolTip(data.tooltips.at(column));
  for (int row = 0; row < data.rows.size(); ++row) {
    for (int column = 0; column < data.rows.at(row).size(); ++column) {
      auto *item = new QTableWidgetItem(data.rows.at(row).at(column));
      item->setToolTip(item->text());
      table->setItem(row, column, item);
    }
  }
  table->resizeColumnsToContents();
  for (int column = 0; column < table->columnCount(); ++column)
    table->setColumnWidth(column, qMin(260, qMax(90, table->columnWidth(column))));
}

void AnalysisWidget::showVideoTable() {
  const int index = m_videoSelector->currentIndex();
  if (index >= 0 && index < m_report.videos.size()) fillTable(m_trackTable, m_report.videos.at(index));
}

void AnalysisWidget::showPreview() {
  m_preview->clear();
  const QString path = AnalysisResults::outputFile(m_outputDir, m_previewSelector->currentData().toString());
  const QPixmap pixmap(path);
  if (pixmap.isNull()) {
    m_preview->setText("暂无可预览的轨迹图");
    return;
  }
  m_preview->setPixmap(pixmap.scaled(QSize(1200, 900), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void AnalysisWidget::openOutput(const QString &relativePath) {
  const QString path = AnalysisResults::outputFile(m_outputDir, relativePath);
  if (path.isEmpty() || !QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
    QMessageBox::warning(this, "打开结果", "文件不存在或系统无法打开该文件。");
}
