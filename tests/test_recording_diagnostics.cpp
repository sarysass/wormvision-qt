// Phase 5 - RecordingDiagnostics 单元测试
#include "utils/RecordingDiagnostics.h"
#include <QtTest>

class TestRecordingDiagnostics : public QObject {
  Q_OBJECT
private slots:

  void mono8_is_directly_recordable() {
    QVERIFY(RecordingDiagnostics::isPixelTypeDirectlyRecordable(0x01080001));
  }

  void bgr8_is_directly_recordable() {
    QVERIFY(RecordingDiagnostics::isPixelTypeDirectlyRecordable(0x02180015));
  }

  void rgb8_is_directly_recordable() {
    QVERIFY(RecordingDiagnostics::isPixelTypeDirectlyRecordable(0x02180014));
  }

  void yuv422_is_directly_recordable() {
    QVERIFY(RecordingDiagnostics::isPixelTypeDirectlyRecordable(0x02100032));
  }

  void bayer_rg8_NOT_directly_recordable() {
    // 这是 0 字节录制文件最常见的根因
    QVERIFY(!RecordingDiagnostics::isPixelTypeDirectlyRecordable(0x01080009));
  }

  void bayer_rg12_NOT_directly_recordable() {
    QVERIFY(!RecordingDiagnostics::isPixelTypeDirectlyRecordable(0x01100011));
  }

  void unknown_type_NOT_directly_recordable() {
    QVERIFY(!RecordingDiagnostics::isPixelTypeDirectlyRecordable(0xDEADBEEF));
  }

  void pixelTypeName_known() {
    QCOMPARE(RecordingDiagnostics::pixelTypeName(0x01080001), QString("Mono8"));
    QCOMPARE(RecordingDiagnostics::pixelTypeName(0x02180015), QString("BGR8"));
    QCOMPARE(RecordingDiagnostics::pixelTypeName(0x01080009),
             QString("BayerRG8"));
  }

  void pixelTypeName_unknown_shows_hex() {
    QString name = RecordingDiagnostics::pixelTypeName(0xDEADBEEF);
    QVERIFY(name.contains("deadbeef", Qt::CaseInsensitive));
  }

  void formatStats_contains_all_numbers() {
    QString s = RecordingDiagnostics::formatRecordingStats(100, 95, 5, 1234567);
    QVERIFY(s.contains("100"));
    QVERIFY(s.contains("95"));
    QVERIFY(s.contains("5"));
    QVERIFY(s.contains("1234567"));
  }
};

QTEST_GUILESS_MAIN(TestRecordingDiagnostics)
#include "test_recording_diagnostics.moc"
