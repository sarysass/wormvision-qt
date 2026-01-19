#include "widgets/ControlPanelWidget.h"
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <cmath>

// ============================================================================
// 构造函数
// ============================================================================

ControlPanelWidget::ControlPanelWidget(QWidget *parent) : QWidget(parent) {
  setupUI();
}

// ============================================================================
// UI 构建
// ============================================================================

void ControlPanelWidget::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(5, 5, 5, 5);
  mainLayout->setSpacing(10);

  mainLayout->addWidget(createDeviceGroup());
  mainLayout->addWidget(createResolutionGroup());
  mainLayout->addWidget(createExposureGroup());
  mainLayout->addWidget(createGainGroup());
  mainLayout->addWidget(createFrameRateGroup());
  mainLayout->addStretch();

  // 默认禁用所有控件 (相机未连接)
  enableControls(false);
}

QGroupBox *ControlPanelWidget::createDeviceGroup() {
  QGroupBox *group = new QGroupBox("设备选择", this);
  QVBoxLayout *layout = new QVBoxLayout(group);

  layout->addWidget(new QLabel("选择相机:", this));
  m_deviceCombo = new QComboBox(this);
  layout->addWidget(m_deviceCombo);

  m_refreshDevicesBtn = new QPushButton("刷新设备列表", this);
  layout->addWidget(m_refreshDevicesBtn);

  return group;
}

QGroupBox *ControlPanelWidget::createResolutionGroup() {
  m_resolutionGroup = new QGroupBox("分辨率控制", this);
  QGridLayout *layout = new QGridLayout(m_resolutionGroup);

  // Row 0: Max Resolution
  m_resolutionMaxLabel = new QLabel("最大: -- x --", this);
  layout->addWidget(m_resolutionMaxLabel, 0, 0, 1, 2);

  // Row 1: Width
  layout->addWidget(new QLabel("宽度:", this), 1, 0);
  m_widthSpinBox = new QSpinBox(this);
  m_widthSpinBox->setRange(0, 99999);
  layout->addWidget(m_widthSpinBox, 1, 1);
  connect(m_widthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &ControlPanelWidget::onWidthChanged);

  // Row 2: Height
  layout->addWidget(new QLabel("高度:", this), 2, 0);
  m_heightSpinBox = new QSpinBox(this);
  m_heightSpinBox->setRange(0, 99999);
  layout->addWidget(m_heightSpinBox, 2, 1);
  connect(m_heightSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &ControlPanelWidget::onHeightChanged);

  // Row 3: Offset X
  layout->addWidget(new QLabel("Offset X:", this), 3, 0);
  m_offsetXSpinBox = new QSpinBox(this);
  m_offsetXSpinBox->setRange(0, 99999);
  layout->addWidget(m_offsetXSpinBox, 3, 1);
  connect(m_offsetXSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &ControlPanelWidget::onOffsetXChanged);

  // Row 4: Offset Y
  layout->addWidget(new QLabel("Offset Y:", this), 4, 0);
  m_offsetYSpinBox = new QSpinBox(this);
  m_offsetYSpinBox->setRange(0, 99999);
  layout->addWidget(m_offsetYSpinBox, 4, 1);
  connect(m_offsetYSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
          &ControlPanelWidget::onOffsetYChanged);

  return m_resolutionGroup;
}

QGroupBox *ControlPanelWidget::createExposureGroup() {
  m_exposureGroup = new QGroupBox("曝光控制 (μs)", this);
  QVBoxLayout *layout = new QVBoxLayout(m_exposureGroup);

  m_exposureSpinBox = new QDoubleSpinBox(this);
  m_exposureSpinBox->setDecimals(1);
  m_exposureSpinBox->setSingleStep(100.0);
  layout->addWidget(m_exposureSpinBox);

  m_exposureSlider = new QSlider(Qt::Horizontal, this);
  m_exposureSlider->setRange(0, 1000);
  layout->addWidget(m_exposureSlider);

  connect(m_exposureSpinBox,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &ControlPanelWidget::onExposureSpinBoxChanged);
  connect(m_exposureSlider, &QSlider::valueChanged, this,
          &ControlPanelWidget::onExposureSliderChanged);

  return m_exposureGroup;
}

