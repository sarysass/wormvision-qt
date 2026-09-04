# Local Video Analysis Implementation Plan

> **For agentic workers:** Use superpowers:subagent-driven-development with focused tests and one scoped final review. The user's instruction against overengineering and excessive validation takes precedence over heavyweight workflow ceremony.

**Goal:** 在现有拍摄软件的视频库选择已完成录像，运行本地分析并查看真实结果。

**Architecture:** 保留 Qt Widgets 采集端，通过 QProcess 启动 MicroHunter-Core 的回环 HTTP 服务，由 QNetworkAccessManager 异步提交和读取任务。复用引擎输出，不移植算法、不新增云服务或独立结果数据库。

**Tech Stack:** C++17 / Qt6 Widgets, Network, Sql / 现有 Python MicroHunter-Core。

**Spec:** 本次用户需求：本地拍摄、视频库选中分析、参考 wormstudio-cloud-nextjs 的展示逻辑；固定 yolo-sam2-optimized-core；图标 P1；在 wormvision-qt 新分支开发；尽量沿用已有基础和风格，不要过度编程和验证。

## Global Constraints

- 分支 `codex/local-analysis-integration`，直接使用当前 wormvision-qt 目录；用户后续已授权整理源码、构建说明后提交并推送此分支。
- 固定 route_id = yolo-sam2-optimized-core；实验类型默认 worm.crawling，可切换 worm.thrashing，不展示其他算法。
- 视频和分析结果只在本机处理；不接云上传，不修改或绕过引擎已有许可机制。
- 保持相机 SDK 采集实现；在停止后的现有轮询中等待大小稳定及 AVI 时长可读，再解除录像忙状态。
- 复用引擎 metrics.json 的动态中文列名/单位及数值，禁止在 Qt 重新计算或伪造科学指标。
- 使用本机现有 SDK/Qt/MSVC 工具链构建，不读取或复制后端 secrets 目录。
- 必要验证：新边界的定向 Qt 测试、一次 Windows 构建和小规模本地引擎集成冒烟；不跑全量模型矩阵、不做长视频压力测试。

### Task 1: 本地服务桥接与分析工作台

**Files:**
- Create: src/services/LocalAnalysisService.h/.cpp
- Create: src/widgets/AnalysisWidget.h/.cpp
- Create: src/utils/AnalysisResults.h/.cpp, tests/test_local_analysis_service.cpp, tests/test_analysis_results.cpp
- Modify: tests/CMakeLists.txt

**Interfaces:**
- AnalysisWidget(QWidget *parent = nullptr); void setSelectedVideos(const QStringList &paths); bool hasActiveAnalysis() const;
- Signal: AnalysisWidget::busyChanged(bool)（用于退出提醒）；不依赖 CameraController。
- LocalAnalysisService 负责 QProcess/回环 API/JSON，请求及结果显示不阻塞 UI。
- Task 2 在主窗加入该 Widget，并连接视频库 analysisRequested(QStringList)。

- [x] 定向测试覆盖中文路径的 JSON 请求、真实 summary/track 结构及缺失值、服务启动失败与重试、HTTP 错误及停止后的请求清理。使用可控的本地 HTTP 测试端，不模拟算法本身。
- [x] 自动发现发行程序或相邻开发目录的 .venv Python + run_cli.py；提供可保存的引擎程序/源码入口设置。参数使用独立 QStringList。
- [x] 启动回环随机端口服务，传入 parent-pid 与本地 workspace-root；读取服务地址并校验 api_version=1；关闭仅结束自己启动的进程。
- [x] 工作台包含所选视频、实验类型、设备、可选 mm/px 标定、固定算法说明、开始/取消和本地历史。
- [x] 结果包括总表/逐视频逐虫表、轨迹图，以及打开标注视频、Excel、PDF和结果目录的入口，复用系统默认程序。
- [x] 每 2 秒读取任务进度，处理所有终态；切换任务后防止旧异步响应覆盖当前选择。
- [x] 按实际 API 和输出结构实现；沿用引擎许可状态，显示真实错误，不生成模拟科研结果。

### Task 2: 视频库集成、图标与构建交付

**Files:**
- Modify: src/mainwindow.h/.cpp, src/widgets/VideoLibraryWidget.h/.cpp
- Modify: src/widgets/CaptureWidget.h/.cpp, src/services/CameraController.h/.cpp（录像忙状态及保存结束检查）
- Modify: CMakeLists.txt, installer/WormVision.iss, resources/icons/wormvision.png/.ico
- Create: docs/LOCAL_ANALYSIS.md
- Modify: tests/test_video_library_widget.cpp

