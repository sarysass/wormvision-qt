# 构建依赖交付（Windows x64，1.1.2 / 1.1.3）

源码位于 `codex/local-analysis-integration` 分支。二进制依赖位于 [build-inputs-1.1.2 Release](https://github.com/sarysass/wormvision-qt/releases/tag/build-inputs-1.1.2)，与 Git 分开下载。机器可读清单见 [build-inputs.json](../third_party/build-inputs.json)，校验值另见 [SHA256SUMS.txt](../third_party/SHA256SUMS.txt)。

这是联网构建所需的项目依赖交付，不是完全离线的开发工具镜像。Visual Studio、Windows SDK、vcpkg、Inno Setup 和 7-Zip 按下面的官方入口安装；Qt 与 SQLite 由已固定基线的 vcpkg 清单准备。

## 交付内容

| 内容 | 位置及用途 |
| --- | --- |
| C++ 源码、图标、MVS 头文件、CommonParameters.ini、构建/安装脚本 | Git 仓库 |
| `microhunter.7z` | Release 原始发行包，包含 exe、运行环境和加密权重；完整解压后打进安装器，不需要 Core 源码或另装 Python |
| `wormvision-mvs-4.7.0.3-win64.zip` | Release 中的 MVS 构建库子集，包含 x64 导入库、运行 DLL、ThirdParty 录像依赖和原始许可文件；不是完整 SDK 或驱动安装器 |
| `vc_redist.x64.exe` | Release 中的微软原始 x64 运行时安装器，版本 14.44.35211.0；复制到构建目录后由现有打包脚本携带 |
| Qt 6.10、SQLite 及其依赖 | `vcpkg.json` 固定 baseline `8f095fa0342454181dc10f1b9c176c19b356ee67`，构建时自动准备 |
| 相机 USB3/GigE 驱动 | 从官方 MVS 安装器安装；仅编译不需要接相机或安装驱动 |

MVS 和 VC++ 文件保留原厂权利及许可。本依赖包不会授予额外的第三方软件权利。发布包不包含本机录像、分析结果或个人激活资料；引擎使用者仍需自己的有效许可。

## 开发工具

- [Visual Studio 2022 Build Tools](https://visualstudio.microsoft.com/vs/older-downloads/)：安装 C++ 桌面开发、MSVC v143 x64、Windows SDK、CMake 和 Ninja。本机构建使用 VS 17.14.24、MSVC 14.44.35207、Windows SDK 10.0.22621.0。
- [vcpkg](https://github.com/microsoft/vcpkg)：按仓库 README 安装并设置 `VCPKG_ROOT`。首次构建 Qt 需要联网下载和编译。
- [Inno Setup 6](https://jrsoftware.org/isdl.php)：生成安装器。
- [7-Zip](https://www.7-zip.org/download.html)：解压原始分析引擎。
- [海康机器人 MVS](https://www.hikrobotics.com/en/machinevision/service/download/)：需要相机驱动、开发文档或完整 SDK 时安装官方版本。本机保存的旧 4.6 安装器没有作为 4.7 依赖发布。

## 下载、校验、构建、打包

在普通 PowerShell 中执行。先安装上述开发工具并按 [README](../README.md) 设置 vcpkg：

```powershell
git clone --branch codex/local-analysis-integration https://github.com/sarysass/wormvision-qt.git
cd wormvision-qt

New-Item -ItemType Directory -Path third_party/downloads -Force | Out-Null
$inputs = Get-Content third_party/build-inputs.json -Raw | ConvertFrom-Json
foreach ($asset in $inputs.assets) {
    $file = Join-Path 'third_party/downloads' $asset.name
    Invoke-WebRequest -UseBasicParsing -Uri $asset.url -OutFile $file
    if ((Get-Item -LiteralPath $file).Length -ne $asset.bytes -or
        (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $asset.sha256) {
        throw "下载校验失败：$($asset.name)"
    }
}

Expand-Archive -LiteralPath third_party/downloads/wormvision-mvs-4.7.0.3-win64.zip `
    -DestinationPath third_party/downloads/mvs-4.7

.\scripts\build.ps1 `
    -MvsRuntimeDir "$PWD\third_party\downloads\mvs-4.7\Runtime\Win64_x64" `
    -MvsLibDir "$PWD\third_party\downloads\mvs-4.7\Development\Libraries\win64"

& 'C:\Program Files\7-Zip\7z.exe' x third_party/downloads/microhunter.7z -obuild/engine
if ($LASTEXITCODE -ne 0) { throw '分析引擎解压失败' }
Copy-Item -LiteralPath third_party/downloads/vc_redist.x64.exe -Destination build/
.\scripts\package.ps1 -Version 1.1.3 -SkipBuild -NoDesktopCopy
```

输出为 `installer/Output/WormVision-Setup-1.1.3.exe`。1.1.3 的输入功能改进沿用同一组发行依赖。引擎目录必须保留 `build/engine/microhunter/microhunter.exe`、`_internal/` 和 `weights/`。如果使用自定义 `-BuildDir`，解压、复制运行时及打包步骤也使用相同目录。

已有完整 MVS 安装环境时，可以直接使用 README 中的默认构建命令。使用依赖 ZIP 时，每次运行 `build.ps1` 或 `configure.ps1` 都传入上述两个 MVS 目录参数；打包使用 `-SkipBuild`，复用刚构建好的目录。

需要安装相机驱动的用户仍须安装官方 MVS。若另行取得可以随应用分发的 Runtime 安装器，可按 [安装器说明](../third_party/installers/README.md) 给 `package.ps1` 传入 `-MvsRuntimeInstaller`。本次提供的 DLL ZIP 不能传给该参数。

## 当前验证边界

这套交付支持编译 Qt 应用并生成包含发行引擎的安装包，分析引擎仍保持原样。当前包在 GTX 1660 Ti 上存在 `Observation.bbox coordinates must be finite` 的实际推理失败，尚未修复；CPU 回退未验证。Release 标记为预发布构建依赖，不应把构建成功或前端测试通过当成视频分析成功。

没有提供整套离线 Qt/vcpkg 缓存，也没有重新发布 Visual Studio 或 Windows SDK。完全断网构建需要另行准备开发工具离线安装器和 vcpkg 下载/二进制缓存。

## 后续大文件存放

原始发行引擎、SDK 库包和应用安装器放 GitHub Releases，Git 只保留小文件及固定版本下载清单。GitHub 当前要求单个 Release 附件小于 2 GiB，当前 `microhunter.7z` 为 1,915,542,859 字节，可以直接上传；超过该限制后可采用分卷或单独的对象存储，并继续记录校验值。[GitHub Release 限制](https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases)

发布新的引擎时使用新的 Release 标签并更新清单，不覆盖旧附件；这样旧版本构建仍可取回对应依赖。
