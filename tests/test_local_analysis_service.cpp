#include "services/LocalAnalysisService.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QSettings>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>

#include <cstdio>

namespace {

// 测试程序自身作为独立引擎子进程，覆盖真实的进程与 HTTP 边界。
int runTestEngine(QCoreApplication &app) {
  QTcpServer server;
  if (!server.listen(QHostAddress::LocalHost, 0)) {
    return 2;
  }
  int infoRequests = 0;
  QObject::connect(&server, &QTcpServer::newConnection, &app, [&]() {
    while (QTcpSocket *socket = server.nextPendingConnection()) {
      QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
      QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket]() {
        QByteArray input = socket->property("input").toByteArray() + socket->readAll();
        socket->setProperty("input", input);
        const qsizetype headerEnd = input.indexOf("\r\n\r\n");
        if (headerEnd < 0 || socket->property("answered").toBool()) {
          return;
        }
        int contentLength = 0;
        for (const QByteArray &line : input.left(headerEnd).split('\n')) {
          if (line.toLower().startsWith("content-length:")) {
            contentLength = line.mid(15).trimmed().toInt();
          }
        }
        if (input.size() < headerEnd + 4 + contentLength) {
          return;
        }
        socket->setProperty("answered", true);
        const QByteArray path = input.split(' ').value(1);
        QByteArray body = "{}";
        QByteArray status = "200 OK";
        QByteArray extra;
        if (path == "/api/system/info") {
          // URL 已输出时服务还未就绪，客户端必须继续握手。
          ++infoRequests;
          if (infoRequests < 2) {
            status = "503 Service Unavailable";
            body = R"({"error":{"message":"warming up"}})";
          } else {
            body = R"({"api_version":1})";
          }
        } else if (path == "/api/launch") {
          body = QJsonDocument(QJsonObject{
              {"args", QJsonArray::fromStringList(app.arguments())},
              {"info_requests", infoRequests}}).toJson(QJsonDocument::Compact);
        } else if (path == "/api/echo") {
          body = input.mid(headerEnd + 4, contentLength);
        } else if (path == "/api/fail") {
          status = "403 Forbidden";
          body = QJsonDocument(QJsonObject{{"error", QJsonObject{
              {"message", QStringLiteral("无法分析：D:/实验/线虫.avi")},
              {"reason", "not_activated"}}}}).toJson(QJsonDocument::Compact);
        } else if (path == "/api/bad-json") {
          body = "not json";
        } else if (path == "/api/redirect") {
          status = "302 Found";
          extra = "Location: http://192.0.2.1:1/outside\r\n";
        } else if (path == "/api/slow") {
          return;
        }
        socket->write("HTTP/1.1 " + status + "\r\nContent-Type: application/json\r\n"
                      "Connection: close\r\n" + extra + "Content-Length: "
                      + QByteArray::number(body.size()) + "\r\n\r\n" + body);
        socket->disconnectFromHost();
      });
    }
  });
  QTextStream(stdout) << "MICROHUNTER_SERVE_URL=http://127.0.0.1:"
                     << server.serverPort() << Qt::endl;
  return app.exec();
}

} // namespace

class TestLocalAnalysisService : public QObject {
  Q_OBJECT

private slots:
  void failedStartCanRecoverAndHandshake() {
    LocalAnalysisService service;
    QSignalSpy errors(&service, &LocalAnalysisService::errorOccurred);
    QSignalSpy ready(&service, &LocalAnalysisService::ready);
    service.setEngine(QDir::tempPath() + "/wormvision-missing-engine.exe");
    service.start();
    QTRY_COMPARE_WITH_TIMEOUT(errors.count(), 1, 5000);
    QVERIFY(!service.isStarting());
    QVERIFY(!service.isReady());

    service.setEngine(QCoreApplication::applicationFilePath());
    LocalAnalysisService saved;
    QCOMPARE(saved.program(), service.program());
    QCOMPARE(saved.script(), QString());
    service.start();
    QVERIFY(!service.isReady());
    QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 5000);
    QVERIFY(!service.isStarting());

    bool done = false;
    service.get("/api/launch", [&](const QJsonDocument &doc, const QString &error) {
      QVERIFY2(error.isEmpty(), qPrintable(error));
      QVERIFY(doc.object().value("info_requests").toInt() >= 2);
      const QJsonArray args = doc.object().value("args").toArray();
      QVERIFY(args.contains("serve"));
      QVERIFY(args.contains("--no-browser"));
      QVERIFY(args.contains(QString::number(QCoreApplication::applicationPid())));
      QVERIFY(args.contains("--workspace-root"));
      done = true;
    });
    QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
  }

  void httpErrorsAndUnicodeKeepServiceReady() {
    LocalAnalysisService service;
    service.setEngine(QCoreApplication::applicationFilePath());
    service.start();
    QTRY_VERIFY_WITH_TIMEOUT(service.isReady(), 5000);

    int completed = 0;
    service.get("/api/fail", [&](const QJsonDocument &, const QString &error) {
      QVERIFY(error.contains(QStringLiteral("D:/实验/线虫.avi")));
      QVERIFY(error.contains("not_activated"));
      ++completed;
    });
    service.get("/api/bad-json", [&](const QJsonDocument &, const QString &error) {
      QVERIFY(!error.isEmpty());
      ++completed;
    });
    service.get("/api/redirect", [&](const QJsonDocument &, const QString &error) {
      QVERIFY(error.contains("302"));
      ++completed;
    });
    const QJsonObject input{{"path", QStringLiteral("D:/实验 空格/线虫.avi")}};
    service.post("/api/echo", input,
                 [&](const QJsonDocument &doc, const QString &error) {
      QVERIFY2(error.isEmpty(), qPrintable(error));
      QCOMPARE(doc.object(), input);
      ++completed;
    });
    QTRY_COMPARE_WITH_TIMEOUT(completed, 4, 5000);
    QVERIFY(service.isReady());
  }

  void stopInvalidatesRequestsAndAllowsRestart() {
    LocalAnalysisService service;
    service.setEngine(QCoreApplication::applicationFilePath());
    service.start();
    QTRY_VERIFY_WITH_TIMEOUT(service.isReady(), 5000);

    int slowCalls = 0;
    service.get("/api/slow", [&](const QJsonDocument &, const QString &error) {
      QVERIFY(!error.isEmpty());
      ++slowCalls;
    });
    bool rejected = false;
    service.get("http://192.0.2.1/outside",
                [&](const QJsonDocument &, const QString &error) {
      QVERIFY(!error.isEmpty());
      rejected = true;
    });
    QTRY_VERIFY(rejected);
    QSignalSpy stopped(&service, &LocalAnalysisService::stopped);
    service.stop();
    QTRY_COMPARE(slowCalls, 1);
    QCOMPARE(stopped.count(), 1);
    QVERIFY(!service.isReady());
    QVERIFY(!service.isStarting());
    service.start();
    QTRY_VERIFY_WITH_TIMEOUT(service.isReady(), 5000);
    QCOMPARE(slowCalls, 1);
  }
};

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  if (app.arguments().value(1) == "serve") {
    return runTestEngine(app);
  }
  QTemporaryDir settings;
  if (!settings.isValid()) {
    return 2;
  }
  QCoreApplication::setOrganizationName("WormVisionTests");
  QCoreApplication::setApplicationName("LocalAnalysisService");
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
  QStandardPaths::setTestModeEnabled(true);
  TestLocalAnalysisService test;
  return QTest::qExec(&test, argc, argv);
}

#include "test_local_analysis_service.moc"