**Interfaces:**
- VideoLibraryWidget::analysisRequested(const QStringList &paths) 发出勾选视频，未勾选则使用选中行。
- CaptureWidget::recordingBusyChanged(bool) 覆盖录制开始至结束统计/入库阶段，保护文件操作与分析入口。
- MainWindow 连接 Task 1 的 AnalysisWidget；关闭时若仍有任务，先提醒用户再结束本应用拥有的服务。

- [x] 视频库测试覆盖勾选/选中、录制保护及不可读 AVI 拒绝；替换云上传按钮/菜单和状态列，保留现有数据库结构。
- [x] 主窗增加“本地分析”页，连接视频选择与录像忙状态。
- [x] 原样使用 P1 PNG，并生成 Windows ICO，保留应用标识和用户数据目录。
- [x] CMake 注册新文件；安装器可携带完整引擎目录；编写使用及配置说明。
- [x] Windows 应用构建和三组定向 Qt 测试通过，完成一次集中审查及针对两项修复的窄复核；合成视频接口验证单独记录。

## Progress

### 当前 Windows 工作区实施（2026-09-04）

- 本地集成使用新分支 `codex/local-analysis-integration`，基于 `44dca95`，在原有目录工作。
- 图标来自用户提供的 `p1.png`，原图复制为 PNG 并转换为多尺寸 Windows ICO。
- 云端参考仓库仅用于只读查看，参考副本不加入当前仓库。
- 使用已有 Qt/MSVC/MVS 工具链完成本机构建。
- 按用户要求只做必要验证：请求与结果边界测试、视频库选择及录制保护测试、应用构建，以及可用环境下的一次本地 API 检查；不扩大到算法矩阵。
- 服务与页面通过简单异步 JSON 接口连接，服务负责进程/回环连接，页面负责固定路线、任务状态与结果展示。工作台公开 `setSelectedVideos`、`setCaptureBusy`、`hasActiveAnalysis` 和 `busyChanged`，供主窗集成。
- [x] 本地服务及边界测试。
- [x] 分析工作台及真实指标/产物展示。
- [x] 视频库、录像状态、主窗和图标集成。
- [x] 定向测试、构建、最终审查和使用文档。

- 两个原始仓库起始干净；Core 仅准备忽略的运行环境和所需模型权重。
- 修复审查发现的提交超时孤立任务及录像过早解除忙状态；不增加任务数据库或新的重试框架。
- 使用现有深色主题在 1280×720 下检查分析页，修复标定控件和滚动布局。
- 本机真实 API 的启动、目录、任务提交及所需模型前两帧推理已验证，取消请求成功；CPU 单帧耗时数分钟，按用户的轻量验证要求停止，不声称完整产物或标注回放已经实测。

### 源码发布与构建交付

- 用户后续要求上传适当内容，使其他开发者能够从 Git 构建软件。
- [x] 整理分析功能源文件、资源及必要测试，补充 README 和 vcpkg 清单。
- [x] 使已有 Windows 构建/打包脚本可配置路径并可靠报告失败。
- [x] 从暂存源码导出的干净副本完成 Release 构建、三组定向测试及 Inno Setup 安装包生成；构建复用本机已安装的 Qt，SDK 由脚本自动补齐。vcpkg 清单另经依赖解析检查。
- 发布目标为 `codex/local-analysis-integration` 功能分支，并创建面向 `main` 的草稿 PR 供审阅。
- 仅上传 Qt 仓库；模型、SDK 二进制、运行环境、录像、机器配置及密钥不加入提交。

### 1.1.2 发行引擎切换

- 用户明确改用工作区根目录的 `microhunter.7z`，不再使用 MicroHunter-Core 源码。
- 完整解压发行包到 `build/engine/microhunter/`，保留 `_internal` 和两份加密模型，打包时递归包含。
- 移除源码自动发现与 Python 入口，迁移旧源码配置；只有发行引擎报告许可有效才允许提交分析。
- 此前源码 FP32 补丁不施加到发行包，当前构建说明已移除该流程。
- 发行 exe 的帮助命令和离线许可状态查询成功，确认 `bundled_release: true`、`not_activated`；实际模型推理需先激活。
