#include "services/LocalAnalysisService.h"
#include "widgets/AnalysisWidget.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QJsonArray>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTemporaryFile>
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
  QJsonObject lastRunRequest;
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
        } else if (path == "/api/license/status") {
          const bool unlicensed = qEnvironmentVariableIsSet("WORMVISION_TEST_UNLICENSED");
          body = QJsonDocument(QJsonObject{
              {"valid", !unlicensed},
              {"bundled_release", !qEnvironmentVariableIsSet("WORMVISION_TEST_SOURCE")}})
                     .toJson(QJsonDocument::Compact);
        } else if (path == "/api/runs" && input.startsWith("POST ")) {
          lastRunRequest = QJsonDocument::fromJson(
              input.mid(headerEnd + 4, contentLength)).object();
          body = R"({"id":"test-job","state":"cancelled"})";
        } else if (path == "/api/runs") {
          body = "[]";
        } else if (path == "/api/runs/test-job") {
          body = R"({"id":"test-job","state":"cancelled"})";
        } else if (path == "/api/last-run-request") {
          body = QJsonDocument(lastRunRequest).toJson(QJsonDocument::Compact);
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
  void cleanup() {
    QSettings().clear();
    qunsetenv("WORMVISION_TEST_UNLICENSED");
    qunsetenv("WORMVISION_TEST_SOURCE");
  }

  void ignoresLegacySourceEngine_data() {
    QTest::addColumn<QString>("program");
    QTest::addColumn<QString>("script");
    QTest::newRow("script-entry") << QString("D:/legacy/launcher.exe")
                                 << QString("D:/legacy/MicroHunter-Core/run_cli.py");
    QTest::newRow("python") << QString("D:/legacy/.venv/Scripts/python.exe") << QString();
    QTest::newRow("pythonw") << QString("D:/legacy/.venv/Scripts/pythonw.exe") << QString();
  }

  void ignoresLegacySourceEngine() {
    QFETCH(QString, program);
    QFETCH(QString, script);
    QSettings settings;
    settings.setValue("analysis/engineProgram", program);
    settings.setValue("analysis/engineScript", script);
    settings.sync();

    LocalAnalysisService service;
    QVERIFY2(service.program() != program, "Legacy source engine must not be selected");
    QVERIFY(!settings.contains("analysis/engineProgram"));
    QVERIFY(!settings.contains("analysis/engineScript"));
    QVERIFY(!service.program().contains("MicroHunter-Core", Qt::CaseInsensitive));
  }

  void unlicensedEngineCannotSubmit_data() {
    QTest::addColumn<bool>("source");
    QTest::newRow("release") << false;
    QTest::newRow("legacy-source") << true;
  }

  void unlicensedEngineCannotSubmit() {
    QFETCH(bool, source);
    qputenv("WORMVISION_TEST_UNLICENSED", "1");
    if (source) qputenv("WORMVISION_TEST_SOURCE", "1");
    QTemporaryFile video(QDir::tempPath() + "/wormvision-license-XXXXXX.avi");
    QVERIFY(video.open());
    QVERIFY(video.write("fake video for request validation") > 0);
    video.close();

    AnalysisWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen);
    auto *service = widget.findChild<LocalAnalysisService *>();
    QVERIFY(service);
    service->setEngine(QCoreApplication::applicationFilePath());
    auto *start = widget.findChild<QPushButton *>("primaryButton");
    QVERIFY(start);
    widget.setSelectedVideos({video.fileName()});
    widget.show();
    const auto licenseLoaded = [&]() {
      for (auto *label : widget.findChildren<QLabel *>()) {
        if (label->text().contains("许可未激活") || label->text().contains("无需激活"))
          return true;
      }
      return false;
    };
    QTRY_VERIFY_WITH_TIMEOUT(licenseLoaded(), 5000);
    QVERIFY(!start->isEnabled());
    start->click();
    QVERIFY(!widget.hasActiveAnalysis());

    bool checked = false;
    service->get("/api/last-run-request",
                 [&](const QJsonDocument &doc, const QString &error) {
      QVERIFY2(error.isEmpty(), qPrintable(error));
      QVERIFY(doc.object().isEmpty());
      checked = true;
    });
    QTRY_VERIFY_WITH_TIMEOUT(checked, 5000);
  }

  void analysisWidgetSubmitsDeviceChoice_data() {
    QTest::addColumn<QString>("deviceChoice");
    QTest::newRow("automatic") << QString("auto");
    QTest::newRow("cpu") << QString("cpu");
    QTest::newRow("cuda") << QString("cuda:0");
  }

  void analysisWidgetSubmitsDeviceChoice() {
    QFETCH(QString, deviceChoice);
    QTemporaryFile video(QDir::tempPath() + "/wormvision-device-XXXXXX.avi");
    QVERIFY(video.open());
    QVERIFY(video.write("fake video for request validation") > 0);
    video.close();

    AnalysisWidget widget;
    widget.setAttribute(Qt::WA_DontShowOnScreen);
    auto *service = widget.findChild<LocalAnalysisService *>();
    QVERIFY(service);
    service->setEngine(QCoreApplication::applicationFilePath());
    QComboBox *devices = nullptr;
    for (auto *combo : widget.findChildren<QComboBox *>()) {
      if (combo->findData("auto") >= 0) devices = combo;
    }
    QVERIFY(devices);
    devices->setCurrentIndex(devices->findData(deviceChoice));
    auto *start = widget.findChild<QPushButton *>("primaryButton");
    QVERIFY(start);
    widget.setSelectedVideos({video.fileName()});
    widget.show();
    QTRY_VERIFY_WITH_TIMEOUT(start->isEnabled(), 5000);
    start->click();
    QTRY_VERIFY_WITH_TIMEOUT(!widget.hasActiveAnalysis(), 5000);

    QJsonObject submitted;
    QString requestError;
    bool done = false;
    service->get("/api/last-run-request",
                 [&](const QJsonDocument &doc, const QString &error) {
      submitted = doc.object();
      requestError = error;
      done = true;
    });
    QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
    QVERIFY2(requestError.isEmpty(), qPrintable(requestError));
    QCOMPARE(submitted.value("videos").toArray(), QJsonArray{video.fileName()});
    QCOMPARE(submitted.value("route_id").toString(), QString("yolo-sam2-optimized-core"));
    if (deviceChoice == "auto") {
      QVERIFY2(!submitted.contains("device") || submitted.value("device").isNull(),
               "Automatic selection must use the engine default, not the literal 'auto'");
    } else {
      QCOMPARE(submitted.value("device").toString(), deviceChoice);
    }
  }

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
  QApplication app(argc, argv);
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
