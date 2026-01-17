#include "ControlPanelWidget.h"
#include <QFormLayout>
#include <algorithm>

ControlPanelWidget::ControlPanelWidget(QWidget *parent) : QWidget(parent) {
  setupUI();
}

ControlPanelWidget::~ControlPanelWidget() {}

void ControlPanelWidget::setupUI() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(10, 10, 10, 10);

  // 标题
  QLabel *titleLabel = new QLabel("相机参数", this);
  titleLabel->setStyleSheet(
      "font-size: 16px; font-weight: bold; padding: 5px;");
  mainLayout->addWidget(titleLabel);

  // 各参数组
  mainLayout->addWidget(createExposureGroup());
  mainLayout->addWidget(createGainGroup());
  mainLayout->addWidget(createFrameRateGroup());
  mainLayout->addWidget(createBinningGroup());

  mainLayout->addStretch();
}

QGroupBox *ControlPanelWidget::createExposureGroup() {
  QGroupBox *group = new QGroupBox("曝光时间 (μs)", this);
  QVBoxLayout *layout = new QVBoxLayout(group);

  m_exposureSlider = new QSlider(Qt::Horizontal, this);
  m_exposureSlider->setRange(0, 1000);
  m_exposureSlider->setValue(100);

  m_exposureSpinBox = new QDoubleSpinBox(this);
  m_exposureSpinBox->setRange(m_exposureMin, m_exposureMax);
  m_exposureSpinBox->setValue(10000.0);
  m_exposureSpinBox->setSuffix(" μs");

  layout->addWidget(m_exposureSlider);
  layout->addWidget(m_exposureSpinBox);

  connect(m_exposureSlider, &QSlider::valueChanged, this,
          &ControlPanelWidget::onExposureSliderChanged);
  connect(m_exposureSpinBox,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          [this](double value) {
            emit exposureChanged(static_cast<float>(value));
          });

  return group;
}

QGroupBox *ControlPanelWidget::createGainGroup() {
  QGroupBox *group = new QGroupBox("增益 (dB)", this);
  QVBoxLayout *layout = new QVBoxLayout(group);

  m_gainSlider = new QSlider(Qt::Horizontal, this);
  m_gainSlider->setRange(0, 200);
  m_gainSlider->setValue(0);

  m_gainSpinBox = new QDoubleSpinBox(this);
  m_gainSpinBox->setRange(m_gainMin, m_gainMax);
  m_gainSpinBox->setValue(0.0);
  m_gainSpinBox->setSuffix(" dB");
  m_gainSpinBox->setSingleStep(0.1);

  layout->addWidget(m_gainSlider);
  layout->addWidget(m_gainSpinBox);

  connect(m_gainSlider, &QSlider::valueChanged, this,
          &ControlPanelWidget::onGainSliderChanged);
  connect(
      m_gainSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
      [this](double value) { emit gainChanged(static_cast<float>(value)); });

  return group;
}

QGroupBox *ControlPanelWidget::createFrameRateGroup() {
  QGroupBox *group = new QGroupBox("帧率 (FPS)", this);
  QVBoxLayout *layout = new QVBoxLayout(group);

  m_frameRateSlider = new QSlider(Qt::Horizontal, this);
  m_frameRateSlider->setRange(1, 100);
  m_frameRateSlider->setValue(25);

  m_frameRateSpinBox = new QDoubleSpinBox(this);
  m_frameRateSpinBox->setRange(m_frameRateMin, m_frameRateMax);
  m_frameRateSpinBox->setValue(25.0);
  m_frameRateSpinBox->setSuffix(" FPS");

  layout->addWidget(m_frameRateSlider);
  layout->addWidget(m_frameRateSpinBox);

  connect(m_frameRateSlider, &QSlider::valueChanged, this,
          &ControlPanelWidget::onFrameRateSliderChanged);
  connect(m_frameRateSpinBox,
          QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          [this](double value) {
            emit frameRateChanged(static_cast<float>(value));
          });

  return group;
}

