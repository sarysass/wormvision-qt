#include "CloudService.h"
#include <QDebug>
#include <QFileInfo>
#include <QTimer>

CloudService &CloudService::instance() {
  static CloudService instance;
  return instance;
}

CloudService::CloudService(QObject *parent) : QObject(parent) {}

void CloudService::fetchWorkspaces() {
  // Mock network delay
  QTimer::singleShot(1000, this, [this]() {
    QList<WorkspaceInfo> list;
    list.append(WorkspaceInfo{"ws_001", "Default Workspace"});
    list.append(WorkspaceInfo{"ws_002", "Project Alpha"});
    list.append(WorkspaceInfo{"ws_003", "Test Environment"});
    emit workspacesFetched(list);
  });
}

void CloudService::uploadFile(const QString &filePath,
                              const QString &workspaceId) {
  qDebug() << "模拟上传中" << filePath << "至" << workspaceId;

  // Validate file
  if (!QFileInfo::exists(filePath)) {
    emit errorOccurred("File not found: " + filePath);
    return;
  }

  // Mock Upload Progress
  // We create a self-destructing timer chain or a helper object.
  // For simplicity, just a few singleShots.

  // 0%
  emit uploadProgress(filePath, 0);

  QTimer::singleShot(500, this,
                     [this, filePath]() { emit uploadProgress(filePath, 33); });

  QTimer::singleShot(1000, this,
                     [this, filePath]() { emit uploadProgress(filePath, 66); });

  QTimer::singleShot(1500, this, [this, filePath]() {
    emit uploadProgress(filePath, 100);
    emit uploadFinished(filePath, true, "Upload Successful");
  });
}