QGroupBox *ControlPanelWidget::createGainGroup() {
  m_gainGroup = new QGroupBox("增益控制 (dB)", this);
  QVBoxLayout *layout = new QVBoxLayout(m_gainGroup);

  m_gainSpinBox = new QDoubleSpinBox(this);
  m_gainSpinBox->setDecimals(2);
  m_gainSpinBox->setSingleStep(0.1);
  layout->addWidget(m_gainSpinBox);

  m_gainSlider = new QSlider(Qt::Horizontal, this);
  m_gainSlider->setRange(0, 1000);
  layout->addWidget(m_gainSlider);

  connect(m_gainSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, &ControlPanelWidget::onGainSpinBoxChanged);
  connect(m_gainSlider, &QSlider::valueChanged, this,
          &ControlPanelWidget::onGainSliderChanged);

  return m_gainGroup;
}

QGroupBox *ControlPanelWidget::createFrameRateGroup() {
  m_frameRateGroup = new QGroupBox("帧率控制 (fps)", this);
  QVBoxLayout *layout = new QVBoxLayout(m_frameRateGroup);

  m_frameRateEnableCheck = new QCheckBox("启用手动帧率限制", this);
  m_frameRateEnableCheck->setChecked(false);
  layout->addWidget(m_frameRateEnableCheck);

  m_frameRateSpinBox = new QDoubleSpinBox(this);
  m_frameRateSpinBox->setDecimals(1);
  m_frameRateSpinBox->setSingleStep(1.0);
  m_frameRateSpinBox->setEnabled(false);
  layout->addWidget(m_frameRateSpinBox);

  m_frameRateSlider = new QSlider(Qt::Horizontal, this);
  m_frameRateSlider->setRange(0, 1000);
  m_frameRateSlider->setEnabled(false);
  layout->addWidget(m_frameRateSlider);

  connect(m_frameRateEnableCheck, &QCheckBox::toggled, this,
          &ControlPanelWidget::onFrameRateEnableToggled);
  connect(m_frameRateSpinBox,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          &ControlPanelWidget::onFrameRateSpinBoxChanged);
  connect(m_frameRateSlider, &QSlider::valueChanged, this,
          &ControlPanelWidget::onFrameRateSliderChanged);

  layout->addWidget(new QLabel("实际帧率:", this));
  m_resultingFrameRateLabel = new QLabel("-- fps", this);
  layout->addWidget(m_resultingFrameRateLabel);

  return m_frameRateGroup;
}

// ============================================================================
// Slider 与 SpinBox 同步
// ============================================================================

int ControlPanelWidget::valueToSlider(float value, float min, float max) {
  if (max <= min)
    return 0;
  return static_cast<int>((value - min) / (max - min) * 1000.0f);
}

float ControlPanelWidget::sliderToValue(int slider, float min, float max) {
  return min + static_cast<float>(slider) / 1000.0f * (max - min);
}

int ControlPanelWidget::valueToSliderLog(float value, float min, float max) {
  if (value <= min)
    return 0;
  if (value >= max)
    return 1000;
  if (min <= 0)
    min = 1.0f; // Prevent log(0) or neg

  // slider = 1000 * log(val / min) / log(max / min)
  double logMin = std::log(min);
  double logMax = std::log(max);
  double logVal = std::log(value);

  return static_cast<int>((logVal - logMin) / (logMax - logMin) * 1000.0);
}

float ControlPanelWidget::sliderToValueLog(int slider, float min, float max) {
  if (slider <= 0)
    return min;
  if (slider >= 1000)
    return max;
  if (min <= 0)
    min = 1.0f;

  // val = min * (max/min)^(slider/1000)
  // log(val) = log(min) + (slider/1000) * (log(max) - log(min))
  double logMin = std::log(min);
  double logMax = std::log(max);
  double logVal =
      logMin + (static_cast<double>(slider) / 1000.0) * (logMax - logMin);

  return static_cast<float>(std::exp(logVal));
}

void ControlPanelWidget::onExposureSpinBoxChanged(double value) {
  m_exposureSlider->blockSignals(true);
  m_exposureSlider->setValue(valueToSliderLog(static_cast<float>(value),
                                              m_exposureMin, m_exposureMax));
  m_exposureSlider->blockSignals(false);
  emit exposureChanged(static_cast<float>(value));
}

void ControlPanelWidget::onExposureSliderChanged(int value) {
  float realValue = sliderToValueLog(value, m_exposureMin, m_exposureMax);
  m_exposureSpinBox->blockSignals(true);
  m_exposureSpinBox->setValue(realValue);
  m_exposureSpinBox->blockSignals(false);
  emit exposureChanged(realValue);
}

void ControlPanelWidget::onGainSpinBoxChanged(double value) {
  m_gainSlider->blockSignals(true);
  m_gainSlider->setValue(valueToSlider(value, m_gainMin, m_gainMax));
  m_gainSlider->blockSignals(false);
  emit gainChanged(static_cast<float>(value));
}