QGroupBox *ControlPanelWidget::createBinningGroup() {
  QGroupBox *group = new QGroupBox("Binning", this);
  QVBoxLayout *layout = new QVBoxLayout(group);

  m_binningCombo = new QComboBox(this);
  m_binningCombo->addItem("1x1 (无)", 1);
  m_binningCombo->addItem("2x2", 2);
  m_binningCombo->addItem("4x4", 4);

  layout->addWidget(m_binningCombo);

  connect(m_binningCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ControlPanelWidget::onBinningChanged);

  return group;
}

void ControlPanelWidget::onExposureSliderChanged(int value) {
  // 对数映射：滑块值 0-1000 -> 曝光 100-1000000 μs
  float logMin = std::log10(m_exposureMin);
  float logMax = std::log10(m_exposureMax);
  float logValue = logMin + (logMax - logMin) * value / 1000.0f;
  float exposure = std::pow(10.0f, logValue);

  m_exposureSpinBox->blockSignals(true);
  m_exposureSpinBox->setValue(exposure);
  m_exposureSpinBox->blockSignals(false);

  emit exposureChanged(exposure);
}

void ControlPanelWidget::onGainSliderChanged(int value) {
  float gain = m_gainMin + (m_gainMax - m_gainMin) * value / 200.0f;

  m_gainSpinBox->blockSignals(true);
  m_gainSpinBox->setValue(gain);
  m_gainSpinBox->blockSignals(false);

  emit gainChanged(gain);
}

void ControlPanelWidget::onFrameRateSliderChanged(int value) {
  float fps = static_cast<float>(value);

  m_frameRateSpinBox->blockSignals(true);
  m_frameRateSpinBox->setValue(fps);
  m_frameRateSpinBox->blockSignals(false);

  emit frameRateChanged(fps);
}

void ControlPanelWidget::onBinningChanged(int index) {
  int factor = m_binningCombo->itemData(index).toInt();
  emit binningChanged(factor);
}

void ControlPanelWidget::setExposureRange(float min, float max, float current) {
  m_exposureMin = min;
  m_exposureMax = max;

  m_exposureSpinBox->blockSignals(true);
  m_exposureSpinBox->setRange(min, max);
  m_exposureSpinBox->setValue(current);
  m_exposureSpinBox->blockSignals(false);

  // Update slider position using log mapping (reverse of
  // onExposureSliderChanged)
  m_exposureSlider->blockSignals(true);
  float logMin = std::log10(m_exposureMin);
  float logMax = std::log10(m_exposureMax);
  float logCurrent = std::log10(current);
  int sliderValue =
      static_cast<int>((logCurrent - logMin) / (logMax - logMin) * 1000.0f);
  m_exposureSlider->setValue(std::clamp(sliderValue, 0, 1000));
  m_exposureSlider->blockSignals(false);
}

void ControlPanelWidget::setGainRange(float min, float max, float current) {
  m_gainMin = min;
  m_gainMax = max;

  m_gainSpinBox->blockSignals(true);
  m_gainSpinBox->setRange(min, max);
  m_gainSpinBox->setValue(current);
  m_gainSpinBox->blockSignals(false);

  // Update slider position (reverse of onGainSliderChanged)
  m_gainSlider->blockSignals(true);
  int sliderValue = static_cast<int>((current - m_gainMin) /
                                     (m_gainMax - m_gainMin) * 200.0f);
  m_gainSlider->setValue(std::clamp(sliderValue, 0, 200));
  m_gainSlider->blockSignals(false);
}

void ControlPanelWidget::setFrameRateRange(float min, float max,
                                           float current) {
  // Cap max to reasonable value for video recording (60 FPS max)
  // Camera may report theoretical max much higher (e.g., 55000 FPS)
  float cappedMax = std::min(max, 60.0f);
  m_frameRateMin = min;
  m_frameRateMax = cappedMax;

  // For frame rate, use linear 1:1 mapping for slider
  m_frameRateSlider->blockSignals(true);
  m_frameRateSlider->setRange(static_cast<int>(min),
                              static_cast<int>(cappedMax));
  m_frameRateSlider->setValue(static_cast<int>(std::min(current, cappedMax)));
  m_frameRateSlider->blockSignals(false);

  m_frameRateSpinBox->blockSignals(true);
  m_frameRateSpinBox->setRange(min, cappedMax);
  m_frameRateSpinBox->setValue(std::min(current, cappedMax));
  m_frameRateSpinBox->blockSignals(false);
}
