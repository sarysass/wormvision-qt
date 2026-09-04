# WormVision Qt

Windows 上的线虫拍摄与本地视频分析软件，使用 Qt Widgets 和海康机器人 MVS SDK。

- 采集相机画面、录像、抓拍和管理本地视频库。
- 完成录像后勾选视频，调用本机 MicroHunter-Core 分析，不上传视频。
- 固定使用 `yolo-sam2-optimized-core`，显示进度、历史任务、汇总和逐虫指标，并打开轨迹图、标注视频及报告。
- 保留原有深浅主题，版本 1.1.0 使用仓库中的 WormStudio 图标。

## 构建前准备

支持 Windows x64、MSVC 和 Release 构建。编译 Qt 应用不要求安装 Python、模型或连接相机。

| 依赖 | 准备方式 |
| --- | --- |
| Git、PowerShell 5.1 或 7 | 使用普通 PowerShell 终端运行下文命令 |
| Visual Studio 2022 / Build Tools | 安装“使用 C++ 的桌面开发”、MSVC v143 x64、Windows SDK、C++ CMake tools（包含 CMake/Ninja） |
| CMake ≥ 3.22、Ninja | 可来自 PATH、Visual Studio CMake tools，或 vcpkg 已下载的工具；脚本不要求固定版本目录 |
| vcpkg | 设置 `VCPKG_ROOT`，或传入 `-VcpkgRoot`；仓库 `vcpkg.json` 固定依赖基线，包含 Qt 6.10 和 SQLite 所需功能 |
| 海康机器人 MVS x64 SDK | 安装官方完整 MVS 的 Development 和 Runtime 组件；已验证 MVS 4.7（DLL 4.7.0.3）。[官方下载](https://www.hikrobotics.com/en/machinevision/service/download/) |
| Inno Setup 6（仅打包） | 安装到默认目录，或给打包脚本传入 `-InnoSetupPath` |

SDK 头文件已在仓库中，`MvCameraControl.lib` 和运行 DLL 由脚本从本机 MVS 安装目录补齐。它们不放入 Git；构建前需按供应商条款取得并安装 SDK。

## 从源码构建

本地分析功能当前位于 `codex/local-analysis-integration` 分支：

```powershell
git clone --branch codex/local-analysis-integration https://github.com/sarysass/wormvision-qt.git
cd wormvision-qt
```

如果尚未安装 vcpkg，可按以下方式准备；已有 vcpkg 时直接设置它的实际路径：

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\dev\vcpkg
& C:\dev\vcpkg\bootstrap-vcpkg.bat -disableMetrics
$env:VCPKG_ROOT = 'C:\dev\vcpkg'
```

安装好 MVS 后，在项目根目录执行：

```powershell
.\scripts\build.ps1
.\build\WormVision.exe
```

脚本自动发现 VS 的 x64 编译环境和构建工具，按 vcpkg 清单准备依赖，同步 MVS 运行库（含录制所需的 ThirdParty DLL），编译后调用 `windeployqt` 部署 Qt 插件及 SQLite。首次准备 Qt 依赖可能较慢。

默认依赖目录为 `build/vcpkg_installed`，使用 [vcpkg manifest mode](https://learn.microsoft.com/en-us/vcpkg/concepts/manifest-mode)。已有兼容的 `VCPKG_ROOT/installed/x64-windows` 时，可复用它以避免再次编译 Qt：

```powershell
.\scripts\build.ps1 -UseInstalledDependencies
```

自定义路径示例（只填写需要覆盖的项）：

```powershell
.\scripts\build.ps1 -VcpkgRoot 'D:\tools\vcpkg' -BuildDir 'out\release' `
  -MvsRuntimeDir 'D:\MVS\Runtime\Win64_x64' `
  -MvsLibDir 'D:\MVS\Development\Libraries\win64'
```

还可通过 `-CMakePath`、`-NinjaPath` 指定工具的 exe。`configure.ps1` 支持相同参数，只配置不编译，且不会删除已有构建目录。更换依赖模式或工具链时请指定新的 `-BuildDir`。

## 本地分析引擎

Qt 客户端和算法引擎分开构建。运行分析需要 [MicroHunter-Core](https://github.com/inflow-lab/MicroHunter-Core) 的读取权限，或维护者提供的完整引擎发行目录；没有引擎仍可编译和使用拍摄、视频库。

已验证的后端提交为 `9cdd91e1fc44ac1a48e04139fe52ffad2da46477`，要求 Python 3.12、`api_version: 1` 以及 `yolo-sam2-optimized-core` 路线。源码开发时将两个仓库放在同一父目录：

```text
workspace/
  wormvision-qt/
  MicroHunter-Core/
```

在 Core 目录执行：

```powershell
uv sync --locked --python 3.12 --no-dev
.\.venv\Scripts\python.exe -c "from microhunter.deployments.download import load_download_specs, sync_weights; sync_weights(tuple(s for s in load_download_specs() if s.deployment_id in ('worm-yolo-default', 'worm-sam2-default')))"
```

应用会查找相邻 Core 的 `.venv/Scripts/python.exe` 与 `run_cli.py`。安装到 Program Files 后，请在“本地分析 → 引擎设置”选择这两个文件，或使用随安装包携带的完整引擎。引擎设置保存在各自电脑上，不随源码同步；本仓库不提交许可密钥或模型权重。

详细操作、许可行为、结果格式及验证范围见 [本地分析说明](docs/LOCAL_ANALYSIS.md)。

## 生成安装包

先完成构建与部署，再执行：

```powershell
.\scripts\package.ps1 -Version 1.1.0 -SkipBuild
```

产物为 `installer/Output/WormVision-Setup-1.1.0.exe`，默认还会复制到桌面。可使用 `-NoDesktopCopy` 禁止桌面副本；自定义构建目录时同时传入 `-BuildDir`。省略 `-SkipBuild` 时会先构建，支持 `-VcpkgRoot` 和 `-UseInstalledDependencies`。

若要分发包含本地分析能力的完整安装包，先把维护者提供的完整 MicroHunter 引擎复制到 `<BuildDir>/engine/`，保留其依赖和权重。支持 `engine/microhunter.exe` 或 `engine/microhunter/microhunter.exe` 两种目录布局。未放入引擎时，安装包只携带 Qt 应用，使用者需单独配置引擎。

可通过 `-MvsRuntimeInstaller` 携带供应商允许分发的相机运行时安装器，详见 [第三方安装器说明](third_party/installers/README.md)。

## 必要验证

在完成 `build.ps1` 后，使用 CMake 同目录的 `ctest.exe` 或 PATH 中的 `ctest`：

```powershell
ctest --test-dir build --output-on-failure -R '^(test_video_library_widget|test_analysis_results|test_local_analysis_service)$'
```

CTest 已配置运行库和插件搜索路径，不需要手动改系统 PATH。测试使用临时数据与本地测试服务，不运行模型。相机录制需按 [设备检查说明](docs/SMOKE_TEST.md) 接硬件验证；完整视频分析及标注回放不属于这些单元测试的验证范围。

## 仓库内容

Git 包含 C++ 源码、Qt 资源、测试、构建/安装脚本和文档。构建产物、安装包、MVS 二进制、Python 环境、权重、录像、结果及激活资料留在本地或通过单独的发行渠道交付，不应执行 `git add -f` 将它们加入仓库。