void ControlPanelWidget::onGainSliderChanged(int value) {
  float realValue = sliderToValue(value, m_gainMin, m_gainMax);
  m_gainSpinBox->blockSignals(true);
  m_gainSpinBox->setValue(realValue);
  m_gainSpinBox->blockSignals(false);
  emit gainChanged(realValue);
}

void ControlPanelWidget::onFrameRateSpinBoxChanged(double value) {
  m_frameRateSlider->blockSignals(true);
  m_frameRateSlider->setValue(
      valueToSlider(value, m_frameRateMin, m_frameRateMax));
  m_frameRateSlider->blockSignals(false);
  emit frameRateChanged(static_cast<float>(value));
}

void ControlPanelWidget::onFrameRateSliderChanged(int value) {
  float realValue = sliderToValue(value, m_frameRateMin, m_frameRateMax);
  m_frameRateSpinBox->blockSignals(true);
  m_frameRateSpinBox->setValue(realValue);
  m_frameRateSpinBox->blockSignals(false);
  emit frameRateChanged(realValue);
}

void ControlPanelWidget::onFrameRateEnableToggled(bool checked) {
  m_frameRateSpinBox->setEnabled(checked);
  m_frameRateSlider->setEnabled(checked);
  emit frameRateEnableChanged(checked);
}

// ============================================================================
// 公共 Slots
// ============================================================================

void ControlPanelWidget::setExposureRange(float min, float max, float current) {
  m_exposureMin = min;
  m_exposureMax = max;

  m_exposureSpinBox->blockSignals(true);
  m_exposureSpinBox->setRange(min, max);
  m_exposureSpinBox->setValue(current);
  m_exposureSpinBox->blockSignals(false);

  m_exposureSpinBox->blockSignals(false);

  m_exposureSlider->blockSignals(true);
  m_exposureSlider->setValue(valueToSliderLog(current, min, max));
  m_exposureSlider->blockSignals(false);
}

void ControlPanelWidget::setGainRange(float min, float max, float current) {
  m_gainMin = min;
  m_gainMax = max;

  m_gainSpinBox->blockSignals(true);
  m_gainSpinBox->setRange(min, max);
  m_gainSpinBox->setValue(current);
  m_gainSpinBox->blockSignals(false);

  m_gainSlider->blockSignals(true);
  m_gainSlider->setValue(valueToSlider(current, min, max));
  m_gainSlider->blockSignals(false);
}

void ControlPanelWidget::setFrameRateRange(float min, float max,
                                           float current) {
  m_frameRateMin = min;
  m_frameRateMax = max;

  m_frameRateSpinBox->blockSignals(true);
  m_frameRateSpinBox->setRange(min, max);
  m_frameRateSpinBox->setValue(current);
  m_frameRateSpinBox->blockSignals(false);

  m_frameRateSlider->blockSignals(true);
  m_frameRateSlider->setValue(valueToSlider(current, min, max));
  m_frameRateSlider->blockSignals(false);
}

void ControlPanelWidget::setResolution(int width, int height) {
  m_widthSpinBox->setValue(width);
  m_heightSpinBox->setValue(height);
}

void ControlPanelWidget::setResolutionMax(int w, int h) {
  m_resolutionMaxLabel->setText(QString("最大: %1 x %2").arg(w).arg(h));
}

void ControlPanelWidget::setOffset(int x, int y) {
  m_offsetXSpinBox->blockSignals(true);
  m_offsetYSpinBox->blockSignals(true);
  m_offsetXSpinBox->setValue(x);
  m_offsetYSpinBox->setValue(y);
  m_offsetXSpinBox->blockSignals(false);
  m_offsetYSpinBox->blockSignals(false);
}

void ControlPanelWidget::setResultingFrameRate(float fps) {
  m_resultingFrameRateLabel->setText(QString("%1 fps").arg(fps, 0, 'f', 2));
}

void ControlPanelWidget::onOffsetXChanged(int value) {
  emit offsetXChanged(value);
}
void ControlPanelWidget::onOffsetYChanged(int value) {
  emit offsetYChanged(value);
}

void ControlPanelWidget::enableControls(bool enabled) {
  m_exposureGroup->setEnabled(enabled);
  m_gainGroup->setEnabled(enabled);
  m_frameRateGroup->setEnabled(enabled);
  m_resolutionGroup->setEnabled(enabled);
}

void ControlPanelWidget::onWidthChanged(int value) { emit widthChanged(value); }

void ControlPanelWidget::onHeightChanged(int value) {
  emit heightChanged(value);
}

void ControlPanelWidget::setResolutionEnabled(bool enabled) {
  if (m_resolutionGroup) {
    m_resolutionGroup->setEnabled(enabled);
  }
}
