# WormVision-Qt DLL 依赖列表

> 本文档列出了 WormVision-Qt 项目**实际使用**的 DLL 依赖，基于源代码分析和项目配置。

---

## 📊 使用情况总结

| 类别 | 总数 | 实际使用 | 未使用 | 说明 |
|------|------|---------|--------|------|
| **Qt6 核心** | 11 | 5 | 6 | 仅使用 CMakeLists.txt 指定的 5 个模块 |
| **海康 SDK** | 40+ | ~35 | ~5 | SDK 内部依赖复杂，建议全部保留 |
| **vcpkg 第三方** | 70+ | ~30 | ~40 | OpenCV、PostgreSQL 等未被使用 |

---

## 1. Qt6 核心 DLL（实际使用：5 个）

### ✅ 必需模块
| DLL 名称 | 用途 | 代码引用 |
|---------|------|---------|
| **Qt6Core.dll** | 核心功能（QObject、QString、QTimer） | 全部源文件 |
| **Qt6Gui.dll** | GUI 基础（QPalette、QImage） | VideoDisplayWidget |
| **Qt6Widgets.dll** | UI 组件（QWidget、QPushButton） | 全部 Widget |
| **Qt6Sql.dll** | SQLite 数据库 | DatabaseManager |
| **Qt6Network.dll** | 网络通信 | CloudService |

### ❌ 未使用（vcpkg 可能自动部署）
| DLL 名称 | 原因 |
|---------|------|
| Qt6Concurrent.dll | 项目未使用 QtConcurrent |
| Qt6DBus.dll | Windows 平台不需要 |
| Qt6OpenGL.dll | 渲染由海康 SDK 的 D3D 处理 |
| Qt6OpenGLWidgets.dll | 未使用 OpenGL Widget |
| Qt6PrintSupport.dll | 无打印功能 |
| Qt6Test.dll | 仅测试用 |
| Qt6Xml.dll | 未使用 QXmlStreamReader |

---

## 2. 海康威视 SDK DLL（必需：~35 个）

> ⚠️ **重要**：海康 SDK 内部依赖关系复杂，**建议全部部署**，即使部分 DLL 看似未直接使用。

### ✅ 核心运行时（绝对必需）

| DLL 名称 | 用途 | 代码调用 |
|---------|------|---------|
| **MvCameraControl.dll** | SDK 主库 | `#include <MvCameraControl.h>` |
| **MvRender.dll** | 零拷贝渲染 | `MV_CC_DisplayOneFrameEx2()` |
| **SuperRender.dll** | 硬件加速渲染 | 渲染引擎依赖 |
| **MVGigEVisionSDK.dll** | GigE 千兆网相机 | `MV_GIGE_DEVICE` |
| **MvUsb3vTL.dll** | USB3.0 相机 | `MV_USB_DEVICE` |
| **MvCamLVision.dll** | 相机链接层 | SDK 内部调用 |
| **MediaProcess.dll** | AVI 视频编码 | `MV_CC_StartRecord()` |
| **FormatConversion.dll** | 图像格式转换 | `MV_CC_SaveImageToFileEx2()` |

### ✅ GenICam 标准 API（必需）

| DLL 名称 | 用途 |
|---------|------|
| GenApi_MD_VC120_v3_0_MV.dll | 参数节点访问（`MV_CC_GetFloatValue` 等） |
| GCBase_MD_VC120_v3_0_MV.dll | GenICam 基础类 |
| MathParser_MD_VC120_v3_0_MV.dll | 参数表达式解析 |
| log4cpp_MD_VC120_v3_0_MV.dll | SDK 日志 |
| NodeMapData_MD_VC120_v3_0_MV.dll | 相机参数节点映射 |
| XmlParser_MD_VC120_v3_0_MV.dll | 相机描述文件解析 |

### ✅ 相机功能控制（必需）

| DLL 名称 | 用途 | 代码调用 |
|---------|------|---------|
| **MvISPControl.dll** | ISP 图像信号处理 | 参数调整 |
| **MvCameraPatch.dll** | 相机固件热更新 | SDK 内部 |
| **MvCameraControlGUI.dll** | 参数控制面板 | 虽未直接调用，但 SDK 内部依赖 |
| **MvCameraControlWrapper.dll** | SDK 封装层 | SDK 内部 |
| **MVMemAlloc.dll** | 内存管理 | SDK 内部 |
| **MvSDKVersion.dll** | SDK 版本查询 | SDK 内部 |

### ✅ 串口/CL 协议栈（串口相机需要）

如果使用**串口相机**则需要以下 DLL：

| DLL 名称 | 用途 |
|---------|------|
| CLSerCOM.dll | 串口通信 |
| CLSerHvc.dll | 硬件控制器 |
| CLAllSerial_MD_VC120_v3_0_MV.dll | 全功能串口库 |
| CLProtocol_MD_VC120_v3_0_MV.dll | 串口协议 |
| Log_MD_VC120_v3_0_MV.dll | 串口日志 |
| MvSerial.dll | 串口接口 |
| MvSerialCtrl.dll | 串口控制 |
| MVFGControl.dll | FG 控制接口 |

