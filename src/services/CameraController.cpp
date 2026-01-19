#include "CameraController.h"
#include <MvCameraControl.h>
#include <QDebug>
#include <chrono>

// ============================================================================
// 构造与析构
// ============================================================================

CameraController::CameraController(QObject *parent) : QObject(parent) {}

CameraController::~CameraController() { close(); }

// ============================================================================
// 设备管理
// ============================================================================

QList<CameraController::DeviceInfo> CameraController::enumerateDevices() {
  QList<DeviceInfo> devices;
  MV_CC_DEVICE_INFO_LIST deviceList;
  memset(&deviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));

  int ret = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &deviceList);
  if (ret != MV_OK) {
    qWarning() << "设备枚举失败:" << Qt::hex << ret;
    return devices;
  }

  for (unsigned int i = 0; i < deviceList.nDeviceNum; i++) {
    MV_CC_DEVICE_INFO *pDev = deviceList.pDeviceInfo[i];
    if (!pDev)
      continue;

    DeviceInfo info;
    info.index = static_cast<int>(i);
    info.deviceType = pDev->nTLayerType;

    if (pDev->nTLayerType == MV_GIGE_DEVICE) {
      info.name = QString::fromLatin1(reinterpret_cast<const char *>(
          pDev->SpecialInfo.stGigEInfo.chModelName));
      info.serialNumber = QString::fromLatin1(reinterpret_cast<const char *>(
          pDev->SpecialInfo.stGigEInfo.chSerialNumber));
    } else if (pDev->nTLayerType == MV_USB_DEVICE) {
      info.name = QString::fromLatin1(reinterpret_cast<const char *>(
          pDev->SpecialInfo.stUsb3VInfo.chModelName));
      info.serialNumber = QString::fromLatin1(reinterpret_cast<const char *>(
          pDev->SpecialInfo.stUsb3VInfo.chSerialNumber));
    }
    devices.append(info);
  }

  qDebug() << "已枚举" << devices.size() << "个相机";
  return devices;
}

