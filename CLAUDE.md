# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```powershell
# Full build (configure, compile, deploy Qt dependencies)
.\scripts\build.ps1

# Configuration only (runs CMake with vcpkg toolchain)
.\scripts\configure.ps1

# Compile only (after configuration)
cmake --build build --config Release

# Run the application
.\build\WormVision.exe
```

**Build Configuration**: CMake 3.22+, Ninja generator, vcpkg through `VCPKG_ROOT` / `-VcpkgRoot`, C++17, pinned Qt 6.10, Release build only. Current setup instructions are in [README.md](README.md).

`build.ps1` 包含完整流程：自动从 MVS 安装目录同步 SDK DLL（含 ThirdParty）和导入库 → 加载 VS 环境 → configure → build → windeployqt → 拷 sqlite3.dll。新机器先按 README 安装 MSVC、vcpkg 和 MVS，再运行脚本；脚本不删除已有构建目录。

## Architecture Overview

WormVision-Qt is a high-performance industrial camera application using Hikvision MVS SDK for zero-copy rendering. The architecture separates hardware interaction, UI, and data management into distinct layers.

### Core Components

**CameraController** ([src/services/CameraController.h](src/services/CameraController.h)) - QObject（普通实例，不是单例；每个 CaptureWidget 创建一个）
- Wraps Hikvision MVS SDK for device enumeration, connection, and parameter control
- Runs camera grab loop on a worker thread (`m_grabThread`)
- Uses zero-copy rendering: `MV_CC_DisplayOneFrameEx2` renders directly to window handle via `VideoDisplayWidget`
- 录制时不支持的像素类型（如 Bayer）自动走 `MV_CC_ConvertPixelTypeEx` 转 BGR8
- 录制路径保留 `std::string m_recordingPath`（GBK，给 SDK）+ `QString m_recordingPathQt`（给 Qt，避免编码丢失）
- 录制 stop 后用 `QTimer` 等待文件大小稳定且 AVI 头可解析出时长，再解除录像忙状态。
- All SDK error codes logged in hex format (`0x%1`)
- Critical: Never block UI thread - all camera operations are async or worker-threaded

**VideoLibraryService** ([src/data/VideoLibraryService.h](src/data/VideoLibraryService.h)) - namespace
- 业务逻辑层，UI 不感兴趣的事情都在这里：`addRecording`、`pruneOrphans`
- 有完整单元测试覆盖

**AppPaths** ([src/utils/AppPaths.h](src/utils/AppPaths.h)) - namespace
- 用户数据目录管理：所有 recording/snapshot/db/log 都写 `%LOCALAPPDATA%\WormLab\WormVision\`
- 不要往 `Program Files` 写（受保护目录 + 海康 SDK C API 不走 Qt 的 VirtualStore）

**VideoDisplayWidget** ([src/widgets/VideoDisplayWidget.h](src/widgets/VideoDisplayWidget.h))
- Receives raw image data from CameraController and passes HWND to SDK for direct rendering
- Paint engine disabled (`paintEngine() = nullptr`) to prevent Qt from interfering with SDK rendering
- Supports zoom functionality via ScrollArea wrapper in CaptureWidget

**CaptureWidget** ([src/widgets/CaptureWidget.h](src/widgets/CaptureWidget.h))
- Main capture/recording interface that composes VideoDisplayWidget + ControlPanelWidget
- Manages video recording with built-in AVI encoding via SDK
- Handles zoom controls (+/-/fit) with scroll area for pan/zoom

**DatabaseManager** ([src/data/DatabaseManager.h](src/data/DatabaseManager.h)) - Singleton
- SQLite wrapper for video metadata, file tracking, and upload state

**ThemeManager** ([src/utils/ThemeManager.h](src/utils/ThemeManager.h)) - Singleton
- Dark/light theme switching applied at startup via `main.cpp`

## Critical Implementation Rules

### Zero-Copy Rendering (Mandatory)
The application's performance depends on SDK rendering directly to the widget's window handle.
- ❌ Never `memcpy` image data to `QImage` for display (causes CPU spikes)
- ✅ Use `MV_CC_DisplayOneFrameEx2` to render directly to window handle
- ✅ Disable Qt's paint engine: `paintEngine() const override { return nullptr; }`

### Hardware-as-Truth
Camera parameters are hardware-dependent and must be queried dynamically.
- ❌ Never hardcode parameter ranges (e.g., `Range(0, 10000)`)
- ✅ Query SDK using `MV_CC_GetFloatValue` to get actual ranges and initialize UI dynamically

### File Path Encoding
Windows C APIs in the SDK require local encoding, not UTF-8.
```cpp
// WRONG - SDK will fail to find files
std::string path = filePath.toStdString();

// CORRECT - use local 8-bit encoding
std::string path = filePath.toLocal8Bit().constData();
```

### Alignment Handling
Hardware buffers use stride padding; always use extended dimensions.
- ❌ Never use only `nWidth`/`nHeight` for buffer processing
- ✅ Always use `nExtendWidth`/`nExtendHeight` to handle hardware stride padding

### Thread Safety
- UI Thread: Never block with camera operations
- Worker Thread: Camera grab loop runs on `std::thread m_grabThread`
- Synchronization: `std::atomic<bool>` for flags, `std::mutex` for complex data
- Cross-thread: Use Qt signals with queued connections

## SDK Documentation Reference

All camera SDK development must reference official documentation in `Development/`:
- API manual: `Development/Documentations/工业相机Windows SDK开发指南V4.6.0（C）.chm`
- Samples: `Development/Samples/C++/General/`
- Parameters: `Development/Documentations/工业相机SDK可配置化参数表.xlsx`

## Code Style

- **Classes**: PascalCase, **Members**: `m_` prefix (camelCase), **Methods**: camelCase, **Constants**: UPPER_SNAKE_CASE
- **Includes**: Local (quotes) → Qt (angle brackets) → Standard (angle brackets), all alphabetically
- **Formatting**: 2-space indent, 100-char soft limit, K&R braces, `Type *name` (space before *)
- **Comments/Logs**: Chinese (Simplified) only
- **Singleton pattern**: Static `instance()` method, deleted copy constructor/assignment

## Testing

- **自动化（ctest）**：覆盖纯函数、数据库与视频库、应用路径与单实例、本地引擎服务及分析结果解析。具体目标见 `tests/CMakeLists.txt`，本次功能的定向测试命令见 README。

  跑法（build 完后在 `build/` 目录）：`ctest --output-on-failure`

- **手动（GUI + SDK + 硬件）**：[docs/SMOKE_TEST.md](docs/SMOKE_TEST.md)，发版前接相机走一遍 12 大项
