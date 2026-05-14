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

**Build Configuration**: CMake 3.20+, Ninja generator, vcpkg toolchain at `C:/vcpkg/scripts/buildsystems/vcpkg.cmake`, C++17, Qt 6.x (实际 vcpkg 装 6.10), Release build only.

`build.ps1` 包含完整流程：自动从 MVS 安装目录同步 SDK DLL（含 ThirdParty）→ 加载 VS 环境 → configure → build → windeployqt → 拷 sqlite3.dll。新机器 git clone 后直接跑 `build.ps1` 就能完整产出。

## Architecture Overview

WormVision-Qt is a high-performance industrial camera application using Hikvision MVS SDK for zero-copy rendering. The architecture separates hardware interaction, UI, and data management into distinct layers.

### Core Components

**CameraController** ([src/services/CameraController.h](src/services/CameraController.h)) - QObject（普通实例，不是单例；每个 CaptureWidget 创建一个）
- Wraps Hikvision MVS SDK for device enumeration, connection, and parameter control
- Runs camera grab loop on a worker thread (`m_grabThread`)
- Uses zero-copy rendering: `MV_CC_DisplayOneFrameEx2` renders directly to window handle via `VideoDisplayWidget`
- 录制时不支持的像素类型（如 Bayer）自动走 `MV_CC_ConvertPixelTypeEx` 转 BGR8
- 录制路径保留 `std::string m_recordingPath`（GBK，给 SDK）+ `QString m_recordingPathQt`（给 Qt，避免编码丢失）
- 录制 stop 后用 `QTimer` 轮询文件大小直到 > 0（SDK 异步 flush AVI 索引）
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

- **自动化（ctest）**：`tests/` 下 5 个 Qt Test suite，51 个 case，覆盖纯函数 + DB CRUD + service 层：
  - `test_smoke` 1 case：框架自检
  - `test_video_utils` 17 case：formatDuration、AVI/MP4 时长解析
  - `test_database_manager` 13 case：CRUD、UNIQUE 冲突、upsert
  - `test_recording_diagnostics` 10 case：像素白名单、统计格式化
  - `test_video_library_service` 10 case：录制入库、脏数据清理、retry 场景

  跑法（build 完后在 `build/` 目录）：`ctest --output-on-failure`

- **手动（GUI + SDK + 硬件）**：[docs/SMOKE_TEST.md](docs/SMOKE_TEST.md)，发版前接相机走一遍 12 大项