> **注意**：如仅使用 GigE/USB 相机，这些 DLL 可能非必需，但建议保留。

### ✅ 视频生产器（录制功能）

| DLL 名称 | 用途 |
|---------|------|
| MvProducerVIR.dll | VIR 生产器 |
| MvLCProducer.dll | LiveCtrl 生产器 |

### ✅ ThirdParty 第三方库

| DLL 名称 | 用途 | 位置 |
|---------|------|------|
| **swscale-9.dll** | FFmpeg 缩放（视频录制） | ThirdParty/ |
| **avutil-60.dll** | FFmpeg 工具库 | ThirdParty/ |
| **libwinpthread-1.dll** | MinGW 线程库 | ThirdParty/ |

---

## 3. Visual C++ 运行时 DLL（必需）

| DLL 名称 | 描述 | 使用方 |
|---------|------|--------|
| **msvcr90.dll** | MSVC 2008 运行时 | 海康 SDK |
| **msvcp90.dll** | MSVC 2008 C++ 标准 | 海康 SDK |
| **msvcm90.dll** | MSVC 2008 混合模式 | 海康 SDK |
| **msvcr100.dll** | MSVC 2010 运行时 | 海康 SDK |
| **msvcr120.dll** | MSVC 2013 运行时 | 海康 SDK |
| **msvcp120.dll** | MSVC 2013 C++ 标准 | 海康 SDK |

