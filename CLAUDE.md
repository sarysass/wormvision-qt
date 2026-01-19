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

**Build Configuration**: CMake 3.20+, Ninja generator, vcpkg toolchain at `C:/vcpkg/scripts/buildsystems/vcpkg.cmake`, C++17, Qt 6.8, Release build only.

## Architecture Overview

WormVision-Qt is a high-performance industrial camera application using Hikvision MVS SDK for zero-copy rendering. The architecture separates hardware interaction, UI, and data management into distinct layers.

### Core Components

**CameraController** ([src/services/CameraController.h](src/services/CameraController.h)) - Singleton
- Wraps Hikvision MVS SDK for device enumeration, connection, and parameter control
- Runs camera grab loop on a worker thread (`m_grabThread`)
- Uses zero-copy rendering: `MV_CC_DisplayOneFrameEx` renders directly to window handle via `VideoDisplayWidget`
- All SDK error codes logged in hex format (`0x%1`)
- Critical: Never block UI thread - all camera operations are async or worker-threaded

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

Currently manual only. No automated testing framework configured. Test by running `.\build\WormVision.exe` and verifying:
1. Camera connection and preview
2. Video recording with AVI encoding
3. Parameter range validation
4. Database operations
