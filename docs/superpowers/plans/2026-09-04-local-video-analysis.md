# Local Video Analysis Implementation Plan

> **For agentic workers:** Use superpowers:subagent-driven-development with focused tests and one scoped final review. The user's instruction against overengineering and excessive validation takes precedence over heavyweight workflow ceremony.

**Goal:** 在现有拍摄软件的视频库选择已完成录像，运行本地分析并查看真实结果。

**Architecture:** 保留 Qt Widgets 采集端，通过 QProcess 启动 MicroHunter-Core 的回环 HTTP 服务，由 QNetworkAccessManager 异步提交和读取任务。复用引擎输出，不移植算法、不新增云服务或独立结果数据库。

**Tech Stack:** C++17 / Qt6 Widgets, Network, Sql / 现有 Python MicroHunter-Core。

**Spec:** 本次用户需求：本地拍摄、视频库选中分析、参考 wormstudio-cloud-nextjs 的展示逻辑；固定 yolo-sam2-optimized-core；图标 P1；在 wormvision-qt 新分支开发；不要过度编程和验证；可使用 ssh win1。

## Global Constraints

- 分支 codex/local-video-analysis，直接使用当前 wormvision-qt 目录；不额外创建 worktree，不提交或推送。
- 固定 route_id = yolo-sam2-optimized-core；实验类型默认 worm.crawling，可切换 worm.thrashing，不展示其他算法。
- 视频和分析结果只在本机处理；不接云上传，不修改或绕过引擎已有许可机制。
- 保持相机 SDK 采集实现；分析只接收已经完成、存在且非零的视频，不分析正在写入的录像。
- 复用引擎 metrics.json 的动态中文列名/单位及数值，禁止在 Qt 重新计算或伪造科学指标。
- 不覆盖 win1 现有脏工作区；在独立目录构建，不读取或复制 secrets 目录。
- 必要验证：新边界的定向 Qt 测试、一次 Windows 构建和小规模本地引擎集成冒烟；不跑全量模型矩阵、不做长视频压力测试。

### Task 1: 本地服务桥接与分析工作台

**Files:**
- Create: src/services/LocalAnalysisService.h/.cpp
- Create: src/widgets/AnalysisWidget.h/.cpp
- Create: tests/test_local_analysis.cpp
- Modify: tests/CMakeLists.txt（只添加新测试）

**Interfaces:**
- AnalysisWidget(QWidget *parent = nullptr); void setSelectedVideos(const QStringList &paths); bool hasActiveAnalysis() const;
- Signal: AnalysisWidget::busyChanged(bool)（用于退出提醒）；不依赖 CameraController。
- LocalAnalysisService 负责 QProcess/回环 API/JSON，请求及结果显示不阻塞 UI。
- Task 2 在主窗加入该 Widget，并连接视频库 analysisRequested(QStringList)。

- [ ] 先编写定向测试，覆盖：提交载荷固定算法且保留中文绝对视频路径/实验类型；正确解析 metrics.json 的 summary.rows[].values 和 videos[].tracks[].values（null 不转 0）；启动失败/错误响应不会留下永久进行中状态。使用可控的本地 HTTP 测试端，避免模拟算法本身。
- [ ] 启动程序自动发现 appDir/engine/microhunter.exe 或开发目录的 .venv Python + run_cli.py；提供可保存的引擎程序/可选源码入口选择。参数为独立 QStringList，不用 shell 拼接。
- [ ] 启动 serve --host 127.0.0.1 --port 0 --no-browser --parent-pid <QtPID> --workspace-root <用户数据下分析目录>，读取 MICROHUNTER_SERVE_URL；超时/退出/网络失败提供可恢复错误；关闭仅结束自己启动的进程。
- [ ] 工作台包含所选视频、实验类型、设备（自动/CUDA/CPU）、可选 mm/px 标定、固定算法说明；开始、取消、刷新及本地历史任务列表。
- [ ] 结果包括总表/逐视频逐虫表、轨迹图片预览，以及打开标注视频、Excel、PDF和结果目录的入口。用系统播放器打开引擎生成的视频，不新增 Qt Multimedia 或云端逐帧数据适配层。
- [ ] 以约 2 秒间隔读取任务进度；区分 queued/running/completed/failed/cancelled，结果只有确认 completed 后才显示完成。切换任务后不让旧异步响应覆盖新选择。
- [ ] 读取实际 API 文档和输出结构，处理启动/未激活/模型缺失错误，禁止展示模拟结果。完成后写简短测试和文件报告，不提交。

### Task 2: 视频库集成、图标与构建交付

**Files:**
- Modify: src/mainwindow.h/.cpp, src/widgets/VideoLibraryWidget.h/.cpp
- Modify: src/widgets/CaptureWidget.h/.cpp（只增加录像忙状态接口/信号）
- Modify: CMakeLists.txt, installer/WormVision.iss, resources/icons/wormvision.png/.ico
- Create: docs/LOCAL_ANALYSIS.md
- Modify: tests/test_video_library_widget.cpp

**Interfaces:**
- VideoLibraryWidget::analysisRequested(const QStringList &paths) 发出勾选视频，未勾选则使用选中行。
- CaptureWidget::recordingBusyChanged(bool) 覆盖录制开始至结束统计/入库阶段，保护文件操作与分析入口。
- MainWindow 连接 Task 1 的 AnalysisWidget；关闭时若仍有任务，先提醒用户再结束本应用拥有的服务。

- [ ] 用一个定向 UI 测试检查所选视频信号及录制期间禁止提交；替换云上传按钮/右键菜单，删除上传状态显示，不做无关数据库迁移。
- [ ] 主窗增加“本地分析”页，从视频库选中视频后跳转；捕获录像尚未结束的状态，避免读取正在写入文件。
- [ ] 原样使用用户 P1 作为 PNG 品牌图，生成匹配的 Windows ICO 资源，保留应用标识和用户数据目录。
- [ ] CMake 注册新源文件；安装器可携带 build/engine 下完整引擎目录；文档写清开发模式/发行引擎配置、输出位置、许可边界及验证范围。
- [ ] 使用 win1 独立暂存目录，复用已安装 SDK/Qt/MSVC；先运行新测试再编译应用，最后做短视频单路线必要冒烟。保留未验证硬件事项，不夸大交付状态。

## Progress

- 分支创建完成，两个原始仓库起始干净。
- 云端参考只读克隆于 /tmp/wormvision-reference.a6Ztj9/repo；采用页面分区和动态指标表思路，不迁移云接口或数据分片格式。
- win1 SSH 已实测可用；既有 Qt 仓库脏，禁止覆盖。
- 决策：按用户要求减少流程开销，仅边界测试和一次最终审查；保留本计划作为进度记录，不创建重复设计文档。
