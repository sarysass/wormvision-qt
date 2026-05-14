#include "VideoUtils.h"

#include <QChar>
#include <QFile>
#include <QtEndian>
#include <cstdint>
#include <cstring>

namespace VideoUtils {

// ============================================================================
// 格式化
// ============================================================================

QString formatDuration(double seconds) {
  if (seconds <= 0) {
    return QStringLiteral("--:--");
  }
  const int total = static_cast<int>(seconds);
  const int h = total / 3600;
  const int m = (total % 3600) / 60;
  const int s = total % 60;
  if (h > 0) {
    return QString("%1:%2:%3")
        .arg(h)
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
  }
  return QString("%1:%2").arg(m).arg(s, 2, 10, QChar('0'));
}

QString formatFileSize(qint64 bytes) {
  if (bytes < 0) {
    return QStringLiteral("0 B");
  }
  if (bytes < 1024) {
    return QString("%1 B").arg(bytes);
  }
  if (bytes < 1024 * 1024) {
    return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
  }
  if (bytes < 1024LL * 1024 * 1024) {
    return QString("%1 MB").arg(bytes / (1024.0 * 1024), 0, 'f', 2);
  }
  return QString("%1 GB").arg(bytes / (1024.0 * 1024 * 1024), 0, 'f', 2);
}

// ============================================================================
// AVI 解析
// ============================================================================
//
// AVI 是 RIFF 容器：
//   RIFF <size> AVI  LIST <size> hdrl avih <size> <avih-struct>
//
// avih 结构（共 56 字节）前 24 字节：
//   uint32 dwMicroSecPerFrame
//   uint32 dwMaxBytesPerSec
//   uint32 dwPaddingGranularity
//   uint32 dwFlags
//   uint32 dwTotalFrames
//   ...
//
// 时长 = totalFrames * microSecPerFrame / 1e6
// ============================================================================

double parseAviDuration(const QByteArray &header) {
  if (header.size() < 64) {
    return 0.0;
  }
  // 验证 RIFF...AVI  签名
  if (header.mid(0, 4) != QByteArray("RIFF") ||
      header.mid(8, 4) != QByteArray("AVI ")) {
    return 0.0;
  }

  const int avihPos = header.indexOf("avih");
  if (avihPos < 0 || avihPos + 4 + 4 + 20 > header.size()) {
    return 0.0;
  }

  // 跳过 "avih"(4) 和 size 字段(4)，开始读 struct
  const char *data = header.constData() + avihPos + 8;
  quint32 microSecPerFrame = 0;
  quint32 totalFrames = 0;
  std::memcpy(&microSecPerFrame, data, 4);
  std::memcpy(&totalFrames, data + 16, 4);

  // AVI 是 little-endian
  microSecPerFrame = qFromLittleEndian(microSecPerFrame);
  totalFrames = qFromLittleEndian(totalFrames);

  if (microSecPerFrame == 0 || totalFrames == 0) {
    return 0.0;
  }
  return static_cast<double>(totalFrames) * microSecPerFrame / 1'000'000.0;
}

// ============================================================================
// MP4 解析
// ============================================================================
//
// MP4 是 ISO base media file format，由 box 组成：
//   [4 bytes size][4 bytes type][...payload...]
//
// 顶层：ftyp, moov, mdat 等
// 在 moov 里找 mvhd（movie header）。mvhd 内含 timescale + duration。
//
// mvhd v0：
//   uint8 version (0)
//   uint8[3] flags
//   uint32 creation_time
//   uint32 modification_time
//   uint32 timescale
//   uint32 duration
//
// mvhd v1：
//   uint8 version (1)
//   uint8[3] flags
//   uint64 creation_time
//   uint64 modification_time
//   uint32 timescale
//   uint64 duration
//
// 时长 = duration / timescale
//
// 注意：box size 可能 == 1 表示扩展为 64-bit size（紧跟 8 字节大 size），
//       size == 0 表示延伸到文件末尾。本实现简单处理：把这种 box 当作不可遍历。
// ============================================================================

static quint32 readU32BE(const char *p) {
  quint32 v;
  std::memcpy(&v, p, 4);
  return qFromBigEndian(v);
}

static quint64 readU64BE(const char *p) {
  quint64 v;
  std::memcpy(&v, p, 8);
  return qFromBigEndian(v);
}

// 在 [data, data+len) 范围内查找指定 type 的 box，返回 payload 起始偏移
// （相对于 data），找不到返回 -1。
static qsizetype findBox(const char *data, qsizetype len, const char *type,
                         qsizetype &outPayloadSize) {
  qsizetype pos = 0;
  while (pos + 8 <= len) {
    quint32 size = readU32BE(data + pos);
    if (size == 1) {
      // 64-bit largesize 不支持，停止扫描
      return -1;
    }
    if (size == 0) {
      // 延伸到末尾
      size = static_cast<quint32>(len - pos);
    }
    if (size < 8 || pos + size > static_cast<qsizetype>(len)) {
      return -1;
    }
    if (std::memcmp(data + pos + 4, type, 4) == 0) {
      outPayloadSize = static_cast<qsizetype>(size) - 8;
      return pos + 8;
    }
    pos += size;
  }
  return -1;
}

double parseMp4Duration(const QByteArray &data) {
  if (data.size() < 16) {
    return 0.0;
  }
  // 必须以 ftyp 开头（mp4/mov 都有）
  if (std::memcmp(data.constData() + 4, "ftyp", 4) != 0) {
    return 0.0;
  }

  // 在顶层找 moov
  qsizetype moovPayloadSize = 0;
  qsizetype moovPayloadOffset =
      findBox(data.constData(), data.size(), "moov", moovPayloadSize);
  if (moovPayloadOffset < 0) {
    return 0.0;
  }

  // 在 moov payload 里找 mvhd
  qsizetype mvhdPayloadSize = 0;
  qsizetype mvhdPayloadOffset =
      findBox(data.constData() + moovPayloadOffset, moovPayloadSize, "mvhd",
              mvhdPayloadSize);
  if (mvhdPayloadOffset < 0 || mvhdPayloadSize < 20) {
    return 0.0;
  }

  const char *mvhd = data.constData() + moovPayloadOffset + mvhdPayloadOffset;
  const quint8 version = static_cast<quint8>(mvhd[0]);

  quint32 timescale = 0;
  quint64 duration = 0;
  if (version == 0) {
    if (mvhdPayloadSize < 4 + 4 + 4 + 4 + 4) {
      return 0.0;
    }
    timescale = readU32BE(mvhd + 4 + 4 + 4);
    duration = readU32BE(mvhd + 4 + 4 + 4 + 4);
  } else if (version == 1) {
    if (mvhdPayloadSize < 4 + 8 + 8 + 4 + 8) {
      return 0.0;
    }
    timescale = readU32BE(mvhd + 4 + 8 + 8);
    duration = readU64BE(mvhd + 4 + 8 + 8 + 4);
  } else {
    return 0.0;
  }

  if (timescale == 0) {
    return 0.0;
  }
  return static_cast<double>(duration) / static_cast<double>(timescale);
}

// ============================================================================
// 文件入口
// ============================================================================

double parseVideoDurationFromFile(const QString &filepath) {
  QFile file(filepath);
  if (!file.open(QIODevice::ReadOnly)) {
    return 0.0;
  }
  // 读前 4KB 足够覆盖大多数 AVI/MP4 的头部 box
  const QByteArray head = file.read(4096);
  file.close();
  if (head.size() < 12) {
    return 0.0;
  }

  // 优先按签名判断
  if (head.mid(0, 4) == QByteArray("RIFF") &&
      head.mid(8, 4) == QByteArray("AVI ")) {
    return parseAviDuration(head);
  }
  if (head.size() >= 8 && std::memcmp(head.constData() + 4, "ftyp", 4) == 0) {
    return parseMp4Duration(head);
  }
  return 0.0;
}

} // namespace VideoUtils
