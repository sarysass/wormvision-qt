#include "LocalAnalysisService.h"
#include "utils/AppPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>
#include <QTimer>

#include <utility>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace {

constexpr const char *PROGRAM_KEY = "analysis/engineProgram";
constexpr const char *SCRIPT_KEY = "analysis/engineScript";
constexpr int START_TIMEOUT_MS = 120000;
// 创建任务前引擎可能先完成一次最长约 20 秒的许可续期。
// 留出余量，避免客户端先超时而引擎随后仍把任务加入队列。
constexpr int HTTP_TIMEOUT_MS = 45000;
constexpr int STDERR_LIMIT = 8192;

bool isLocalUrl(const QUrl &url) {
  return url.isValid() && url.scheme() == "http"
         && (url.host() == "127.0.0.1" || url.host() == "localhost")
         && url.port() > 0 && url.port() <= 65535 && url.userInfo().isEmpty()
         && (url.path().isEmpty() || url.path() == "/")
         && !url.hasQuery() && !url.hasFragment();
}

QString apiError(const QJsonDocument &document) {
  const QJsonObject error = document.object().value("error").toObject();
  QString message = error.value("message").toString();
  const QString reason = error.value("reason").toString();
  if (!reason.isEmpty()) {
    message = message.isEmpty() ? reason : message + QStringLiteral("（") + reason
                                         + QStringLiteral("）");
  }
  return message;
}

} // namespace

LocalAnalysisService::LocalAnalysisService(QObject *parent) : QObject(parent) {
  m_network = new QNetworkAccessManager(this);
  m_network->setProxy(QNetworkProxy::NoProxy);
  m_startTimer = new QTimer(this);
  m_startTimer->setSingleShot(true);
  m_startTimer->setInterval(START_TIMEOUT_MS);
  m_probeTimer = new QTimer(this);
  m_probeTimer->setSingleShot(true);
  m_probeTimer->setInterval(300);
  connect(m_startTimer, &QTimer::timeout, this, [this]() {
    fail(QStringLiteral("本地分析引擎启动超时，请检查引擎及其依赖后重试。"));
  });
  connect(m_probeTimer, &QTimer::timeout, this, &LocalAnalysisService::probe);
  discoverEngine();
}

LocalAnalysisService::~LocalAnalysisService() {
  // 析构不再调用页面回调；仅终止本对象创建的进程，不等待引擎清理 GPU。
  for (QNetworkReply *reply : m_requests.keys()) {
    reply->disconnect(this);
    reply->abort();
  }
  m_requests.clear();
  releaseProcess();
}

bool LocalAnalysisService::isReady() const { return m_ready; }
bool LocalAnalysisService::isStarting() const { return m_starting; }
QString LocalAnalysisService::program() const { return m_program; }
QString LocalAnalysisService::script() const { return m_script; }

void LocalAnalysisService::setEngine(const QString &program, const QString &script) {
  QSettings settings;
  settings.setValue(PROGRAM_KEY, program.trimmed());
  settings.setValue(SCRIPT_KEY, script.trimmed());
  settings.sync();
  discoverEngine();
}

void LocalAnalysisService::discoverEngine() {
  QSettings settings;
  m_program = settings.value(PROGRAM_KEY).toString().trimmed();
  m_script = settings.value(SCRIPT_KEY).toString().trimmed();
  if (!m_program.isEmpty()) {
    return;
  }
  m_script.clear();
  const QDir appDir(QCoreApplication::applicationDirPath());
  for (const QString &relative : {QStringLiteral("engine/microhunter.exe"),
                                  QStringLiteral("engine/microhunter/microhunter.exe")}) {
    const QString candidate = appDir.filePath(relative);
    if (QFileInfo(candidate).isFile()) {
      m_program = candidate;
      return;
    }
  }
  // 兼容从 build 目录或 IDE 启动，相邻引擎仓仍以实际文件存在为准。
  for (const QString &root : {appDir.absolutePath(), QDir::currentPath()}) {
    QDir directory(root);
    for (int level = 0; level < 4; ++level) {
      const QDir core(directory.filePath("../MicroHunter-Core"));
      const QString python = core.absoluteFilePath(".venv/Scripts/python.exe");
      const QString entry = core.absoluteFilePath("run_cli.py");
      if (QFileInfo(python).isFile() && QFileInfo(entry).isFile()) {
        m_program = QDir::cleanPath(python);
        m_script = QDir::cleanPath(entry);
        return;
      }
      if (!directory.cdUp()) {
        break;
      }
    }
  }
}

