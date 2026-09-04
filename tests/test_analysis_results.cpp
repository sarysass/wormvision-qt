#include "utils/AnalysisResults.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class TestAnalysisResults : public QObject {
  Q_OBJECT
private slots:
  void readsEngineTablesWithoutRecalculating() {
    // 结构与 export.py 的 _metric_table_payload 一致；数据仅用于解析边界测试。
    const auto document = QJsonDocument::fromJson(R"JSON({
      "schema_version":"1", "analysis_id":"test-run",
      "summary":{
        "columns":[
          {"key":"video_file","label":"视频文件","unit":null},
          {"key":"group","label":"实验组","unit":null},
          {"key":"motion.speed.mean","label":"平均速度","unit":"mm/s",
           "aggregation":"median_across_tracks"},
          {"key":"worm.body.length.median","label":"体长中位数","unit":"px"}],
        "rows":[{"video_id":"video-1","video_file":"实验.avi","group":"未分组",
          "values":{"motion.speed.mean":0.125,"worm.body.length.median":null}}]},
      "videos":[{"video_id":"video-1","video_file":"实验.avi",
        "columns":[{"key":"track_id","label":"轨迹ID","unit":null},
          {"key":"motion.speed.mean","label":"平均速度","unit":"mm/s"}],
        "tracks":[{"track_id":"1","source_track_id":"track-a",
          "values":{"motion.speed.mean":0}},
          {"track_id":"2","source_track_id":"track-b","values":{}}]}]
    })JSON");
    const auto report = AnalysisResults::parse(document);
    QVERIFY2(report.error.isEmpty(), qPrintable(report.error));
    QCOMPARE(report.summary.headers,
             QStringList({"视频文件", "实验组", "平均速度（mm/s）", "体长中位数（px）"}));
    QCOMPARE(report.summary.rows.size(), 1);
    QCOMPARE(report.summary.rows.first(), QStringList({"实验.avi", "未分组", "0.125", "—"}));
    QVERIFY(report.summary.tooltips.at(2).contains("中位数"));
    QCOMPARE(report.videos.size(), 1);
    QCOMPARE(report.videos.first().title, QString("实验.avi"));
    QCOMPARE(report.videos.first().rows,
             QVector<QStringList>({{"1", "0"}, {"2", "—"}}));
  }

  void preservesMissingValuesAndRejectsWrongShape() {
    QCOMPARE(AnalysisResults::displayValue(QJsonValue(QJsonValue::Null)), QString("—"));
    QCOMPARE(AnalysisResults::displayValue(QJsonValue()), QString("—"));
    QCOMPARE(AnalysisResults::displayValue(QJsonValue(0)), QString("0"));
    QVERIFY(!AnalysisResults::parse(QJsonDocument::fromJson("{\"summary\":[]}")).error.isEmpty());
  }

  void outputFilesMustStayWithinResultDirectory() {
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    QDir root(temporary.path());
    QVERIFY(root.mkpath("run/results"));
    QFile inside(root.filePath("run/results/metrics.json"));
    QVERIFY(inside.open(QIODevice::WriteOnly));
    inside.write("{}");
    inside.close();
    QFile outside(root.filePath("outside.json"));
    QVERIFY(outside.open(QIODevice::WriteOnly));
    outside.write("{}");
    outside.close();
    const QString outputDir = root.filePath("run");
    QVERIFY(!AnalysisResults::outputFile(outputDir, "results/metrics.json").isEmpty());
    QVERIFY(AnalysisResults::outputFile(outputDir, "../outside.json").isEmpty());
    QVERIFY(AnalysisResults::outputFile(outputDir, outside.fileName()).isEmpty());
    QVERIFY(AnalysisResults::outputFile(outputDir, "missing.json").isEmpty());
  }
};

QTEST_GUILESS_MAIN(TestAnalysisResults)
#include "test_analysis_results.moc"
