# 本地视频分析

WormVision 在原有采集、视频库和深浅主题基础上增加“本地分析”页。
视频由本机 MicroHunter-Core 处理，界面不再提供云上传入口。

## 使用流程

1. 在“采集”页完成录像，等待录像保存结束。
2. 打开“视频库”，勾选需要分析的视频；未勾选时使用选中的表格行。
3. 点击“分析选中”，进入“本地分析”。
4. 选择“固体爬行”或“液体摆动”，计算设备通常保持“自动”。有实测标定时填写 mm/px，没有时保持未启用。
5. 点击“开始分析”。算法固定为 `yolo-sam2-optimized-core`，同时生成标注回放。
6. 完成后查看汇总指标、逐视频逐虫指标和轨迹图，或打开标注视频、Excel、PDF及结果目录。

录像保存期间禁止提交分析、删除/重命名视频及更换保存位置；分析进行时也保护视频库文件操作。
停止录像后，现有轮询会等待文件大小至少连续约 600ms 稳定且 AVI 头可解析出时长。
约 6 秒内仍不能确认保存时会提示；视频库也会拒绝提交时长不可读的 AVI。该检查不替代完整视频解码校验。
运行时可查看其他历史结果，“取消任务”始终针对本次活动任务。取消由引擎在处理检查点响应，可能需要稍候。
退出时若录像或分析未完成，会询问是否结束任务。

## 引擎配置

界面按以下顺序查找引擎：

1. “引擎设置”中保存的程序和可选 Python 入口。
2. 程序目录的 `engine/microhunter.exe` 或 `engine/microhunter/microhunter.exe`。
3. 相邻 `MicroHunter-Core/.venv/Scripts/python.exe` 与 `MicroHunter-Core/run_cli.py`。

发行版应复制完整引擎目录，保留 `_internal`、权重和其他运行依赖，不能只复制 exe。
在本项目 `build/engine/` 放入完整引擎后，现有安装器会一起打包。

源码运行要求遵循引擎 `pyproject.toml`：Python 3.12，以及引擎声明的依赖。
已验证 Core 提交为 `9cdd91e1fc44ac1a48e04139fe52ffad2da46477`，需要本地 API v1；仅带旧 CLI 的引擎不兼容。
Qt 的编译不依赖 Core，分析时才需要引擎仓库读取权限或维护者提供的完整发行目录。首次构建见 [README](../README.md)。
在引擎目录执行以下命令创建环境，并按官方清单只下载该路线所用的 YOLO 和 SAM2 权重：

```powershell
uv sync --locked --python 3.12 --no-dev
.\.venv\Scripts\python.exe -c "from microhunter.deployments.download import load_download_specs, sync_weights; sync_weights(tuple(s for s in load_download_specs() if s.deployment_id in ('worm-yolo-default', 'worm-sam2-default')))"
```

如果引擎位于其他目录，在“引擎设置”选择其 Python 程序，并将入口设为该仓库的 `run_cli.py`。

发行版沿用引擎已有激活机制，可以在“许可与激活”中激活。视频不会上传；许可激活和续期按引擎原有规则联网。
源码模式仅在引擎明确报告 `bundled_release: false` 时显示无需激活，Qt 不修改或绕过后端守卫。

## 数据与结果

- Qt 通过 `QProcess` 启动自己的引擎进程，绑定 `127.0.0.1` 随机端口，约每 2 秒读取任务进度。
- 读取服务地址后还会检查 `/api/system/info` 的 `api_version: 1`，确认服务就绪。
- 工作区位于应用用户数据目录的 `analysis` 下；Windows 通常是 `%LOCALAPPDATA%/WormLab/WormVision/analysis`。
- 结果位于工作区 `outputs/<run>/` 下，包括 `results/metrics.json`、Excel、`qc/trajectory`、`qc/review` 和 `report`。
- 指标表直接读取引擎的中文列名、单位及数值；`null` 和缺失值显示为 `—`。Qt 不重新计算科研指标。
- 当前百分比属于引擎正在执行的阶段；进入下一阶段可能重新计数。
- 内存任务使用 `job_id`，持久化历史使用引擎的运行记录。页面重启后从历史读取结果。
- 图片在页面内预览，视频、Excel和PDF由系统默认程序打开。