void LocalAnalysisService::start() {
  if (m_starting || m_ready) {
    return;
  }
  discoverEngine();
  if (m_program.isEmpty()) {
    emit errorOccurred(QStringLiteral("未找到本地分析引擎，请选择 microhunter.exe 或配置引擎路径。"));
    return;
  }
  const QString workspace = QDir(AppPaths::appDataDir()).filePath("analysis");
  if (!QDir().mkpath(workspace)) {
    emit errorOccurred(QStringLiteral("无法创建分析工作区：%1").arg(workspace));
    return;
  }

  ++m_generation;
  m_starting = true;
  m_stdout.clear();
  m_stderr.clear();
  m_baseUrl.clear();
  m_process = new QProcess(this);
  m_process->setProcessChannelMode(QProcess::SeparateChannels);
  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
  environment.insert("PYTHONUTF8", "1");
  environment.insert("PYTHONIOENCODING", "utf-8");
  m_process->setProcessEnvironment(environment);
  m_process->setWorkingDirectory(QFileInfo(m_script.isEmpty() ? m_program : m_script)
                                     .absolutePath());
#ifdef Q_OS_WIN
  m_process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
    args->flags |= CREATE_NO_WINDOW;
    args->startupInfo->dwFlags |= STARTF_USESHOWWINDOW;
    args->startupInfo->wShowWindow = SW_HIDE;
  });
#endif
  connect(m_process, &QProcess::readyReadStandardOutput, this,
          &LocalAnalysisService::readOutput);
  connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
    m_stderr = (m_stderr + m_process->readAllStandardError()).right(STDERR_LIMIT);
  });
  connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
      fail(QStringLiteral("无法启动本地分析引擎：%1").arg(m_process->errorString()));
    }
  });
  connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
          [this](int exitCode, QProcess::ExitStatus) {
    m_stderr = (m_stderr + m_process->readAllStandardError()).right(STDERR_LIMIT);
    fail(QStringLiteral("本地分析引擎已退出（代码 %1），可重新启动。").arg(exitCode));
  });

  QStringList arguments;
  if (!m_script.isEmpty()) {
    arguments << "-u" << m_script;
  }
  arguments << "serve" << "--host" << "127.0.0.1" << "--port" << "0"
            << "--no-browser" << "--parent-pid"
            << QString::number(QCoreApplication::applicationPid())
            << "--workspace-root" << workspace;
  m_startTimer->start();
  emit statusChanged(QStringLiteral("正在启动本地分析引擎…"));
  m_process->start(m_program, arguments);
}

void LocalAnalysisService::readOutput() {
  m_stdout += m_process->readAllStandardOutput();
  qsizetype newline;
  while ((newline = m_stdout.indexOf('\n')) >= 0) {
    const QByteArray line = m_stdout.left(newline).trimmed();
    m_stdout.remove(0, newline + 1);
    const QByteArray prefix("MICROHUNTER_SERVE_URL=");
    if (!line.startsWith(prefix) || !m_baseUrl.isEmpty()) {
      continue;
    }
    const QUrl url(QString::fromUtf8(line.mid(prefix.size())), QUrl::StrictMode);
    if (!isLocalUrl(url)) {
      fail(QStringLiteral("引擎返回了无效的本地服务地址。"));
      return;
    }
    m_baseUrl = url;
    emit statusChanged(QStringLiteral("正在连接本地分析引擎…"));
    probe();
  }
  // 不保留无限长的第三方启动日志。
  m_stdout = m_stdout.right(65536);
}

void LocalAnalysisService::probe() {
  if (!m_starting || m_baseUrl.isEmpty() || m_probePending) {
    return;
  }
  m_probePending = true;
  const quint64 generation = m_generation;
  request("/api/system/info", nullptr,
          [this, generation](const QJsonDocument &document, const QString &error) {
    if (generation != m_generation || !m_starting) {
      return;
    }
    m_probePending = false;
    if (!error.isEmpty()) {
      m_probeTimer->start();
      return;
    }
    if (document.object().value("api_version").toInt(-1) != 1) {
      fail(QStringLiteral("本地分析引擎 API 版本不兼容，需要 api_version=1。"));
      return;
    }
    m_startTimer->stop();
    m_starting = false;
    m_ready = true;
    emit statusChanged(QStringLiteral("本地分析引擎已就绪"));
    emit ready();
  });
}

