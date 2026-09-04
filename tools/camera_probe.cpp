#include "CameraParams.h"
#include "MvCameraControl.h"

#include <cstdio>
#include <cstring>

static const char *errorName(unsigned int code) {
  switch (code) {
  case 0x00000000: return "MV_OK";
  case 0x80000006: return "MV_E_RESOURCE";
  case 0x8000001A: return "MV_E_NORESPONSE";
  case 0x80000203: return "MV_E_ACCESS_DENIED";
  case 0x80000204: return "MV_E_BUSY";
  case 0x80000206: return "MV_E_NETER";
  case 0x80000221: return "MV_E_IP_CONFLICT";
  default: return "UNKNOWN";
  }
}

static void printIp(const char *label, unsigned int ip) {
  std::printf("%s=%u.%u.%u.%u (0x%08X)\n", label, (ip >> 24) & 0xff,
              (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff, ip);
}

static void tryOpen(MV_CC_DEVICE_INFO *dev, unsigned int mode,
                    const char *modeName) {
  void *handle = nullptr;
  int ret = MV_CC_CreateHandle(&handle, dev);
  std::printf("CreateHandle(%s): 0x%08X %s\n", modeName,
              static_cast<unsigned int>(ret), errorName(static_cast<unsigned int>(ret)));
  if (ret != MV_OK) {
    return;
  }

  ret = MV_CC_OpenDevice(handle, mode, 0);
  std::printf("OpenDevice(%s): 0x%08X %s\n", modeName,
              static_cast<unsigned int>(ret), errorName(static_cast<unsigned int>(ret)));
  if (ret == MV_OK) {
    MV_CC_CloseDevice(handle);
  }
  MV_CC_DestroyHandle(handle);
}

int main() {
  MV_CC_DEVICE_INFO_LIST list;
  std::memset(&list, 0, sizeof(list));

  int ret = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &list);
  std::printf("EnumDevices: 0x%08X %s, count=%u\n",
              static_cast<unsigned int>(ret), errorName(static_cast<unsigned int>(ret)),
              list.nDeviceNum);
  if (ret != MV_OK || list.nDeviceNum == 0) {
    return ret == MV_OK ? 2 : 1;
  }

  for (unsigned int i = 0; i < list.nDeviceNum; ++i) {
    MV_CC_DEVICE_INFO *dev = list.pDeviceInfo[i];
    if (!dev) {
      continue;
    }

    std::printf("\nDevice[%u] type=0x%08X mac=%04X%08X\n", i, dev->nTLayerType,
                dev->nMacAddrHigh, dev->nMacAddrLow);
    if (dev->nTLayerType == MV_GIGE_DEVICE) {
      const MV_GIGE_DEVICE_INFO &g = dev->SpecialInfo.stGigEInfo;
      std::printf("model=%s serial=%s user=%s\n", g.chModelName,
                  g.chSerialNumber, g.chUserDefinedName);
      printIp("camera.currentIp", g.nCurrentIp);
      printIp("camera.subnet", g.nCurrentSubNetMask);
      printIp("camera.gateway", g.nDefultGateWay);
      printIp("host.netExport", g.nNetExport);
    }

    tryOpen(dev, MV_ACCESS_Exclusive, "Exclusive");
    tryOpen(dev, MV_ACCESS_Control, "Control");
    tryOpen(dev, MV_ACCESS_Monitor, "Monitor");
  }

  return 0;
}