bool CameraController::open(int deviceIndex) {
  if (m_isOpen)
    return true;

  // 枚举设备
  MV_CC_DEVICE_INFO_LIST deviceList;
  memset(&deviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
  int ret = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &deviceList);
  if (ret != MV_OK || deviceIndex >= static_cast<int>(deviceList.nDeviceNum)) {
    emit error("设备枚举失败或设备索引无效");
    return false;
  }

  MV_CC_DEVICE_INFO *pDevInfo = deviceList.pDeviceInfo[deviceIndex];

  // 创建句柄
  ret = MV_CC_CreateHandle(&m_cameraHandle, pDevInfo);
  if (ret != MV_OK) {
    emit error(QString("创建句柄失败: 0x%1").arg(ret, 8, 16, QChar('0')));
    return false;
  }

  // 打开设备
  ret = MV_CC_OpenDevice(m_cameraHandle);
  if (ret != MV_OK) {
    MV_CC_DestroyHandle(m_cameraHandle);
    m_cameraHandle = nullptr;
    emit error(QString("打开设备失败: 0x%1").arg(ret, 8, 16, QChar('0')));
    return false;
  }

  // GigE 优化包大小
  if (pDevInfo->nTLayerType == MV_GIGE_DEVICE) {
    int packetSize = MV_CC_GetOptimalPacketSize(m_cameraHandle);
    if (packetSize > 0) {
      MV_CC_SetIntValue(m_cameraHandle, "GevSCPSPacketSize", packetSize);
      qDebug() << "GigE 包大小设置为:" << packetSize;
    }
  }

  m_isOpen = true;

  // 确保触发模式关闭 (连续采集模式)
  int nRet =
      MV_CC_SetEnumValue(m_cameraHandle, "TriggerMode", MV_TRIGGER_MODE_OFF);
  if (MV_OK != nRet) {
    qWarning() << "设置触发模式关闭失败:" << Qt::hex << nRet;
  }

  // ========== MVS 方案：从 SDK 获取参数范围 ==========
  MVCC_FLOATVALUE floatVal;

  // 曝光范围
  if (MV_OK == MV_CC_GetFloatValue(m_cameraHandle, "ExposureTime", &floatVal)) {
    emit exposureRangeReady(floatVal.fMin, floatVal.fMax, floatVal.fCurValue);
    qDebug() << "曝光范围:" << floatVal.fMin << "-" << floatVal.fMax
             << ", 当前:" << floatVal.fCurValue;
  }

  // 增益范围
  if (MV_OK == MV_CC_GetFloatValue(m_cameraHandle, "Gain", &floatVal)) {
    emit gainRangeReady(floatVal.fMin, floatVal.fMax, floatVal.fCurValue);
    qDebug() << "增益范围:" << floatVal.fMin << "-" << floatVal.fMax
             << ", 当前:" << floatVal.fCurValue;
  }

  // 帧率范围
  if (MV_OK ==
      MV_CC_GetFloatValue(m_cameraHandle, "AcquisitionFrameRate", &floatVal)) {
    // 限制 UI 显示的最大帧率，避免出现 10000+ 的无效范围
    // 如果硬件返回的最大值超过 120，且当前值小于 120，则将 UI 上限限制为
    // 120
    float uiMax = floatVal.fMax;
    if (uiMax > 120.0f && floatVal.fCurValue <= 120.0f) {
      uiMax = 120.0f;
    }
    emit frameRateRangeReady(floatVal.fMin, uiMax, floatVal.fCurValue);
    qDebug() << "帧率范围:" << floatVal.fMin << "-" << floatVal.fMax
             << ", 当前:" << floatVal.fCurValue;
  }

  // 分辨率 (宽/高/最大值)
  MVCC_INTVALUE intVal;
  int w = 0, h = 0;
  int wMax = 0, hMax = 0;
  int wInc = 8, hInc = 8;

  if (MV_OK == MV_CC_GetIntValue(m_cameraHandle, "Width", &intVal)) {
    w = intVal.nCurValue;
    wMax = intVal.nMax;
    wInc = intVal.nInc > 0 ? intVal.nInc : 8;
    m_widthInc = wInc; // 保存步长
  }
  if (MV_OK == MV_CC_GetIntValue(m_cameraHandle, "Height", &intVal)) {
    h = intVal.nCurValue;
    hMax = intVal.nMax;
    hInc = intVal.nInc > 0 ? intVal.nInc : 8;
    m_heightInc = hInc; // 保存步长
  }

  if (w > 0 && h > 0) {
    emit resolutionReady(w, h, wInc, hInc);
    qDebug() << "分辨率:" << w << "x" << h << " 步长:" << wInc << "x" << hInc;
  }
  if (wMax > 0 && hMax > 0) {
    emit resolutionMaxReady(wMax, hMax);
    qDebug() << "最大分辨率:" << wMax << "x" << hMax;
  }

  // 偏移量 (OffsetX/OffsetY)
  int offX = 0, offY = 0;
  if (MV_OK == MV_CC_GetIntValue(m_cameraHandle, "OffsetX", &intVal))
    offX = intVal.nCurValue;
  if (MV_OK == MV_CC_GetIntValue(m_cameraHandle, "OffsetY", &intVal))
    offY = intVal.nCurValue;
  emit offsetReady(offX, offY);

  // 结果帧率 (ResultingFrameRate)
  if (MV_OK ==
      MV_CC_GetFloatValue(m_cameraHandle, "ResultingFrameRate", &floatVal)) {
    emit resultingFrameRateReady(floatVal.fCurValue);
    qDebug() << "结果帧率:" << floatVal.fCurValue;
  }

  emit cameraOpened();
  qDebug() << "相机已成功打开";
  return true;
}

void CameraController::close() {
  if (!m_isOpen)
    return;

  stopGrabbing();

  if (m_isRecording)
    stopRecording();

  MV_CC_CloseDevice(m_cameraHandle);
  MV_CC_DestroyHandle(m_cameraHandle);
  m_cameraHandle = nullptr;
  m_isOpen = false;

  emit cameraClosed();
  qDebug() << "相机已关闭";
}

// ============================================================================
// 图像采集
// ============================================================================

void CameraController::setDisplayHandle(void *hwnd) { m_displayHandle = hwnd; }

bool CameraController::startGrabbing() {
  if (!m_isOpen || m_isGrabbing)
    return false;

  int ret = MV_CC_StartGrabbing(m_cameraHandle);
  if (ret != MV_OK) {
    emit error(QString("开始采集失败: 0x%1").arg(ret, 8, 16, QChar('0')));
    return false;
  }

  m_isGrabbing = true;
  m_stopGrabbing = false;
  m_frameCount = 0;
  m_grabThread = std::thread(&CameraController::grabLoop, this);

  qDebug() << "开始采集";
  return true;
}

void CameraController::stopGrabbing() {
  if (!m_isGrabbing)
    return;

  m_stopGrabbing = true;
  if (m_grabThread.joinable())
    m_grabThread.join();

  MV_CC_StopGrabbing(m_cameraHandle);
  m_isGrabbing = false;

  qDebug() << "停止采集，总帧数:" << m_frameCount.load();
}

