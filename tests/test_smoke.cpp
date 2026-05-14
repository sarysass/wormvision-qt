// Phase 0 - smoke test：验证 Qt Test 框架接通
#include <QtTest>

class TestSmoke : public QObject {
  Q_OBJECT
private slots:
  void initial_smoke_check() {
    QCOMPARE(1 + 1, 2);
    QVERIFY(QString("hello").contains("ell"));
  }
};

QTEST_GUILESS_MAIN(TestSmoke)
#include "test_smoke.moc"