出现启动失败、缺少权重或参数错误时，界面显示引擎实际错误。修正环境后可通过“刷新历史”重连，或在“引擎设置”更改程序后重新启动。
提交请求留出引擎许可续期所需时间；若网络中断导致提交结果无法确认，界面会停止自有引擎，避免任务在后台继续运行。重新连接后可查看历史状态。

1.1.1 修复“自动”计算设备的请求参数：不向引擎传入字符串 `auto`，由后端选择可用的 CUDA 或 CPU；手动设备仍传 `cuda:0` 或 `cpu`。若在 1.1.0 遇到 `Invalid CUDA 'device=auto' requested`，请更新并重启应用后重新提交；该错误可能改变旧引擎进程的设备环境，不能仅刷新历史后重试。

本机 GTX 1660 Ti 另发现 YOLO 默认半精度产生非有限检测框，导致 `Observation.bbox coordinates must be finite`。对应 Core 基线需应用 [YOLO 单精度兼容补丁](../patches/microhunter-core-yolo-fp32.patch)，源码配置步骤见 README。补丁只调整 YOLO 检测/跟踪的默认精度，不丢弃无效框或改变指标计算。

## 实现边界与验证

新增代码集中于 `LocalAnalysisService`、`AnalysisWidget` 和 `AnalysisResults`；主窗口、视频库及录制忙状态沿用现有结构，未迁移云端服务或更换 UI 框架。
后端契约以相邻引擎的 `docs/local-api.md` 和实际实现为准。

展示组织参考 `inflow-lab/wormstudio-cloud-nextjs` 的结果页、`track-details-section.tsx`、`metrics-table-card.tsx` 和 `task-details-panel.tsx`。
本地使用引擎现成的轨迹图、标注视频和报告，不引入云端分片、存储或上传协议。

定向检查覆盖视频选择与录像保护、真实指标 JSON 解析及缺失值、引擎启动握手与重试、HTTP 错误和进程重启。
运行命令：在构建完成后执行 `ctest --test-dir build --output-on-failure -R "^(test_video_library_widget|test_analysis_results|test_local_analysis_service)$"`。
真实相机录制仍需按 `docs/SMOKE_TEST.md` 接设备确认；合成视频只能检查流程，不能证明识别精度或科研指标效度。

本机验证记录（2026-09-04）：

- Windows Release 应用构建成功，上述三组定向测试全部通过。
- 使用既有深色主题检查 1280×720 页面，修复标定选项和左侧滚动布局。
- 相邻 Core 的 Python 3.12 环境及所需两项模型已准备，权重经官方下载器校验。
- 真实本地 API 已启动，版本、许可快照、目录接口及固定路线任务提交正常；8 帧合成 AVI 的 CPU 分析推进至 2/8，取消接口返回成功。
- CPU 单帧耗时数分钟，因此限定验证范围并停止该次测试；完整结果产物尚未完成实测。该次请求未开启标注回放，回放也未实测。验证脚本与快照保留在忽略的 `build/engine-smoke/`。

1.1.1 修复后补充验证（2026-09-04）：

- 真实界面提交请求的回归测试覆盖自动、CPU 和 CUDA，修复前自动模式失败，修复后三种选择及原服务测试通过。
- 本机已安装并启动 1.1.1，安装后的程序哈希与构建产物一致。
- 在新引擎进程中，以同一优化路线和修复后的自动设备语义重试一段 160 帧 AVI，完整分析成功，生成指标 JSON、Excel、PDF、轨迹图及 160 帧标注视频。
- 此次真实分析通过引擎 CLI 运行并保存到应用的同一分析工作区，可在本地分析历史中查看；Computer Use 已读取原失败界面，但当前 Windows 截图接口不兼容，未完成结果页面的视觉复核。