void CameraController::grabLoop() {
  MV_FRAME_OUT frameOut;
  memset(&frameOut, 0, sizeof(MV_FRAME_OUT));

  while (!m_stopGrabbing) {
    int ret = MV_CC_GetImageBuffer(m_cameraHandle, &frameOut, 1000);
    if (ret == MV_OK) {
      // 更新分辨率和像素类型
      int w = frameOut.stFrameInfo.nWidth;
      int h = frameOut.stFrameInfo.nHeight;
      int extendW = frameOut.stFrameInfo.nExtendWidth;
      int extendH = frameOut.stFrameInfo.nExtendHeight;
      int pixelType = frameOut.stFrameInfo.enPixelType;

      if (w != m_width || h != m_height) {
        m_width = w;
        m_height = h;
        emit resolutionChanged(w, h);
      }
      m_extendWidth = extendW;
      m_extendHeight = extendH;
      m_pixelType = pixelType;

      // SDK 直接渲染到窗口
      if (m_displayHandle) {
        MV_CC_IMAGE stImage = {0};
        stImage.enPixelType = frameOut.stFrameInfo.enPixelType;
        stImage.nWidth = extendW;
        stImage.nHeight = extendH;
        stImage.nImageLen = frameOut.stFrameInfo.nFrameLenEx;
        stImage.pImageBuf = frameOut.pBufAddr;

        MV_CC_DisplayOneFrameEx2(m_cameraHandle, m_displayHandle, &stImage, 0);
      }

      // 录制时输入帧
      if (m_isRecording) {
        MV_CC_INPUT_FRAME_INFO inputInfo;
        memset(&inputInfo, 0, sizeof(MV_CC_INPUT_FRAME_INFO));
        inputInfo.pData = frameOut.pBufAddr;
        inputInfo.nDataLen =
            frameOut.stFrameInfo.nFrameLenEx; // 使用 nFrameLenEx
        int nRet = MV_CC_InputOneFrame(m_cameraHandle, &inputInfo);
        if (nRet != MV_OK) {
          qWarning() << "录制输入帧失败:" << Qt::hex << nRet;
        }
      }

      // 缓存帧用于抓拍
      {
        std::lock_guard<std::mutex> lock(m_frameMutex);
        m_frameLen = frameOut.stFrameInfo.nFrameLenEx; // 使用 Ex
        m_frameBuffer.assign(frameOut.pBufAddr, frameOut.pBufAddr + m_frameLen);
      }

      m_frameCount++;
      emit frameRendered(m_frameCount);

      MV_CC_FreeImageBuffer(m_cameraHandle, &frameOut);
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  }
}

// ============================================================================
// 参数控制
// ============================================================================

void CameraController::setExposure(float microseconds) {
  if (!m_isOpen)
    return;
  int ret = MV_CC_SetFloatValue(m_cameraHandle, "ExposureTime", microseconds);
  if (ret != MV_OK) {
    qWarning() << "设置曝光失败:" << Qt::hex << ret;
  }
}

void CameraController::setGain(float db) {
  if (!m_isOpen)
    return;
  int ret = MV_CC_SetFloatValue(m_cameraHandle, "Gain", db);
  if (ret != MV_OK) {
    qWarning() << "设置增益失败:" << Qt::hex << ret;
  }
}

void CameraController::setFrameRate(float fps) {
  if (!m_isOpen)
    return;
  // MVS 方案：只有在用户启用帧率限制时才设置
  int ret = MV_CC_SetFloatValue(m_cameraHandle, "AcquisitionFrameRate", fps);
  if (ret != MV_OK) {
    qWarning() << "设置帧率失败:" << Qt::hex << ret;
  }
}

void CameraController::setFrameRateEnable(bool enable) {
  if (!m_isOpen)
    return;
  int ret =
      MV_CC_SetBoolValue(m_cameraHandle, "AcquisitionFrameRateEnable", enable);
  if (ret != MV_OK) {
    qWarning() << "设置帧率启用失败:" << Qt::hex << ret;
  }
}

void CameraController::setOffsetX(int offset) {
  if (!m_isOpen)
    return;
  int ret = MV_CC_SetIntValue(m_cameraHandle, "OffsetX", offset);
  if (ret != MV_OK)
    qWarning() << "设置OffsetX失败:" << Qt::hex << ret;
}

void CameraController::setOffsetY(int offset) {
  if (!m_isOpen)
    return;
  int ret = MV_CC_SetIntValue(m_cameraHandle, "OffsetY", offset);
  if (ret != MV_OK)
    qWarning() << "设置OffsetY失败:" << Qt::hex << ret;
}

void CameraController::setWidth(int width) {
  if (!m_isOpen)
    return;

  // 自动对齐到步长
  if (m_widthInc > 1) {
    width = (width / m_widthInc) * m_widthInc;
    if (width < m_widthInc)
      width = m_widthInc;
  }

  int ret = MV_CC_SetIntValue(m_cameraHandle, "Width", width);
  if (ret != MV_OK)
    qWarning() << "设置Width失败:" << Qt::hex << ret;
}

void CameraController::setHeight(int height) {
  if (!m_isOpen)
    return;

  // 自动对齐到步长
  if (m_heightInc > 1) {
    height = (height / m_heightInc) * m_heightInc;
    if (height < m_heightInc)
      height = m_heightInc;
  }

  int ret = MV_CC_SetIntValue(m_cameraHandle, "Height", height);
  if (ret != MV_OK)
    qWarning() << "设置Height失败:" << Qt::hex << ret;
}

// ============================================================================
// 录制功能
// ============================================================================

bool CameraController::startRecording(const QString &filePath, float fps,
                                      int bitRateKbps) {
  if (!m_isOpen || m_isRecording)
    return false;

  if (m_width == 0 || m_height == 0 || m_pixelType == 0) {
    emit recordingError("无法开始录制: 尚未获取有效帧数据");
    return false;
  }

  MV_CC_RECORD_PARAM recordParam;
  memset(&recordParam, 0, sizeof(MV_CC_RECORD_PARAM));
  recordParam.enRecordFmtType = MV_FormatType_AVI;
  recordParam.nWidth = static_cast<unsigned short>(m_width);
  recordParam.nHeight = static_cast<unsigned short>(m_height);
  recordParam.fFrameRate = fps;
  recordParam.nBitRate = bitRateKbps;
  recordParam.enPixelType = static_cast<MvGvspPixelType>(m_pixelType);

  // SDK 在 Windows 下通常需要本地编码 (GBK) 路径
  // 使用 toLocal8Bit() 而不是 toStdString() (UTF-8)
  m_recordingPath = filePath.toLocal8Bit().constData();
  recordParam.strFilePath = const_cast<char *>(m_recordingPath.c_str());

  qDebug() << "开始录制:" << filePath;
  qDebug() << "  尺寸:" << m_width << "x" << m_height;
  qDebug() << "  像素类型:" << m_pixelType;
  qDebug() << "  FPS:" << fps << ", 码率:" << bitRateKbps << "kbps";

  int ret = MV_CC_StartRecord(m_cameraHandle, &recordParam);
  if (ret != MV_OK) {
    emit recordingError(
        QString("录制启动失败: 0x%1").arg(ret, 8, 16, QChar('0')));
    return false;
  }

  m_isRecording = true;
  emit recordingStarted(filePath);
  return true;
}

void CameraController::stopRecording() {
  if (!m_isRecording)
    return;

  MV_CC_StopRecord(m_cameraHandle);
  m_isRecording = false;
  emit recordingStopped(QString::fromStdString(m_recordingPath));
  qDebug() << "录制已停止:" << QString::fromStdString(m_recordingPath);
}

// ============================================================================
// 抓拍功能
// ============================================================================

bool CameraController::saveSnapshot(const QString &filePath,
                                    SnapshotFormat format, int quality) {
  std::lock_guard<std::mutex> lock(m_frameMutex);

  if (m_frameBuffer.empty() || m_extendWidth == 0 || m_extendHeight == 0) {
    emit snapshotError("无法抓拍: 没有可用的帧数据");
    return false;
  }

  MV_CC_IMAGE stImg = {0};
  stImg.enPixelType = static_cast<MvGvspPixelType>(m_pixelType);
  stImg.nWidth = m_extendWidth;   // 使用 ExtendWidth 对齐
  stImg.nHeight = m_extendHeight; // 使用 ExtendHeight 对齐
  stImg.nImageLen = m_frameLen;
  stImg.pImageBuf = m_frameBuffer.data();

  MV_CC_SAVE_IMAGE_PARAM stSaveParams;
  memset(&stSaveParams, 0, sizeof(MV_CC_SAVE_IMAGE_PARAM));

  switch (format) {
  case FORMAT_BMP:
    stSaveParams.enImageType = MV_Image_Bmp;
    break;
  case FORMAT_JPEG:
    stSaveParams.enImageType = MV_Image_Jpeg;
    break;
  case FORMAT_PNG:
    stSaveParams.enImageType = MV_Image_Png;
    break;
  }
  stSaveParams.nQuality = quality;
  stSaveParams.iMethodValue = 1; // 均衡模式

  // Windows MVS SDK requires ANSI/GBK path for file operations
  // 使用 toLocal8Bit() 确保中文路径正确
  std::string path = filePath.toLocal8Bit().constData();

  // 使用 Ex2 接口 (参考官方 ImageSave.cpp 示例)
  int ret = MV_CC_SaveImageToFileEx2(m_cameraHandle, &stImg, &stSaveParams,
                                     const_cast<char *>(path.c_str()));
  if (ret != MV_OK) {
    emit snapshotError(QString("保存失败: 0x%1").arg(ret, 8, 16, QChar('0')));
    return false;
  }

  emit snapshotSaved(filePath);
  qDebug() << "截图已保存:" << filePath;
  return true;
}
