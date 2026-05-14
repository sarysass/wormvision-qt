#ifndef VIDEOUTILS_H
#define VIDEOUTILS_H

#include <QByteArray>
#include <QString>

/**
 * @brief 视频工具函数集（纯函数，无 Qt 信号槽/无外部状态）
 *
 * 设计目标：可单元测试。所有依赖通过参数或字节流注入。
 */
namespace VideoUtils {

/**
 * @brief 把秒数格式化成 "mm:ss" 或 "h:mm:ss"
 * @param seconds 秒数，<= 0 返回 "--:--"
 */
QString formatDuration(double seconds);

/**
 * @brief 把字节数格式化成 B/KB/MB/GB
 */
QString formatFileSize(qint64 bytes);

/**
 * @brief 从文件读取视频时长（自动识别 AVI/MP4）
 * @return 秒数；无法解析时返回 0.0
 */
double parseVideoDurationFromFile(const QString &filepath);

// === 以下为暴露出来便于测试的内部函数 ===

/**
 * @brief 从内存字节流解析 AVI 文件时长
 * @param header 至少前 256 字节
 * @return 秒数；不是有效 AVI 返回 0.0
 */
double parseAviDuration(const QByteArray &header);

/**
 * @brief 从内存字节流解析 MP4 文件时长
 *
 * MP4/MOV 由 box 组成。本函数在前 N 字节里找 moov→mvhd box，
 * 读 timescale 和 duration 算出秒数。
 *
 * @param data 文件前若干字节（推荐 ≥ 4KB）
 * @return 秒数；不是有效 MP4 或找不到 mvhd 返回 0.0
 */
double parseMp4Duration(const QByteArray &data);

} // namespace VideoUtils

#endif // VIDEOUTILS_H
