# AGENTS.md

This file provides guidance for agentic coding assistants working on the WormVision-Qt project.

## Project Overview

WormVision-Qt is a Qt6 C++17 desktop application for industrial camera capture and video management using Hikvision MVS SDK. The project focuses on high-performance zero-copy rendering and stability for worm behavior analysis experiments.

## Build Commands

### Full Build
```powershell
.\scripts\build.ps1
```
Configures CMake with vcpkg toolchain, compiles Release build with Ninja, and deploys Qt dependencies using windeployqt.

### Manual Configuration
```powershell
.\scripts\configure.ps1
```
Runs CMake configuration only (cleans build directory first).

### Build Only
```powershell
cmake --build build --config Release
```
Requires prior CMake configuration.

### Run
```powershell
.\build\WormVision.exe
```

### Build Configuration
- **CMake**: 3.31+
- **Generator**: Ninja
- **Toolchain**: vcpkg (`C:/vcpkg/scripts/buildsystems/vcpkg.cmake`)
- **Build Type**: Release
- **C++ Standard**: C++17
- **Qt**: 6.8 (via vcpkg)
- **Visual Studio**: MSVC with x64 architecture

## Code Style Guidelines

### Naming Conventions

**Classes**: PascalCase
```cpp
class CameraController : public QObject { ... };
class VideoDisplayWidget : public QWidget { ... };
```

**Member Variables**: `m_` prefix (camelCase)
```cpp
void *m_cameraHandle = nullptr;
bool m_isOpen = false;
QList<DeviceInfo> m_devices;
```

**Methods**: camelCase
```cpp
bool open(int deviceIndex);
void stopGrabbing();
void setupUI();
```

**Constants**: UPPER_SNAKE_CASE
```cpp
static constexpr float EMA_ALPHA = 0.1f;
#define MV_OK 0
```

**Enums/Structs**: PascalCase
```cpp
enum SnapshotFormat { FORMAT_BMP, FORMAT_JPEG, FORMAT_PNG };
struct DeviceInfo { QString name; QString serialNumber; };
```

**Signals**: camelCase
```cpp
signals:
void errorOccurred(const QString &msg);
void uploadProgress(const QString &filePath, int percent);
```

### Includes Order

1. Local project includes (quotes) - alphabetically
2. Qt includes (angle brackets) - alphabetically
3. Standard library includes (angle brackets) - alphabetically

```cpp
#include "CameraController.h"
#include "VideoDisplayWidget.h"

#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>

#include <atomic>
#include <memory>
#include <mutex>
```

### Formatting

- **Indentation**: 2 spaces (tabs not allowed)
- **Max line width**: 100 characters (soft limit)
- **Brace style**: K&R - opening brace on same line
- **Pointer alignment**: `Type *name` (space before *)
- **Reference alignment**: `Type &name` (space before &)

### Types and Type Safety

- Use Qt types for Qt objects: `QString`, `QList`, `QVector`
- Use `std::atomic` for thread-safe primitives
- Use `std::mutex` with `std::lock_guard` or RAII patterns
- Use `nullptr` instead of `NULL`
- Use `auto` sparingly - prefer explicit types for API clarity
- Use `enum class` or scoped enums where appropriate
- Use `static constexpr` for compile-time constants

### Error Handling

**SDK Error Codes**:
```cpp
int ret = MV_CC_SetFloatValue(m_cameraHandle, "ExposureTime", value);
if (ret != MV_OK) {
    emit error(QString("设置曝光失败: 0x%1").arg(ret, 8, 16, QChar('0')));
    return false;
}
```

**Database Errors**:
```cpp
if (!query.exec()) {
    emit databaseError(query.lastError().text());
    return false;
}
```

**Logging**:
- `qDebug()` for informational messages
- `qWarning()` for non-critical errors
- `qCritical()` for critical errors
- All logs in Chinese (Simplified)

**Return Values**:
- `bool` for success/failure operations
- Return `-1` or empty containers for invalid data
- Use signals for async error notifications to UI

### Memory Management

**Resource Acquisition**:
- Use RAII pattern for all resources
- Ensure cleanup in destructors
- Use Qt parent-child relationships for widgets
- Use `QScopedPointer` or `std::unique_ptr` where ownership is clear

```cpp
class CameraController {
public:
    ~CameraController() { close(); }  // RAII cleanup
    
private:
    void close() {
        if (m_isOpen) {
            MV_CC_CloseDevice(m_cameraHandle);
            MV_CC_DestroyHandle(m_cameraHandle);
        }
    }
};
```

### Qt-Specific Patterns

**Signal/Slot Connections**:
- Prefer modern syntax with lambdas for simple cases
- Use function pointer syntax for type safety
- Explicitly manage connection type (queued for cross-thread)

