#ifndef ANALYSISRESULTS_H
#define ANALYSISRESULTS_H

#include <QJsonDocument>
#include <QStringList>
#include <QVector>

/** 仅展示引擎固化的指标，不在界面重新计算科研数据。 */
namespace AnalysisResults {
struct Table {
  QString videoId;
  QString title;
  QStringList headers;
  QStringList tooltips;
  QVector<QStringList> rows;
};

struct Report {
  Table summary;
  QVector<Table> videos;
  QString error;
};

Report parse(const QJsonDocument &document);
QString displayValue(const QJsonValue &value);
/** 只接受结果目录内实际存在的文件，符号链接解析后也须位于目录内。 */
QString outputFile(const QString &outputDir, const QString &relativePath);
} // namespace AnalysisResults

#endif // ANALYSISRESULTS_H
