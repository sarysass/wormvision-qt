#ifndef CONTROLPANELWIDGET_H
#define CONTROLPANELWIDGET_H

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QWidget>

/**
 * @brief 相机参数控制面板
 *
 * 功能组：
 * - 设备选择
 * - 曝光控制
 * - 增益控制
 * - 帧率控制
 * - 分辨率显示 (只读)
 */
class ControlPanelWidget : public QWidget {
  Q_OBJECT

public:
  explicit ControlPanelWidget(QWidget *parent = nullptr);

  // 获取设备选择控件
  QComboBox *deviceCombo() const { return m_deviceCombo; }
  QPushButton *refreshDevicesBtn() const { return m_refreshDevicesBtn; }

signals:
  // 参数变化信号
  void exposureChanged(float value);
  void gainChanged(float value);
  void frameRateChanged(float value);
  void frameRateEnableChanged(bool enabled);

public slots:
  // 设置参数范围 (由 CameraController 信号触发)
  void setExposureRange(float min, float max, float current);
  void setGainRange(float min, float max, float current);
  void setFrameRateRange(float min, float max, float current);

  // 设置分辨率显示 (只读)
  void setResolution(int width, int height);

  // 启用/禁用所有控件
  void enableControls(bool enabled);
  void setResolutionEnabled(bool enabled);

signals:
  void offsetXChanged(int value);
  void offsetYChanged(int value);
  void widthChanged(int value);
  void heightChanged(int value);

public slots:
  void setResolutionMax(int w, int h);
  void setOffset(int x, int y);
  void setResultingFrameRate(float fps);

private slots:
  void onExposureSpinBoxChanged(double value);
  void onExposureSliderChanged(int value);
  void onGainSpinBoxChanged(double value);
  void onGainSliderChanged(int value);
  void onFrameRateSpinBoxChanged(double value);
  void onFrameRateSliderChanged(int value);
  void onFrameRateEnableToggled(bool checked);
  void onOffsetXChanged(int value);
  void onOffsetYChanged(int value);
  void onWidthChanged(int value);
  void onHeightChanged(int value);

private:
  void setupUI();
  QGroupBox *createDeviceGroup();
  QGroupBox *createExposureGroup();
  QGroupBox *createGainGroup();
  QGroupBox *createFrameRateGroup();
  QGroupBox *createResolutionGroup();

  // Slider 值与实际值转换
  int valueToSlider(float value, float min, float max);
  float sliderToValue(int slider, float min, float max);

  // 对数转换 (用于曝光时间)
  int valueToSliderLog(float value, float min, float max);
  float sliderToValueLog(int slider, float min, float max);

  // 设备选择
  QComboBox *m_deviceCombo = nullptr;
  QPushButton *m_refreshDevicesBtn = nullptr;

  // 曝光
  QGroupBox *m_exposureGroup = nullptr;
  QDoubleSpinBox *m_exposureSpinBox = nullptr;
  QSlider *m_exposureSlider = nullptr;
  float m_exposureMin = 0;
  float m_exposureMax = 1;

  // 增益
  QGroupBox *m_gainGroup = nullptr;
  QDoubleSpinBox *m_gainSpinBox = nullptr;
  QSlider *m_gainSlider = nullptr;
  float m_gainMin = 0;
  float m_gainMax = 1;

  // 帧率
  QGroupBox *m_frameRateGroup = nullptr;
  QCheckBox *m_frameRateEnableCheck = nullptr;
  QDoubleSpinBox *m_frameRateSpinBox = nullptr;
  QSlider *m_frameRateSlider = nullptr;
  float m_frameRateMin = 0;
  float m_frameRateMax = 1;
  QLabel *m_resultingFrameRateLabel = nullptr;

  // 分辨率 (只读)
  QGroupBox *m_resolutionGroup = nullptr;
  QSpinBox *m_widthSpinBox = nullptr;
  QSpinBox *m_heightSpinBox = nullptr;
  QLabel *m_resolutionMaxLabel = nullptr;
  QSpinBox *m_offsetXSpinBox = nullptr;
  QSpinBox *m_offsetYSpinBox = nullptr;
};

#endif // CONTROLPANELWIDGET_H