> **替代方案**：安装 [VC++ Redistributable AIO](https://github.com/abbodi1406/vcredist) 可替代这些 DLL。

---

## 4. Intel MKL / 线程库（必需）

| DLL 名称 | 描述 | 使用方 |
|---------|------|--------|
| **libmmd.dll** | Intel 数学核心库 | 海康 SDK |
| **svml_dispmd.dll** | 短向量数学库 | 海康 SDK |

---

## 5. 线程与硬件库（必需）

| DLL 名称 | 描述 | 使用方 |
|---------|------|--------|
| **pthreadGC2.dll** | POSIX 线程（GCC） | FFmpeg/ThirdParty |
| **pthreadVC2.dll** | POSIX 线程（MSVC） | FFmpeg/ThirdParty |
| **libusb0.dll** | USB 设备访问 | USB 相机 |

---

## 6. DirectX 渲染（推荐）

| DLL 名称 | 描述 | 用途 |
|---------|------|------|
| **D3DCompiler_43.dll** | D3D 编译器 | 硬件加速渲染 |

> **可选**：缺失时软件渲染仍可工作，但性能较低。

---

## 7. Qt 依赖的第三方库（实际使用：~30 个）

### ✅ 图像/字体（必需）

| DLL 名称 | 用途 | Qt 模块 |
|---------|------|---------|
| freetype.dll | 字体渲染 | Qt6Gui |
| harfbuzz.dll | 文本整形 | Qt6Gui |
| harfbuzz-subset.dll | HarfBuzz 子集 | Qt6Gui |
| jpeg62.dll | JPEG 解码 | Qt6Gui |
| libpng16.dll | PNG 解码 | Qt6Gui |
| libsharpyuv.dll | YUV 转换 | Qt6Gui |
| libwebp.dll | WebP 支持 | Qt6Gui |
| libwebpdecoder.dll | WebP 解码 | Qt6Gui |
| libwebpdemux.dll | WebP 多路复用 | Qt6Gui |
| libwebpmux.dll | WebP 编码 | Qt6Gui |
| tiff.dll | TIFF 支持 | Qt6Gui |
| turbojpeg.dll | JPEG 加速 | Qt6Gui |

### ✅ 压缩/编码（必需）

| DLL 名称 | 用途 | Qt 模块 |
|---------|------|---------|
| zlib1.dll | Zlib 压缩 | Qt6Core |
| bz2.dll | Bzip2 压缩 | Qt6Core |
| lz4.dll | LZ4 压缩 | Qt6Core |
| zstd.dll | Zstandard 压缩 | Qt6Core |
| liblzma.dll | XZ 压缩 | Qt6Core |
| brotlicommon.dll | Brotli 通用 | Qt6Network |
| brotlidec.dll | Brotli 解码 | Qt6Network |
| brotlienc.dll | Brotli 编码 | Qt6Network |

### ✅ 国际化（必需）

| DLL 名称 | 用途 | Qt 模块 |
|---------|------|---------|
| icudt78.dll | ICU 数据 | Qt6Core |
| icuin78.dll | ICU 国际化 | Qt6Core |
| icuio78.dll | ICU I/O | Qt6Core |
| icutu78.dll | ICU 工具 | Qt6Core |
| icuuc78.dll | ICU 通用 | Qt6Core |

### ✅ 网络与安全（云服务需要）

| DLL 名称 | 用途 | Qt 模块 |
|---------|------|---------|
| **libssl-3-x64.dll** | OpenSSL 3 TLS | Qt6Network |
| **libcrypto-3-x64.dll** | OpenSSL 3 加密 | Qt6Network |

> **注意**：如果未实现云上传功能，OpenSSL 可选。

### ✅ 正则表达式（必需）

| DLL 名称 | 用途 | Qt 模块 |
|---------|------|---------|
| pcre2-8.dll | PCRE2 (UTF-8) | Qt6Core |
| pcre2-16.dll | PCRE2 (UTF-16) | Qt6Core |
| pcre2-32.dll | PCRE2 (UTF-32) | Qt6Core |
| pcre2-posix.dll | PCRE2 POSIX | Qt6Core |

### ✅ 其他（必需）

| DLL 名称 | 用途 | Qt 模块 |
|---------|------|---------|
| double-conversion.dll | 双精度转换 | Qt6Core |
| libexpat.dll | XML 解析 | Qt6Core |
| sqlite3.dll | SQLite 数据库 | Qt6Sql |

### ❌ 未使用（可忽略）

| DLL 名称 | 原因 |
|---------|------|
| dbus-1-3.dll | Windows 平台不需要 |
| md4c.dll | Markdown 解析（Qt 未使用） |
| md4c-html.dll | Markdown HTML（Qt 未使用） |
| **所有 opencv_*.dll** | 项目未使用 OpenCV |
| **所有 libpq\*.dll** | 未使用 PostgreSQL |
| **libprotobuf\*.dll** | OpenCV 依赖 |
| abseil_dll.dll | 未使用 Abseil |
| legacy.dll | 未知用途（可能 OpenCV 依赖） |
| pkgconf-7.dll | 构建工具运行时（部署不需要） |

---

## 8. 实际部署清单

### 🚀 最小化部署（仅运行 WormVision.exe）

**目录结构：**
```
WormVision.exe
├── Qt6Core.dll
├── Qt6Gui.dll
├── Qt6Widgets.dll
├── Qt6Sql.dll
├── Qt6Network.dll
├── sqlite3.dll
├── libssl-3-x64.dll (如使用云服务)
├── libcrypto-3-x64.dll (如使用云服务)
├── platforms/
│   └── qwindows.dll
├── styles/
│   └── qwindowsvistastyle.dll
├── imageformats/
│   ├── qjpeg.dll
│   ├── qico.dll
│   └── qsvg.dll
├── [所有 ICU DLL: icudt78.dll, icuin78.dll, icuio78.dll, icutu78.dll, icuuc78.dll]
├── [所有 pcre2 DLL]
├── [所有图像格式 DLL: freetype.dll, harfbuzz*.dll, jpeg62.dll, libpng16.dll, ...]
├── [所有压缩 DLL: zlib1.dll, bz2.dll, lz4.dll, zstd.dll, liblzma.dll]
├── [所有 brotli DLL]
├── libs/
│   └── hikvision/
│       └── bin/
│           └── [全部复制，约 40 个 DLL]
```

### 📋 自动部署脚本

```cmake
# CMakeLists.txt 已配置自动复制
# 运行 cmake --build build --config Release 后
# 所有 DLL 会自动复制到 build/Release/ 目录
```

### 🔧 手动部署工具

使用 `windeployqt.exe` 自动部署 Qt DLL：

```powershell
# 安装 Qt 后执行
windeployqt.exe --release --no-translations build\WormVision.exe

# 然后手动复制 libs/hikvision/bin/ 下的所有 DLL
```

---

## 9. 功能缺失对照表

| 功能 | 缺失 DLL | 症状 |
|------|---------|------|
| 应用启动 | Qt6Core.dll | "无法启动此程序，因为计算机中丢失 Qt6Core.dll" |
| UI 显示 | Qt6Widgets.dll | 窗口无法创建 |
| 数据库 | Qt6Sql.dll | 视频库无法加载 |
| 相机连接 | MvCameraControl.dll | SDK 初始化失败 |
| 视频预览 | MvRender.dll | 黑屏，无图像 |
| 视频录制 | MediaProcess.dll | `MV_CC_StartRecord` 失败 |
| 抓拍保存 | FormatConversion.dll | `MV_CC_SaveImageToFileEx2` 失败 |
| 云上传 | libssl-3-x64.dll | HTTPS 请求失败 |
| 中文显示 | icu*.dll | 日期/时间乱码 |
| JPEG 保存 | jpeg62.dll | 无法保存 JPEG |

---

## 10. 验证工具

### Dependencies GUI（推荐）
```powershell
# 下载：https://github.com/lucasg/Dependencies/releases
Dependencies.exe -chain build\WormVision.exe
```

### dumpbin（Visual Studio）
```cmd
dumpbin /DEPENDENTS build\WormVision.exe
```

---

**生成时间**: 2026-01-20
**项目版本**: WormVision 1.0.0
**Qt 版本**: 6.8 (vcpkg)
**平台**: Windows x64
**编译器**: MSVC 14.44 (VS2022)
**分析方式**: 源代码静态分析 + CMake 配置
