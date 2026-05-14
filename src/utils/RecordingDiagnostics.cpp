#include "RecordingDiagnostics.h"

namespace RecordingDiagnostics {

// 海康 MvGvspPixelType 常用值（来自 SDK 头文件 PixelType.h）
// 完整列表很长，这里只列我们关心的"可直接录制"白名单。
namespace {
constexpr quint32 PT_Mono8 = 0x01080001;
constexpr quint32 PT_RGB8_Packed = 0x02180014;  // RGB8
constexpr quint32 PT_BGR8_Packed = 0x02180015;  // BGR8
constexpr quint32 PT_YUV422_Packed = 0x02100032;
constexpr quint32 PT_YUV422_YUYV_Packed = 0x02100047;

constexpr quint32 PT_BayerGR8 = 0x01080008;
constexpr quint32 PT_BayerRG8 = 0x01080009;
constexpr quint32 PT_BayerGB8 = 0x0108000A;
constexpr quint32 PT_BayerBG8 = 0x0108000B;
constexpr quint32 PT_BayerRG10 = 0x0110000D;
constexpr quint32 PT_BayerRG12 = 0x01100011;
} // namespace

bool isPixelTypeDirectlyRecordable(quint32 t) {
  switch (t) {
  case PT_Mono8:
  case PT_RGB8_Packed:
  case PT_BGR8_Packed:
  case PT_YUV422_Packed:
  case PT_YUV422_YUYV_Packed:
    return true;
  default:
    return false;
  }
}

QString pixelTypeName(quint32 t) {
  switch (t) {
  case PT_Mono8:
    return QStringLiteral("Mono8");
  case PT_RGB8_Packed:
    return QStringLiteral("RGB8");
  case PT_BGR8_Packed:
    return QStringLiteral("BGR8");
  case PT_YUV422_Packed:
    return QStringLiteral("YUV422");
  case PT_YUV422_YUYV_Packed:
    return QStringLiteral("YUV422_YUYV");
  case PT_BayerGR8:
    return QStringLiteral("BayerGR8");
  case PT_BayerRG8:
    return QStringLiteral("BayerRG8");
  case PT_BayerGB8:
    return QStringLiteral("BayerGB8");
  case PT_BayerBG8:
    return QStringLiteral("BayerBG8");
  case PT_BayerRG10:
    return QStringLiteral("BayerRG10");
  case PT_BayerRG12:
    return QStringLiteral("BayerRG12");
  default:
    return QString("0x%1").arg(t, 8, 16, QChar('0'));
  }
}

QString formatRecordingStats(qint64 totalFrames, qint64 inputOk,
                             qint64 inputFail, qint64 fileSizeBytes) {
  return QString("录制统计：grab=%1, input成功=%2, input失败=%3, 文件=%4 字节")
      .arg(totalFrames)
      .arg(inputOk)
      .arg(inputFail)
      .arg(fileSizeBytes);
}

} // namespace RecordingDiagnostics
