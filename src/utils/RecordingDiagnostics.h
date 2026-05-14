#ifndef RECORDINGDIAGNOSTICS_H
#define RECORDINGDIAGNOSTICS_H

#include <QString>
#include <cstdint>

/**
 * @brief 录制相关的纯诊断工具（无 SDK 依赖，可单测）
 */
namespace RecordingDiagnostics {

/**
 * @brief 判断一个海康 MvGvspPixelType 值能否直接 AVI 录制。
 *
 * 海康 SDK 的 MV_CC_StartRecord 只支持 Mono8 / BGR8 / RGB8 / YUV422 等少数格式，
 * Bayer / 10/12-bit / 压缩格式 必须先经 MV_CC_ConvertPixelType 转 BGR8 再
 * InputOneFrame。
 *
 * @param mvGvspPixelType 海康的 MvGvspPixelType 枚举值（uint32）
 */
bool isPixelTypeDirectlyRecordable(quint32 mvGvspPixelType);

/**
 * @brief 把像素类型枚举值转人类可读名字（用于日志）
 */
QString pixelTypeName(quint32 mvGvspPixelType);

/**
 * @brief 录制结束时把统计数据格式化成给用户看的字符串。
 *
 * @param totalFrames grab 总帧数
 * @param inputOk 调用 MV_CC_InputOneFrame 成功次数
 * @param inputFail 调用 MV_CC_InputOneFrame 失败次数
 * @param fileSizeBytes 最终文件大小（用于检测"0 字节"故障）
 */
QString formatRecordingStats(qint64 totalFrames, qint64 inputOk,
                             qint64 inputFail, qint64 fileSizeBytes);

} // namespace RecordingDiagnostics

#endif // RECORDINGDIAGNOSTICS_H
