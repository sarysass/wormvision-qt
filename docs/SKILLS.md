# SKILLS.md

此文件为 AI Agent (如 Claude/Gemini) 在本项目工作时提供核心指导。

## 项目概览

WormVision Qt 是一个基于 C++/Qt 6 的桌面端线虫行为学实验视频采集与分析平台。它集成了海康威视 (Hikvision) 工业相机 SDK，对标工业级机器视觉软件架构，追求极致的性能（零拷贝渲染）和稳定性。

## 技术栈

- **语言**: C++17 (MSVC cl.exe)
- **GUI 框架**: Qt 6.8 (Widgets Module)
- **相机 SDK**: Hikvision MVS SDK (MvCameraControl.dll, 位于 `Development/`)
- **数据库**: SQLite 3 (via QtSql)
- **构建系统**: CMake 3.31 + Ninja
- **包管理器**: vcpkg (manifest mode)
- **脚本**: PowerShell 7

## 常用命令

### 开发与构建
```powershell
./scripts/build.ps1           # 完整构建流程 (配置 + 编译 + 部署 Qt 依赖)
```

该脚本会自动检测 Visual Studio 环境，配置 CMake (使用 vcpkg 工具链)，编译 Release 版本，并运行 `windeployqt` 部署所需 DLL。

## 🧠 核心知识库 (Development 目录)

**CRITICAL: `Development` 文件夹是本项目唯一的真理来源 (Single Source of Truth)。**

在开发任何涉及相机控制、图像采集、回调处理或录像的功能时，**严禁猜测** API 用法。必须严格遵循以下查阅流程：

### 1. 理论查询 (Documentations)
遇到参数含义不清、错误码未知或需要查找功能 Key 名称时：
- **API 字典**: `Development/Documentations/工业相机Windows SDK开发指南V4.6.0（C）.chm`
  - *用途*: 查阅 `MV_CC_SaveImageToFileEx2` 等函数原型、结构体定义及错误码含义。
- **参数映射表**: `Development/Documentations/工业相机SDK可配置化参数表.xlsx`
  - *用途*: 查找控制特定功能（如曝光、白平衡、触发延迟）的字符串 Key Name (如 "ExposureTime") 及其数据类型/范围。

### 2. 代码参考 (Samples)
实现复杂功能时，**必须**直接复制并参考官方 C++ 示例代码，**以官方实现为准**：
- **录像功能**: 参考 `Development/Samples/C++/General/Recording`
  - *关注点*: 如何正确填充 `MV_CC_RECORD_PARAM`，特别是 `nFrameLenEx` 的使用。
- **抓拍/截图**: 参考 `Development/Samples/C++/General/ImageSave`
  - *关注点*: 使用 `MV_CC_SaveImageToFileEx2` 接口，以及 `nExtendWidth` 的对齐处理。
- **高性能渲染**: 参考 `Development/Samples/C++/General/GrabImage_Display`
  - *关注点*: 使用 `MV_CC_DisplayOneFrameEx2` 直接传递窗口句柄实现零拷贝。

### 3. 连接与编译 (Library)
- **头文件**: `Development/Includes`
- **静态库**: `Development/Libraries/win64`

## 架构说明

### 目录结构
```
src/
  main.cpp                    # 入口
  services/                   # 业务逻辑层 (无 UI)
    CameraController.cpp      # 相机控制核心 (单例)
  widgets/                    # UI 视图层
    VideoDisplayWidget.cpp    # 绘图区域 (重写 PaintEngine 禁用 Qt 绘制)
    CaptureWidget.cpp         # 采集主界面
    ControlPanelWidget.cpp    # 参数调整面板
  data/                       # 数据层
    DatabaseManager.cpp       # SQLite 封装
scripts/
  build.ps1                   # 一键构建脚本
Development/                  # 海康 SDK 完整资源包 (知识库)
```

### 关键模式 (Key Patterns)

**1. 硬件即真理 (Hardware is Source of Truth)**
❌ **禁止**: 在代码中硬编码参数范围 (如 `Range(0, 10000)`)。
✅ **必须**: 打开相机后，调用 `MV_CC_GetFloatValue` 查询 SDK 返回的 `Min` / `Max` / `Cur`，并据此初始化 UI 控件。

**2. 零拷贝渲染 (Zero-Copy Rendering)**
❌ **禁止**: `memcpy` 图像数据到 `QImage` 再用 `QPainter` 绘制（除非做算法分析）。
✅ **必须**: 使用 `MV_CC_DisplayOneFrameEx2` 将图像直接通过 DMA 传输到 `VideoDisplayWidget` 的 `HWND`。

**3. 路径安全 (Path Safety)**
❌ **禁止**: 使用 `.toStdString()` (UTF-8) 传递文件路径给 SDK。
✅ **必须**: Windows C API 要求 ANSI/GBK 编码，**必须**使用 `.toLocal8Bit()`。
```cpp
std::string path = filePath.toLocal8Bit().constData(); // 正确
```

**4. 尺寸对齐 (Alignment)**
❌ **禁止**: 仅使用 `nWidth` / `nHeight` 处理图像 buffer。
✅ **必须**: 始终读取并使用 `nExtendWidth` / `nExtendHeight` 以处理硬件的 stride padding，防止图像歪斜。

## 数据库架构

- **videos 表**: 存储录制的视频元数据。
  - `filepath`: 唯一索引，用于去重。
  - `duration`: 录制时长（秒）。
  - `upload_status`: 标记云端同步状态。

## 重要约定

- **语言**: UI 显示、日志 (`qDebug`) 、说明文档及代码注释全部使用 **简体中文**。
- **RAII**: `CameraController` 必须确保在析构中调用 `CloseDevice` 和 `DestroyHandle`。
- **线程模型**:
  - **UI 线程**: 处理点击事件、更新进度条。严禁阻塞。
  - **采集线程**: 独占一个 `std::thread` 运行 `GrabLoop`，负责取流、渲染、录制帧输入。
- **Git 提交**: 提交前确保 `.\scripts\build.ps1` 运行通过。

## 关键文件参考

| 用途 | 文件路径 |
|------|---------|
| **知识库-API手册** | `Development/Documentations/*.chm` |
| **知识库-示例代码** | `Development/Samples/C++/General/` |
| 相机控制服务 | `src/services/CameraController.cpp` |
| 视频显示组件 | `src/widgets/VideoDisplayWidget.cpp` |
| CMake 配置 | `CMakeLists.txt` |
