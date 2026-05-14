#ifndef VIDEOLIBRARYSERVICE_H
#define VIDEOLIBRARYSERVICE_H

#include "DatabaseManager.h"
#include <QString>

/**
 * @brief 视频库服务层 —— 把 UI 不感兴趣的业务逻辑从 widget 里抽出来，便于单测。
 *
 * 设计原则：
 *  - 不依赖 Qt UI（不能 include QWidget / QMessageBox）
 *  - 接收 DatabaseManager 引用，不假设单例（测试用 :memory: db）
 *  - 所有方法纯函数式，可重入
 */
namespace VideoLibraryService {

/**
 * @brief 录制完成后把文件元数据写入 DB。
 *
 * - 通过 `QFileInfo` 读取 filesize / birthTime
 * - 通过 `VideoUtils::parseVideoDurationFromFile` 解析时长
 * - 文件不存在或 0 字节时**不入库**（返回 false）—— 避免脏数据
 *
 * @param filePath 录制文件绝对路径
 * @param db DatabaseManager 引用
 * @return 成功入库返回 true
 */
bool addRecording(const QString &filePath, DatabaseManager &db);

/**
 * @brief 清理 DB 中的脏记录。
 *
 * 删除以下记录（同时把 0 字节文件本身也从磁盘删除）：
 *  - 文件不存在的 DB 记录
 *  - 文件大小为 0 的 DB 记录
 *
 * @param db DatabaseManager 引用
 * @return 被清理的记录数
 */
int pruneOrphans(DatabaseManager &db);

} // namespace VideoLibraryService

#endif // VIDEOLIBRARYSERVICE_H
