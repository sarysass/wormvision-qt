// Phase 1 - VideoUtils 单元测试
#include "utils/VideoUtils.h"

#include <QtTest>

// ---- 字节流构造助手 ----
namespace {

// 把 32-bit little-endian 写入 QByteArray
QByteArray u32LE(quint32 v) {
  QByteArray r(4, '\0');
  r[0] = char(v & 0xFF);
  r[1] = char((v >> 8) & 0xFF);
  r[2] = char((v >> 16) & 0xFF);
  r[3] = char((v >> 24) & 0xFF);
  return r;
}

// 把 32-bit big-endian 写入 QByteArray
QByteArray u32BE(quint32 v) {
  QByteArray r(4, '\0');
  r[0] = char((v >> 24) & 0xFF);
  r[1] = char((v >> 16) & 0xFF);
  r[2] = char((v >> 8) & 0xFF);
  r[3] = char(v & 0xFF);
  return r;
}

// 构造一个最小合法 AVI 头部，可指定时长参数
//   total_seconds = totalFrames * microSecPerFrame / 1e6
QByteArray makeAviHeader(quint32 microSecPerFrame, quint32 totalFrames) {
  QByteArray buf;
  buf.append("RIFF");
  buf.append(u32LE(1024)); // 整体 size，随便填
  buf.append("AVI ");
  buf.append("LIST");
  buf.append(u32LE(192)); // hdrl LIST size
  buf.append("hdrl");
  buf.append("avih");
  buf.append(u32LE(56)); // avih struct size
  // avih struct（56 字节，我们只关心前 24 字节）
  buf.append(u32LE(microSecPerFrame)); // +0
  buf.append(u32LE(1'000'000));        // +4 dwMaxBytesPerSec
  buf.append(u32LE(0));                // +8 dwPaddingGranularity
  buf.append(u32LE(0));                // +12 dwFlags
  buf.append(u32LE(totalFrames));      // +16 dwTotalFrames
  buf.append(u32LE(0));                // +20 dwInitialFrames
  // 填齐 56 字节
  while (buf.size() < (4 + 4 + 4) + (4 + 4 + 4) + (4 + 4 + 4 + 56)) {
    buf.append('\0');
  }
  // 再补一点尾巴，确保 indexOf 不出错
  buf.append(QByteArray(64, '\0'));
  return buf;
}

// 构造一个最小合法 MP4 头部：ftyp + moov(mvhd)
//   duration_seconds = duration_in_units / timescale
QByteArray makeMp4Header(quint32 timescale, quint32 duration_v0) {
  // ---- ftyp box ----
  QByteArray ftyp;
  ftyp.append(u32BE(16)); // size = 16
  ftyp.append("ftyp");
  ftyp.append("isom"); // major_brand
  ftyp.append(u32BE(512));

  // ---- mvhd box (v0, 100 字节 + 8 头 = 108) ----
  QByteArray mvhdPayload;
  mvhdPayload.append('\0');         // version = 0
  mvhdPayload.append(QByteArray(3, '\0')); // flags
  mvhdPayload.append(u32BE(0));     // creation_time
  mvhdPayload.append(u32BE(0));     // modification_time
  mvhdPayload.append(u32BE(timescale));
  mvhdPayload.append(u32BE(duration_v0));
  // 后面填齐到 100 字节
  mvhdPayload.append(QByteArray(100 - mvhdPayload.size(), '\0'));

  QByteArray mvhd;
  mvhd.append(u32BE(quint32(mvhdPayload.size() + 8)));
  mvhd.append("mvhd");
  mvhd.append(mvhdPayload);

  // ---- moov box（包含 mvhd） ----
  QByteArray moov;
  moov.append(u32BE(quint32(mvhd.size() + 8)));
  moov.append("moov");
  moov.append(mvhd);

  return ftyp + moov;
}

} // namespace

class TestVideoUtils : public QObject {
  Q_OBJECT
private slots:

  // ========== formatDuration ==========
  void formatDuration_zero_returns_dashes() {
    QCOMPARE(VideoUtils::formatDuration(0), QString("--:--"));
    QCOMPARE(VideoUtils::formatDuration(-5), QString("--:--"));
  }

  void formatDuration_under_one_hour_uses_mm_ss() {
    QCOMPARE(VideoUtils::formatDuration(5), QString("0:05"));
    QCOMPARE(VideoUtils::formatDuration(65), QString("1:05"));
    QCOMPARE(VideoUtils::formatDuration(3599), QString("59:59"));
  }

  void formatDuration_over_one_hour_uses_h_mm_ss() {
    QCOMPARE(VideoUtils::formatDuration(3600), QString("1:00:00"));
    QCOMPARE(VideoUtils::formatDuration(3725), QString("1:02:05"));
    QCOMPARE(VideoUtils::formatDuration(36000), QString("10:00:00"));
  }

