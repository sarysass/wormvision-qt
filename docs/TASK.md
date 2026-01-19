# WormVision Qt 重写 - Week 1 任务

## 第一步：环境搭建与核心渲染

- [x] Qt 6.10 环境安装与配置 (vcpkg)
  - [x] 通过 vcpkg 安装 Qt 6.10 (29/29 全部完成)
  - [x] 验证安装完成

- [x] 创建基础项目结构
  - [x] 在 `d:\projects\wormvision-qt` 创建新项目目录
  - [x] 配置 CMakeLists.txt
  - [x] 创建目录结构 (src/, resources/, libs/)

- [x] 集成海康威视 SDK
  - [x] 复制 SDK DLL 到项目 libs 目录
  - [x] 获取 SDK 头文件
  - [x] 配置 CMake 链接设置

- [x] 实现 OpenGL 渲染组件
  - [x] 创建 `VideoDisplayWidget` 类
  - [x] 实现 OpenGL 纹理渲染
  - [x] 实现测试图片循环播放功能

- [ ] 验证
  - [ ] CMake 配置成功
  - [ ] 编译成功
  - [ ] 运行测试图片循环播放 DEMO

## 录像性能优化

- [x] **使用 SDK 内置录像 API 替代 OpenCV VideoWriter**
  - [x] 分析海康 MVS 软件低 CPU 占用原因
  - [x] 发现 SDK 提供 `MV_CC_StartRecord` / `MV_CC_InputOneFrame` / `MV_CC_StopRecord` API
  - [x] 在 `CameraController` 中实现 SDK 录像功能
    - [x] `startRecording(filePath, fps, bitRateKbps)` 方法
    - [x] `stopRecording()` 方法
    - [x] 在 `grabLoop` 中调用 `MV_CC_InputOneFrame` 发送原始帧
  - [x] 更新 `CaptureWidget` 使用新的 SDK 录像
    - [x] 移除 `VideoRecorder` 依赖
    - [x] 文件格式从 MP4 改为 AVI (SDK 仅支持 AVI)
  - [x] 编译验证通过

**优化效果预期：**
- CPU 占用: ~50% → ~10% (原始帧直接送入 SDK 编码器，无需颜色转换)
- 文件大小: 可控码率 (默认 4000 kbps)
- 编码器: SDK 内置优化编码


## 云平台集成 (wormstudio.cloud)

### 登录功能 - Mode B 设备授权流程

**用户操作流程：**
1. 用户在浏览器登录 wormstudio.cloud
2. 用户在"账户设置"页面点击"生成设备码" → 显示 `ABCD-1234`（有效期5分钟）
3. 用户在 WormVision 客户端点击"登录" → 输入设备码
4. 客户端调用 `/api/auth/device-verify` 验证 → 返回 Session Token
5. 登录完成，Token 保存本地

**实现任务：**

- [ ] **云平台端 (wormstudio-cloud-nextjs)**
  - [ ] 添加"生成设备码"页面 (`/settings/device-code`)
  - [ ] 新增 API: `POST /api/auth/device-code` 生成设备码
  - [ ] 新增 API: `POST /api/auth/device-verify` 验证设备码并返回 Token

- [ ] **客户端 (wormvision-qt)**
  - [ ] 新增 `AuthService` 类 - Token 管理、登录状态
  - [ ] 新增 `LoginDialog` 对话框 - 输入设备码
  - [ ] 主窗口添加登录/登出菜单项
  - [ ] 保存 Token 到本地 (QSettings 加密存储)

### 视频上传功能

- [ ] **客户端上传实现**
  - [ ] 新增 `CloudUploader` 类 - S3 分块上传
  - [ ] 实现 SHA256 校验和计算
  - [ ] 实现进度上报
  - [ ] 修改 `onBatchUploadClicked()` 调用上传逻辑

- [ ] **UI 集成**
  - [ ] 上传进度对话框
  - [ ] 视频库显示上传状态
  - [ ] 上传完成通知

### API 设计

```
# 云平台新增 API

POST /api/auth/device-code
Request: { }  (需要已登录)
Response: { code: "ABCD-1234", expiresAt: "2025-01-17T17:00:00Z" }

POST /api/auth/device-verify  
Request: { code: "ABCD-1234" }
Response: { token: "jwt...", user: { id, email, name } }
```

## 编译指南

### 1. 配置项目
```powershell
cd d:\projects\wormvision-qt
"C:\vcpkg\downloads\tools\cmake-3.31.10-windows\cmake-3.31.10-windows-x86_64\bin\cmake.exe" -B build -S . -G Ninja -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="C:\vcpkg\downloads\tools\ninja-1.12.1-windows\ninja.exe"
```

### 2. 编译项目
```powershell
"C:\vcpkg\downloads\tools\cmake-3.31.10-windows\cmake-3.31.10-windows-x86_64\bin\cmake.exe" --build build
```

### 3. 运行
```powershell
.\build\WormVision.exe
```

## 项目结构
```
wormvision-qt/
├── CMakeLists.txt          # 构建配置
├── docs/                   # 文档
├── src/
│   ├── main.cpp           # 入口
│   ├── mainwindow.h/.cpp  # 主窗口
│   ├── widgets/           # UI 组件
│   │   ├── VideoDisplayWidget   # OpenGL 渲染
│   │   ├── CaptureWidget        # 采集视图
│   │   └── ControlPanelWidget   # 控制面板
│   ├── services/          # 服务类
│   │   ├── CameraController     # 相机控制
│   │   └── VideoRecorder        # 视频录制
│   └── data/              # 数据层
│       └── DatabaseManager      # SQLite 管理
├── resources/             # 资源文件
│   └── styles/            # 深色/浅色主题
└── libs/hikvision/        # SDK 文件
    ├── include/           # 头文件
    └── bin/               # DLL
```