```cpp
connect(m_camera, &CameraController::errorOccurred,
        this, &MainWindow::handleError);
connect(button, &QPushButton::clicked, [this]() { doSomething(); });
```

**Singleton Pattern**:
```cpp
class DatabaseManager {
public:
    static DatabaseManager &instance() {
        static DatabaseManager instance;
        return instance;
    }
    // Prevent copying
    DatabaseManager(const DatabaseManager &) = delete;
    DatabaseManager &operator=(const DatabaseManager &) = delete;
private:
    DatabaseManager() = default;
    ~DatabaseManager() = default;
};
```

**Q_OBJECT Macro**: Required for all classes with signals/slots or meta-object features

### Threading Model

**UI Thread**: Handles all user interaction, widget updates, and event processing. Never block.

**Worker Threads**: Use `std::thread` for CPU-intensive or blocking operations
```cpp
std::thread m_grabThread;
std::atomic<bool> m_stopGrabbing{false};

void CameraController::startGrabbing() {
    m_stopGrabbing = false;
    m_isGrabbing = true;
    m_grabThread = std::thread(&CameraController::grabLoop, this);
}
```

**Thread Safety**:
- Use `std::atomic` for simple flags and counters
- Use `std::mutex` for complex data structures
- Use Qt signals for cross-thread communication (queued connections)
- Never access widgets from non-UI threads

### File Path Handling

**CRITICAL**: Windows C APIs require ANSI/GBK encoding, not UTF-8
```cpp
// WRONG - SDK will fail to find files
std::string path = filePath.toStdString();

// CORRECT - use local 8-bit encoding
std::string path = filePath.toLocal8Bit().constData();
```

### Code Comments

- All comments and documentation in **Chinese (Simplified)**
- Use `/** */` for class/function documentation
- Use `//` for inline comments
- Document public APIs with parameter descriptions

```cpp
/**
 * @brief 打开指定索引的相机设备
 * @param deviceIndex 设备索引 (0-based)
 * @return true 成功打开, false 失败 (触发 error 信号)
 */
bool open(int deviceIndex = 0);
```

## Critical Development Rules

### Hardware-as-Truth
❌ Never hardcode parameter ranges (e.g., `Range(0, 10000)`)
✅ Query SDK for actual ranges using `MV_CC_GetFloatValue` and initialize UI dynamically

### Zero-Copy Rendering
❌ Never `memcpy` image data to `QImage` for display (causes CPU spike)
✅ Use `MV_CC_DisplayOneFrameEx2` to render directly to window handle

### Alignment Handling
❌ Never use only `nWidth`/`nHeight` for buffer processing
✅ Always use `nExtendWidth`/`nExtendHeight` to handle hardware stride padding

### SDK Documentation Reference
All camera SDK development must reference the official documentation in `Development/`:
- API manual: `Development/Documentations/工业相机Windows SDK开发指南V4.6.0（C）.chm`
- Samples: `Development/Samples/C++/General/`
- Parameters: `Development/Documentations/工业相机SDK可配置化参数表.xlsx`

## Project Structure

```
src/
├── main.cpp                    # Application entry point
├── mainwindow.h/.cpp           # Main window with view switching
├── services/
│   ├── CameraController.h/.cpp # Camera SDK wrapper (singleton)
│   └── CloudService.h/.cpp     # Cloud API integration (singleton)
├── widgets/
│   ├── VideoDisplayWidget.h/.cpp    # Zero-copy video display
│   ├── CaptureWidget.h/.cpp         # Capture/recording UI
│   ├── ControlPanelWidget.h/.cpp    # Parameter controls
│   └── VideoLibraryWidget.h/.cpp    # Video management UI
├── data/
│   └── DatabaseManager.h/.cpp  # SQLite database wrapper (singleton)
└── utils/
    └── ThemeManager.h/.cpp     # Theme switching (singleton)

tests/                          # Test files (currently empty)
scripts/
├── build.ps1                   # Full build script
└── configure.ps1               # CMake configuration only

Development/                    # Hikvision SDK documentation (SOURCE OF TRUTH)
├── Documentations/
├── Samples/
├── Includes/
└── Libraries/
```

## Testing

Currently no automated testing framework is configured. Manual testing is performed by:
1. Running `.\build\WormVision.exe`
2. Testing camera connection and preview
3. Testing video recording and playback
4. Verifying database operations

When adding tests, create test files in `tests/` directory and update CMakeLists.txt accordingly.

## Git Workflow

- Ensure `.\scripts\build.ps1` runs successfully before committing
- Test all modified functionality manually
- Commit messages should be concise and in Chinese
- Reference relevant issues or documentation in commit messages

## Dependencies

- Qt 6.8 (Widgets, Sql, Network modules)
- Hikvision MVS SDK (MvCameraControl.dll)
- SQLite 3
- vcpkg for package management
- CMake 3.31+, Ninja
- Visual Studio with MSVC (x64)
