#include "CameraController.h"
#include "../utils/RecordingDiagnostics.h"
#include <MvCameraControl.h>
#include <QDebug>
#include <QFileInfo>
#include <QTimer>
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

    // Phase 3 修复 #5：原代码用 fromLatin1，设备名/序列号含中文（用户自定义名）会乱码。
    // 海康 SDK 在 Windows 上字段是本地编码（GBK），用 fromLocal8Bit 解码。
    if (pDev->nTLayerType == MV_GIGE_DEVICE) {
      info.name = QString::fromLocal8Bit(reinterpret_cast<const char *>(
          pDev->SpecialInfo.stGigEInfo.chModelName));
      info.serialNumber = QString::fromLocal8Bit(reinterpret_cast<const char *>(
          pDev->SpecialInfo.stGigEInfo.chSerialNumber));
    } else if (pDev->nTLayerType == MV_USB_DEVICE) {
      info.name = QString::fromLocal8Bit(reinterpret_cast<const char *>(
          pDev->SpecialInfo.stUsb3VInfo.chModelName));
      info.serialNumber = QString::fromLocal8Bit(reinterpret_cast<const char *>(
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

      // 录制时输入帧（Phase 5：原始 Bayer 等格式需要先转 BGR8）
      // 加锁防止 stopRecording 在 InputOneFrame 中间执行 StopRecord，
      // 否则会出现 MV_E_CALLORDER (0x80000003) 把 AVI 索引写坏导致 0 字节
      std::unique_lock<std::mutex> recLock(m_recordMutex, std::defer_lock);
      if (m_isRecording) {
        recLock.lock();
      }
      // double-check：拿到锁后 stopRecording 可能已经把 isRecording 置 false
      if (m_isRecording) {
        MV_CC_INPUT_FRAME_INFO inputInfo;
        memset(&inputInfo, 0, sizeof(MV_CC_INPUT_FRAME_INFO));

        if (!m_recordingNeedsConvert) {
          inputInfo.pData = frameOut.pBufAddr;
          inputInfo.nDataLen = frameOut.stFrameInfo.nFrameLenEx;
        } else {
          // 转 BGR8：dstSize = w * h * 3
          const unsigned int dstSize =
              static_cast<unsigned int>(extendW) * extendH * 3;
          if (m_convertBuffer.size() < dstSize) {
            m_convertBuffer.resize(dstSize);
          }
          MV_CC_PIXEL_CONVERT_PARAM_EX cvt;
          memset(&cvt, 0, sizeof(cvt));
          cvt.nWidth = static_cast<unsigned short>(extendW);
          cvt.nHeight = static_cast<unsigned short>(extendH);
          cvt.pSrcData = frameOut.pBufAddr;
          cvt.nSrcDataLen = frameOut.stFrameInfo.nFrameLenEx;
          cvt.enSrcPixelType =
              static_cast<MvGvspPixelType>(frameOut.stFrameInfo.enPixelType);
          cvt.enDstPixelType = PixelType_Gvsp_BGR8_Packed;
          cvt.pDstBuffer = m_convertBuffer.data();
          cvt.nDstBufferSize = dstSize;
          int cret = MV_CC_ConvertPixelTypeEx(m_cameraHandle, &cvt);
          if (cret != MV_OK) {
            m_recordConvertFail.fetch_add(1);
            m_lastInputErrorCode.store(static_cast<quint32>(cret));
            qWarning() << "ConvertPixelTypeEx 失败:" << Qt::hex << cret;
            MV_CC_FreeImageBuffer(m_cameraHandle, &frameOut);
            continue;
          }
          inputInfo.pData = m_convertBuffer.data();
          inputInfo.nDataLen = cvt.nDstLen;
        }

        int nRet = MV_CC_InputOneFrame(m_cameraHandle, &inputInfo);
        if (nRet != MV_OK) {
          m_recordInputFail.fetch_add(1);
          m_lastInputErrorCode.store(static_cast<quint32>(nRet));
          // 写到日志文件，方便后续诊断（qInstallMessageHandler 已接管 qWarning）
          if (m_recordInputFail.load() <= 5) {
            qWarning() << "InputOneFrame 失败 #"
                       << m_recordInputFail.load()
                       << " errCode=" << Qt::hex << nRet
                       << " nDataLen=" << inputInfo.nDataLen;
          }
        } else {
          m_recordInputOk.fetch_add(1);
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

  // Phase 3 修复 #4：原代码硬编码 23fps，导致 60fps 相机录出的 AVI 严重失真。
  // fps <= 0 时从 SDK 读 ResultingFrameRate 作为真实帧率。
  if (fps <= 0.0f) {
    fps = currentResultingFps();
    if (fps <= 0.0f) {
      fps = 30.0f; // 兜底
    }
    qDebug() << "录制 fps 自动取自相机:" << fps;
  }

  // Phase 5 核心修复：判定相机当前像素类型是否被 SDK AVI 录制直接支持。
  // 不支持（如 Bayer 系列、10/12-bit）时强制走 BGR8 + 转换路径，
  // 否则 MV_CC_InputOneFrame 会静默失败，导致录出 0 字节文件。
  m_recordingNeedsConvert = !RecordingDiagnostics::isPixelTypeDirectlyRecordable(
      static_cast<quint32>(m_pixelType));
  const MvGvspPixelType recordPixelType =
      m_recordingNeedsConvert ? PixelType_Gvsp_BGR8_Packed
                              : static_cast<MvGvspPixelType>(m_pixelType);
  qDebug() << "录制像素类型:"
           << RecordingDiagnostics::pixelTypeName(
                  static_cast<quint32>(m_pixelType))
           << "→"
           << RecordingDiagnostics::pixelTypeName(
                  static_cast<quint32>(recordPixelType))
           << (m_recordingNeedsConvert ? "(需要转换)" : "(直接录制)");

  MV_CC_RECORD_PARAM recordParam;
  memset(&recordParam, 0, sizeof(MV_CC_RECORD_PARAM));
  recordParam.enRecordFmtType = MV_FormatType_AVI;
  recordParam.nWidth = static_cast<unsigned short>(m_width);
  recordParam.nHeight = static_cast<unsigned short>(m_height);
  recordParam.fFrameRate = fps;
  recordParam.nBitRate = bitRateKbps;
  recordParam.enPixelType = recordPixelType;

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

  // Phase 5：重置统计计数 + 记录实际像素类型
  m_recordInputOk = 0;
  m_recordInputFail = 0;
  m_recordConvertFail = 0;
  m_lastInputErrorCode = 0;
  m_recordingActualPixelType = static_cast<quint32>(recordPixelType);

  m_isRecording = true;
  emit recordingStarted(filePath);
  return true;
}

float CameraController::currentResultingFps() const {
  if (!m_isOpen)
    return 0.0f;
  MVCC_FLOATVALUE v;
  if (MV_OK == MV_CC_GetFloatValue(m_cameraHandle, "ResultingFrameRate", &v)) {
    return v.fCurValue;
  }
  return 0.0f;
}

void CameraController::stopRecording() {
  if (!m_isRecording)
    return;

  // 关键修复：拿锁 → 置 false → StopRecord，保证 grabLoop 不会和 StopRecord 交错
  {
    std::lock_guard<std::mutex> lock(m_recordMutex);
    m_isRecording = false;
    MV_CC_StopRecord(m_cameraHandle);
  }

  const QString path = QString::fromStdString(m_recordingPath);
  // 立即给 UI 反馈（清"录制中"label 等）
  emit recordingStopped(path);
  qDebug() << "录制已停止，轮询 SDK flush AVI 索引:" << path;

  // 关键修复：MV_CC_StopRecord 返回后 SDK 仍在异步 flush AVI 头/索引/尾。
  // 用轮询而不是固定延迟：每 300ms 查一次文件大小，size>0 立即 emit；
  // 最多等 20 * 300ms = 6 秒兜底。
  const qint64 ok = m_recordInputOk.load();
  const qint64 fail = m_recordInputFail.load();
  const qint64 convFail = m_recordConvertFail.load();
  const quint32 lastErr = m_lastInputErrorCode.load();
  const quint32 actualPixel = m_recordingActualPixelType;
  pollFlushAndEmitStats(path, ok, fail, convFail, lastErr, actualPixel, 20);
}

void CameraController::pollFlushAndEmitStats(const QString &path, qint64 ok,
                                             qint64 fail, qint64 convFail,
                                             quint32 lastErr,
                                             quint32 actualPixel,
                                             int retriesLeft) {
  const qint64 size = QFileInfo(path).size();
  if (size > 0 || retriesLeft <= 0) {
    qInfo() << RecordingDiagnostics::formatRecordingStats(
                   ok + fail + convFail, ok, fail + convFail, size)
            << "convFail=" << convFail << "lastErr=0x"
            << QString::number(lastErr, 16) << "polled" << (20 - retriesLeft)
            << "times";
    emit recordingStats(ok + fail + convFail, ok, fail, size, lastErr,
                        actualPixel, convFail);
    return;
  }
  QTimer::singleShot(300, this, [this, path, ok, fail, convFail, lastErr,
                                 actualPixel, retriesLeft]() {
    pollFlushAndEmitStats(path, ok, fail, convFail, lastErr, actualPixel,
                          retriesLeft - 1);
  });
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
