#ifndef CONTROLPANELWIDGET_H
#define CONTROLPANELWIDGET_H

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>


/**
 * @brief 相机控制面板
 *
 * 包含：
 * - 曝光时间控制
 * - 增益控制
 * - 帧率控制
 * - Binning 设置
 */
class ControlPanelWidget : public QWidget {
  Q_OBJECT

public:
  explicit ControlPanelWidget(QWidget *parent = nullptr);
  ~ControlPanelWidget();

signals:
  void exposureChanged(float microseconds);
  void gainChanged(float db);
  void frameRateChanged(float fps);
  void binningChanged(int factor);

public slots:
  void setExposureRange(float min, float max, float current);
  void setGainRange(float min, float max, float current);
  void setFrameRateRange(float min, float max, float current);

private slots:
  void onExposureSliderChanged(int value);
  void onGainSliderChanged(int value);
  void onFrameRateSliderChanged(int value);
  void onBinningChanged(int index);

private:
  void setupUI();
  QGroupBox *createExposureGroup();
  QGroupBox *createGainGroup();
  QGroupBox *createFrameRateGroup();
  QGroupBox *createBinningGroup();

  // 曝光控制
  QSlider *m_exposureSlider;
  QDoubleSpinBox *m_exposureSpinBox;
  float m_exposureMin = 100.0f;
  float m_exposureMax = 1000000.0f;

  // 增益控制
  QSlider *m_gainSlider;
  QDoubleSpinBox *m_gainSpinBox;
  float m_gainMin = 0.0f;
  float m_gainMax = 20.0f;

  // 帧率控制
  QSlider *m_frameRateSlider;
  QDoubleSpinBox *m_frameRateSpinBox;
  float m_frameRateMin = 1.0f;
  float m_frameRateMax = 60.0f;

  // Binning 控制
  QComboBox *m_binningCombo;
};

#endif // CONTROLPANELWIDGET_H
