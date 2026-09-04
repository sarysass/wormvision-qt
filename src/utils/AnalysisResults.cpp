#include "AnalysisResults.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>

#include <cmath>

namespace {
AnalysisResults::Table readTable(const QJsonObject &object, const QString &rowsKey) {
  AnalysisResults::Table table;
  table.videoId = object.value("video_id").toString();
  table.title = object.value("video_file").toString();
  QStringList keys;
  for (const QJsonValue &entry : object.value("columns").toArray()) {
    const QJsonObject column = entry.toObject();
    const QString key = column.value("key").toString();
    if (key.isEmpty()) continue;
    keys.append(key);
    QString label = column.value("label").toString(key);
    const QString unit = column.value("unit").toString();
    if (!unit.isEmpty()) label += "（" + unit + "）";
    table.headers.append(label);
    QString tooltip = key;
    if (column.value("aggregation").toString() == "median_across_tracks")
      tooltip += "\n引擎输出的逐轨迹中位数";
    table.tooltips.append(tooltip);
  }
  for (const QJsonValue &entry : object.value(rowsKey).toArray()) {
    const QJsonObject row = entry.toObject();
    const QJsonObject values = row.value("values").toObject();
    QStringList cells;
    for (const QString &key : keys) {
      const bool metadata = key == "video_file" || key == "group" || key == "track_id";
      cells.append(AnalysisResults::displayValue(metadata ? row.value(key) : values.value(key)));
    }
    table.rows.append(cells);
  }
  return table;
}
} // namespace

namespace AnalysisResults {
Report parse(const QJsonDocument &document) {
  Report report;
  const QJsonObject root = document.object();
  const QJsonObject summary = root.value("summary").toObject();
  if (!document.isObject() || !summary.value("columns").isArray() ||
      !summary.value("rows").isArray() || !root.value("videos").isArray()) {
    report.error = "无法识别指标文件结构，请检查引擎版本和 metrics.json。";
    return report;
  }
  report.summary = readTable(summary, "rows");
  for (const QJsonValue &video : root.value("videos").toArray()) {
    const QJsonObject table = video.toObject();
    if (!table.value("columns").isArray() || !table.value("tracks").isArray()) {
      report.error = "逐视频指标结构不完整，请检查 metrics.json。";
      return report;
    }
    report.videos.append(readTable(table, "tracks"));
  }
  return report;
}

QString displayValue(const QJsonValue &value) {
  if (value.isDouble()) {
    const double number = value.toDouble();
    return std::isfinite(number) ? QString::number(number, 'g', 15) : QString("—");
  }
  if (value.isString()) return value.toString();
  if (value.isBool()) return value.toBool() ? "是" : "否";
  return "—";
}

QString outputFile(const QString &outputDir, const QString &relativePath) {
  if (outputDir.isEmpty() || relativePath.isEmpty() || QDir::isAbsolutePath(relativePath)) return {};
  const QString root = QFileInfo(outputDir).canonicalFilePath();
  if (root.isEmpty() || !QFileInfo(root).isDir()) return {};
  const QFileInfo candidate(QDir(root).filePath(relativePath));
  if (!candidate.isFile()) return {};
  const QString resolved = candidate.canonicalFilePath();
  const QString prefix = QDir::fromNativeSeparators(root) + "/";
#ifdef Q_OS_WIN
  const auto comparison = Qt::CaseInsensitive;
#else
  const auto comparison = Qt::CaseSensitive;
#endif
  if (!QDir::fromNativeSeparators(resolved).startsWith(prefix, comparison)) return {};
  return resolved;
}
} // namespace AnalysisResults