  // ========== formatFileSize ==========
  void formatFileSize_bytes() {
    QCOMPARE(VideoUtils::formatFileSize(0), QString("0 B"));
    QCOMPARE(VideoUtils::formatFileSize(512), QString("512 B"));
    QCOMPARE(VideoUtils::formatFileSize(1023), QString("1023 B"));
  }

  void formatFileSize_kilobytes() {
    QCOMPARE(VideoUtils::formatFileSize(1024), QString("1.0 KB"));
    QCOMPARE(VideoUtils::formatFileSize(2048), QString("2.0 KB"));
  }

  void formatFileSize_megabytes() {
    QCOMPARE(VideoUtils::formatFileSize(1024LL * 1024), QString("1.00 MB"));
    QCOMPARE(VideoUtils::formatFileSize(10LL * 1024 * 1024), QString("10.00 MB"));
  }

  void formatFileSize_gigabytes() {
    QCOMPARE(VideoUtils::formatFileSize(1024LL * 1024 * 1024),
             QString("1.00 GB"));
  }

  void formatFileSize_negative_returns_zero_b() {
    QCOMPARE(VideoUtils::formatFileSize(-1), QString("0 B"));
  }

  // ========== parseAviDuration ==========
  void parseAvi_valid_header_30fps_300frames_is_10sec() {
    // 30 fps → microSecPerFrame = 33333；300 帧 → 9.99秒
    QByteArray buf = makeAviHeader(33333, 300);
    double sec = VideoUtils::parseAviDuration(buf);
    QVERIFY(sec > 9.9 && sec < 10.1);
  }

  void parseAvi_one_minute_at_60fps() {
    // 60fps → 16666 μs/frame，3600 帧 = 60 秒
    QByteArray buf = makeAviHeader(16666, 3600);
    double sec = VideoUtils::parseAviDuration(buf);
    QVERIFY(sec > 59.9 && sec < 60.1);
  }

  void parseAvi_zero_frames_returns_zero() {
    QByteArray buf = makeAviHeader(33333, 0);
    QCOMPARE(VideoUtils::parseAviDuration(buf), 0.0);
  }

  void parseAvi_non_riff_returns_zero() {
    QByteArray buf(256, '\0');
    QCOMPARE(VideoUtils::parseAviDuration(buf), 0.0);
  }

  void parseAvi_too_short_returns_zero() {
    QCOMPARE(VideoUtils::parseAviDuration(QByteArray("RIFF")), 0.0);
  }

  // ========== parseMp4Duration ==========
  void parseMp4_valid_header_returns_duration() {
    // timescale=1000，duration=5000 单位 → 5.0 秒
    QByteArray buf = makeMp4Header(1000, 5000);
    double sec = VideoUtils::parseMp4Duration(buf);
    QVERIFY2(sec > 4.99 && sec < 5.01,
             qPrintable(QString("got %1").arg(sec)));
  }

  void parseMp4_timescale_30000_duration_1800000_is_60sec() {
    // 标准 NTSC 时基 30000/1001 ≈ 29.97；30000 timescale，60 秒 = 1800000 单位
    QByteArray buf = makeMp4Header(30000, 1800000);
    double sec = VideoUtils::parseMp4Duration(buf);
    QVERIFY(sec > 59.9 && sec < 60.1);
  }

  void parseMp4_zero_timescale_returns_zero() {
    QByteArray buf = makeMp4Header(0, 5000);
    QCOMPARE(VideoUtils::parseMp4Duration(buf), 0.0);
  }

  void parseMp4_no_ftyp_returns_zero() {
    QByteArray buf(256, '\0');
    QCOMPARE(VideoUtils::parseMp4Duration(buf), 0.0);
  }

  void parseMp4_no_mvhd_returns_zero() {
    // 只有 ftyp，没 moov
    QByteArray buf;
    buf.append(u32BE(16));
    buf.append("ftyp");
    buf.append("isom");
    buf.append(u32BE(512));
    QCOMPARE(VideoUtils::parseMp4Duration(buf), 0.0);
  }

  // ========== parseVideoDurationFromFile（集成） ==========
  void parseFile_avi_temp() {
    QTemporaryFile f;
    QVERIFY(f.open());
    f.write(makeAviHeader(33333, 300));
    f.close();
    double sec = VideoUtils::parseVideoDurationFromFile(f.fileName());
    QVERIFY(sec > 9.9 && sec < 10.1);
  }

  void parseFile_mp4_temp() {
    QTemporaryFile f;
    QVERIFY(f.open());
    f.write(makeMp4Header(1000, 5000));
    f.close();
    double sec = VideoUtils::parseVideoDurationFromFile(f.fileName());
    QVERIFY(sec > 4.99 && sec < 5.01);
  }

  void parseFile_nonexistent_returns_zero() {
    QCOMPARE(VideoUtils::parseVideoDurationFromFile("Z:/no/such/file.avi"),
             0.0);
  }
};

QTEST_GUILESS_MAIN(TestVideoUtils)
#include "test_video_utils.moc"