void LocalAnalysisService::releaseProcess() {
  QProcess *process = m_process;
  m_process = nullptr;
  if (!process) {
    return;
  }
  process->disconnect(this);
  // 让进程在事件循环中自行回收，避免 QProcess 析构同步等待阻塞界面。
  process->setParent(QCoreApplication::instance());
  if (process->state() == QProcess::NotRunning) {
    process->deleteLater();
  } else {
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            process, &QObject::deleteLater);
    connect(process, &QProcess::started, process, &QProcess::kill);
    process->kill();
  }
}

void LocalAnalysisService::clearSession(const QString &reason) {
  ++m_generation;
  m_ready = false;
  m_starting = false;
  m_probePending = false;
  m_baseUrl.clear();
  m_startTimer->stop();
  m_probeTimer->stop();
  releaseProcess();
  const QHash<QNetworkReply *, Callback> requests = std::move(m_requests);
  m_requests.clear();
  for (auto it = requests.cbegin(); it != requests.cend(); ++it) {
    it.key()->disconnect(this);
    it.key()->abort();
    it.key()->deleteLater();
    if (it.value()) {
      it.value()(QJsonDocument(), reason);
    }
  }
}

void LocalAnalysisService::stop() {
  const bool wasActive = m_starting || m_ready || m_process;
  clearSession(QStringLiteral("本地分析服务已停止。"));
  if (wasActive) {
    emit statusChanged(QStringLiteral("本地分析引擎已停止"));
    emit stopped();
  }
}

void LocalAnalysisService::fail(const QString &message) {
  QString detail = message;
  const QString stderrTail = QString::fromUtf8(m_stderr).trimmed();
  if (!stderrTail.isEmpty()) {
    detail += "\n" + stderrTail;
  }
  clearSession(detail);
  emit stopped();
  emit errorOccurred(detail);
}

void LocalAnalysisService::get(const QString &path, Callback callback) {
  if (!m_ready) {
    QTimer::singleShot(0, this, [callback = std::move(callback)]() {
      if (callback) {
        callback(QJsonDocument(), QStringLiteral("本地分析引擎尚未就绪。"));
      }
    });
    return;
  }
  request(path, nullptr, std::move(callback));
}

void LocalAnalysisService::post(const QString &path, const QJsonObject &body,
                                Callback callback) {
  if (!m_ready) {
    QTimer::singleShot(0, this, [callback = std::move(callback)]() {
      if (callback) {
        callback(QJsonDocument(), QStringLiteral("本地分析引擎尚未就绪。"));
      }
    });
    return;
  }
  request(path, &body, std::move(callback));
}

void LocalAnalysisService::request(const QString &path, const QJsonObject *body,
                                 Callback callback) {
  const QUrl relative(path, QUrl::StrictMode);
  const QUrl url = m_baseUrl.resolved(relative);
  if (!path.startsWith("/api/") || !relative.isRelative() || !relative.isValid()
      || relative.hasFragment() || url.host() != m_baseUrl.host()
      || url.port() != m_baseUrl.port() || url.scheme() != "http") {
    QTimer::singleShot(0, this, [callback = std::move(callback)]() {
      if (callback) {
        callback(QJsonDocument(), QStringLiteral("只允许访问当前本地引擎的 API 路径。"));
      }
    });
    return;
  }
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::ManualRedirectPolicy);
  request.setTransferTimeout(m_starting ? 2000 : HTTP_TIMEOUT_MS);
  QNetworkReply *reply = body
      ? m_network->post(request, QJsonDocument(*body).toJson(QJsonDocument::Compact))
      : m_network->get(request);
  m_requests.insert(reply, std::move(callback));
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    Callback callback = m_requests.take(reply);
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
    QString error;
    if (status < 200 || status >= 300) {
      error = apiError(document);
      if (error.isEmpty()) {
        error = status > 0 ? QStringLiteral("本地引擎请求失败（HTTP %1）。").arg(status)
                           : QStringLiteral("无法连接本地引擎：%1").arg(reply->errorString());
      }
    } else if (reply->error() != QNetworkReply::NoError) {
      error = QStringLiteral("本地引擎请求失败：%1").arg(reply->errorString());
    } else if (parseError.error != QJsonParseError::NoError || document.isNull()) {
      error = QStringLiteral("本地引擎返回了无效的 JSON：%1").arg(parseError.errorString());
    }
    reply->deleteLater();
    if (callback) {
      callback(document, error);
    }
  });
}
