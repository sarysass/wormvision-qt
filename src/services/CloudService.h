#ifndef CLOUDSERVICE_H
#define CLOUDSERVICE_H

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantMap>


struct WorkspaceInfo {
  QString id;
  QString name;
};

class CloudService : public QObject {
  Q_OBJECT
public:
  static CloudService &instance();

  void fetchWorkspaces();
  void uploadFile(const QString &filePath, const QString &workspaceId);

signals:
  void workspacesFetched(const QList<WorkspaceInfo> &workspaces);
  void uploadProgress(const QString &filePath, int percent);
  void uploadFinished(const QString &filePath, bool success,
                      const QString &msg);
  void errorOccurred(const QString &msg);

private:
  explicit CloudService(QObject *parent = nullptr);
};

#endif // CLOUDSERVICE_H
