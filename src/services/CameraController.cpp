#include "CameraController.h"
#include <QDebug>
#include <QImage>

#include "MvCameraControl.h"

CameraController::CameraController(QObject *parent)
    : QObject(parent), m_cameraHandle(nullptr) {
  initSDK();
}

CameraController::~CameraController() {
  if (m_isGrabbing) {
    stopGrabbing();
  }
  if (m_isOpen) {
    close();
  }
  uninitSDK();

  if (m_pDataBuf) {
    delete[] m_pDataBuf;
    m_pDataBuf = nullptr;
  }
}

void CameraController::initSDK() {
  int nRet = MV_CC_Initialize();
  if (MV_OK != nRet) {
    qDebug() << "SDK Initialize fail! nRet:" << nRet;
    return;
  }
  qDebug() << "相机 SDK 初始化成功";
}

void CameraController::uninitSDK() {
  MV_CC_Finalize();
  qDebug() << "相机 SDK 反初始化";
}

bool CameraController::open() {
  if (m_isOpen) {
    return true;
  }

  int nRet = MV_OK;
  MV_CC_DEVICE_INFO_LIST stDeviceList;
  memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));

  // 枚举设备
  nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDeviceList);
  if (MV_OK != nRet) {
    emit error(QString("枚举设备失败: %1").arg(nRet));
    return false;
  }

  if (stDeviceList.nDeviceNum == 0) {
    emit error("未找到相机设备");
    return false;
  }

  // 选择第一个设备
  MV_CC_DEVICE_INFO *pDeviceInfo = stDeviceList.pDeviceInfo[0];
  if (NULL == pDeviceInfo) {
    emit error("设备信息为空");
    return false;
  }

  // 创建句柄
  nRet = MV_CC_CreateHandle(&m_cameraHandle, pDeviceInfo);
  if (MV_OK != nRet) {
    emit error(QString("创建句柄失败: %1").arg(nRet));
    return false;
  }

  // 打开设备
  nRet = MV_CC_OpenDevice(m_cameraHandle);
  if (MV_OK != nRet) {
    MV_CC_DestroyHandle(m_cameraHandle);
    m_cameraHandle = nullptr;
    emit error(QString("打开相机失败: %1").arg(nRet));
    return false;
  }

  // 如果是 GigE 相机，设置最佳包大小
  if (pDeviceInfo->nTLayerType == MV_GIGE_DEVICE) {
    int nPacketSize = MV_CC_GetOptimalPacketSize(m_cameraHandle);
    if (nPacketSize > 0) {
      nRet =
          MV_CC_SetIntValue(m_cameraHandle, "GevSCPSPacketSize", nPacketSize);
      if (nRet != MV_OK) {
        qWarning() << "Warning: Set Packet Size fail nRet:" << nRet;
      }
    } else {
      qWarning() << "Warning: Get Packet Size fail nRet:" << nPacketSize;
    }
  }

  // 预分配缓冲区 (假设最大 20MP RGB)
  if (m_pDataBuf == nullptr) {
    m_nDataBufSize = 4096 * 3000 * 3 + 2048; // 稍微多一点
    m_pDataBuf = new unsigned char[m_nDataBufSize];
  }

  m_isOpen = true;
  emit cameraOpened();
  qDebug() << "相机已打开";

  // 恢复之前的参数设置
  setExposure(m_exposure);
  setGain(m_gain);
  setFrameRate(m_frameRate);

  // 从 SDK 获取参数范围并发射信号
  auto expRange = getExposureRange();
  emit exposureRangeReady(expRange.min, expRange.max, expRange.current);

  auto gainRange = getGainRange();
  emit gainRangeReady(gainRange.min, gainRange.max, gainRange.current);

  auto fpsRange = getFrameRateRange();
  emit frameRateRangeReady(fpsRange.min, fpsRange.max, fpsRange.current);

  return true;
}

void CameraController::close() {
  if (!m_isOpen) {
    return;
  }

  if (m_isGrabbing) {
    stopGrabbing();
  }

  if (m_cameraHandle) {
    MV_CC_CloseDevice(m_cameraHandle);
    MV_CC_DestroyHandle(m_cameraHandle);
    m_cameraHandle = nullptr;
  }

  m_isOpen = false;
  emit cameraClosed();
  qDebug() << "相机已关闭";
}

bool CameraController::startGrabbing() {
  if (!m_isOpen) {
    emit error("相机未打开");
    return false;
  }

  if (m_isGrabbing) {
    return true;
  }

  int nRet = MV_CC_StartGrabbing(m_cameraHandle);
  if (MV_OK != nRet) {
    emit error(QString("启动采集失败: %1").arg(nRet));
    return false;
  }

  m_stopRequested = false;
  m_isGrabbing = true;

  // 启动采集线程
  m_grabThread = std::thread(&CameraController::grabLoop, this);

  qDebug() << "采集已启动";
  return true;
}

void CameraController::stopGrabbing() {
  if (!m_isGrabbing) {
    return;
  }

  m_stopRequested = true;

  // 停止采集 API
  MV_CC_StopGrabbing(m_cameraHandle);

  // 等待线程结束
  if (m_grabThread.joinable()) {
    m_grabThread.join();
  }

  m_isGrabbing = false;
  qDebug() << "采集已停止";
}

