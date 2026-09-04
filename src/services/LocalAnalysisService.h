#ifndef LOCALANALYSISSERVICE_H
#define LOCALANALYSISSERVICE_H

#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QUrl>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class QProcess;
class QTimer;

/** 管理本应用启动的本地分析引擎及其异步 JSON API。 */
class LocalAnalysisService : public QObject {
  Q_OBJECT

public:
  using Callback = std::function<void(const QJsonDocument &, const QString &)>;

  explicit LocalAnalysisService(QObject *parent = nullptr);
  ~LocalAnalysisService() override;

  void start();
  void stop();
  bool isReady() const;
  bool isStarting() const;
  QString program() const;
  QString script() const;
  /** 保存程序路径及可选的 Python 入口脚本；下次启动生效。 */
  void setEngine(const QString &program, const QString &script = QString());
  /** 仅接受当前引擎的 API 相对路径；回调错误字符串为空表示成功。 */
  void get(const QString &path, Callback callback);
  void post(const QString &path, const QJsonObject &body, Callback callback);

signals:
  void ready();
  void stopped();
  void errorOccurred(const QString &message);
  void statusChanged(const QString &message);

private:
  void discoverEngine();
  void readOutput();
  void probe();
  void fail(const QString &message);
  void clearSession(const QString &reason);
  void releaseProcess();
  void request(const QString &path, const QJsonObject *body, Callback callback);

  QProcess *m_process = nullptr;
  QNetworkAccessManager *m_network = nullptr;
  QTimer *m_startTimer = nullptr;
  QTimer *m_probeTimer = nullptr;
  QHash<QNetworkReply *, Callback> m_requests;
  QString m_program;
  QString m_script;
  QByteArray m_stdout;
  QByteArray m_stderr;
  QUrl m_baseUrl;
  quint64 m_generation = 0;
  bool m_ready = false;
  bool m_starting = false;
  bool m_probePending = false;
};

#endif // LOCALANALYSISSERVICE_H