void CameraController::grabLoop() {
  MV_FRAME_OUT stImageInfo = {0};
  memset(&stImageInfo, 0, sizeof(MV_FRAME_OUT));

  int nRet = MV_OK;

  while (!m_stopRequested) {
    nRet = MV_CC_GetImageBuffer(m_cameraHandle, &stImageInfo, 1000);
    if (nRet == MV_OK) {
      // 获取图像成功
      // 我们直接将数据转换到 m_pDataBuf 中

      MV_CC_PIXEL_CONVERT_PARAM stConvertParam = {0};
      stConvertParam.nWidth = stImageInfo.stFrameInfo.nWidth;
      stConvertParam.nHeight = stImageInfo.stFrameInfo.nHeight;
      stConvertParam.pSrcData = stImageInfo.pBufAddr;
      stConvertParam.nSrcDataLen = stImageInfo.stFrameInfo.nFrameLen;
      stConvertParam.enSrcPixelType = stImageInfo.stFrameInfo.enPixelType;
      // 使用 RGB8 Packed 格式 (3字节/像素)
      stConvertParam.enDstPixelType = PixelType_Gvsp_RGB8_Packed;
      stConvertParam.pDstBuffer = m_pDataBuf;
      stConvertParam.nDstBufferSize = m_nDataBufSize;

      int nRetConvert = MV_CC_ConvertPixelType(m_cameraHandle, &stConvertParam);
      if (MV_OK == nRetConvert) {
        // 构造 QImage，使用显式步长 (stride) = width * 3
        // 注意：QImage 默认构造不拷贝数据，只是包装指针
        QImage wrapperImage(m_pDataBuf, stConvertParam.nWidth,
                            stConvertParam.nHeight, stConvertParam.nWidth * 3,
                            QImage::Format_RGB888);

        // 执行深拷贝 (Deep Copy)，因为 m_pDataBuf 在下一次循环会被覆盖
        // 这里的 copy() 会处理内存对齐，生成的 QImage 是 4 字节对齐的，适合 Qt
        // 渲染
        QImage finalImage = wrapperImage.copy();

        m_frameCount++;
        emit frameReady(finalImage, m_frameCount);
      } else {
        qWarning() << "Convert Pixel fail:" << nRetConvert;
      }

      MV_CC_FreeImageBuffer(m_cameraHandle, &stImageInfo);
    } else {
      if (m_stopRequested)
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
}

void CameraController::setExposure(float microseconds) {
  m_exposure = microseconds;
  if (m_isOpen && m_cameraHandle) {
    int nRet =
        MV_CC_SetFloatValue(m_cameraHandle, "ExposureTime", microseconds);
    if (nRet != MV_OK) {
      qWarning() << "Set ExposureTime fail:" << nRet;
    }
  }
  emit parameterChanged("exposure", microseconds);
}

void CameraController::setGain(float db) {
  m_gain = db;
  if (m_isOpen && m_cameraHandle) {
    int nRet = MV_CC_SetFloatValue(m_cameraHandle, "Gain", db);
    if (nRet != MV_OK) {
      qWarning() << "Set Gain fail:" << nRet;
    }
  }
  emit parameterChanged("gain", db);
}

void CameraController::setFrameRate(float fps) {
  m_frameRate = fps;
  if (m_isOpen && m_cameraHandle) {
    // 需要先 enable AcquisitionFrameRateEnable
    MV_CC_SetBoolValue(m_cameraHandle, "AcquisitionFrameRateEnable", true);
    int nRet = MV_CC_SetFloatValue(m_cameraHandle, "AcquisitionFrameRate", fps);
    if (nRet != MV_OK) {
      qWarning() << "Set AcquisitionFrameRate fail:" << nRet;
    }
  }
  emit parameterChanged("frameRate", fps);
}

void CameraController::setBinning(int factor) {
  m_binning = factor;
  // 一般相机不支持直接设置 "Binning"，通常是 "BinningHorizontal" 和
  // "BinningVertical" 并且某些相机只支持特定整数
  if (m_isOpen && m_cameraHandle) {
    // 尝试设置，不一定成功，取决于相机能力
    int nRet = MV_CC_SetIntValue(m_cameraHandle, "BinningHorizontal", factor);
    if (nRet == MV_OK) {
      MV_CC_SetIntValue(m_cameraHandle, "BinningVertical", factor);
    } else {
      qWarning() << "Set Binning fail:" << nRet;
    }
  }
  emit parameterChanged("binning", static_cast<float>(factor));
}

CameraController::ParameterRange CameraController::getExposureRange() const {
  ParameterRange range = {100.0f, 1000000.0f, m_exposure}; // 默认值
  if (m_isOpen && m_cameraHandle) {
    MVCC_FLOATVALUE floatVal;
    int nRet = MV_CC_GetFloatValue(m_cameraHandle, "ExposureTime", &floatVal);
    if (nRet == MV_OK) {
      range.min = floatVal.fMin;
      range.max = floatVal.fMax;
      range.current = floatVal.fCurValue;
    }
  }
  return range;
}

CameraController::ParameterRange CameraController::getGainRange() const {
  ParameterRange range = {0.0f, 20.0f, m_gain}; // 默认值
  if (m_isOpen && m_cameraHandle) {
    MVCC_FLOATVALUE floatVal;
    int nRet = MV_CC_GetFloatValue(m_cameraHandle, "Gain", &floatVal);
    if (nRet == MV_OK) {
      range.min = floatVal.fMin;
      range.max = floatVal.fMax;
      range.current = floatVal.fCurValue;
    }
  }
  return range;
}

CameraController::ParameterRange CameraController::getFrameRateRange() const {
  ParameterRange range = {1.0f, 60.0f, m_frameRate}; // 默认值
  if (m_isOpen && m_cameraHandle) {
    MVCC_FLOATVALUE floatVal;
    int nRet =
        MV_CC_GetFloatValue(m_cameraHandle, "AcquisitionFrameRate", &floatVal);
    if (nRet == MV_OK) {
      range.min = floatVal.fMin;
      range.max = floatVal.fMax;
      range.current = floatVal.fCurValue;
    }
  }
  return range;
}
