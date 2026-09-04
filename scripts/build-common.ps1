# 两个入口共用环境发现与配置，保留现有构建目录和 SDK 布局。
function Invoke-BuildCommand {
    param([string]$Executable, [string[]]$Arguments)

    $previousPreference = $ErrorActionPreference
    $exitCode = $null
    $global:LASTEXITCODE = $null
    try {
        # Windows PowerShell 5.1 会将原生程序 stderr 转为错误记录；以退出码判断成败。
        $ErrorActionPreference = "Continue"
        & $Executable @Arguments 2>&1 | Out-Host
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    if ($null -eq $exitCode -or $exitCode -ne 0) {
        throw "命令失败（退出码：$exitCode）：$Executable $($Arguments -join ' ')"
    }
}

function Find-BuildTool {
    param(
        [string]$Name,
        [string]$ConfiguredPath,
        [string]$VsInstallPath,
        [string]$VcpkgRoot
    )

    if ($ConfiguredPath) {
        if (-not (Test-Path -LiteralPath $ConfiguredPath -PathType Leaf)) {
            throw "指定的 $Name 路径不存在：$ConfiguredPath"
        }
        return (Resolve-Path -LiteralPath $ConfiguredPath).Path
    }
    $command = Get-Command "$Name.exe" -CommandType Application -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($command) {
        return $command.Source
    }
    $vsRelativePath = if ($Name -eq "cmake") {
        "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    } else {
        "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
    }
    $vsTool = Join-Path $VsInstallPath $vsRelativePath
    if (Test-Path -LiteralPath $vsTool -PathType Leaf) {
        return $vsTool
    }
    $toolsDir = Join-Path $VcpkgRoot "downloads\tools"
    if (Test-Path -LiteralPath $toolsDir -PathType Container) {
        $toolDirs = Get-ChildItem -LiteralPath $toolsDir -Directory -Filter "$Name*" |
            Sort-Object LastWriteTime -Descending
        foreach ($toolDir in $toolDirs) {
            $tool = Get-ChildItem -LiteralPath $toolDir.FullName -Recurse -File -Filter "$Name.exe" |
                Select-Object -First 1
            if ($tool) {
                return $tool.FullName
            }
        }
    }
    throw "未找到 $Name.exe。请安装并加入 PATH，或用 -$($Name)Path 指定完整路径。"
}

function Sync-HikvisionSdk {
    param([string]$ProjectRoot, [string]$RuntimeDir, [string]$LibDir)

    $importLibrary = Join-Path $LibDir "MvCameraControl.lib"
    if (-not (Test-Path -LiteralPath $importLibrary -PathType Leaf)) {
        throw "缺少海康 SDK x64 导入库：$importLibrary。请安装 MVS 开发组件，或用 -MvsLibDir 指定包含该文件的目录。"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $RuntimeDir "MvCameraControl.dll") -PathType Leaf)) {
        throw "缺少海康 SDK x64 运行库：$RuntimeDir\MvCameraControl.dll。请安装 MVS，或用 -MvsRuntimeDir 指定 Runtime\Win64_x64 目录。"
    }
    $destination = Join-Path $ProjectRoot "libs\hikvision\bin"
    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    # 所有 DLL 均复制，包含 ThirdParty 下录制需要的依赖，且不按版本维护文件名清单。
    Get-ChildItem -LiteralPath $RuntimeDir -File -Filter "*.dll" |
        Copy-Item -Destination $destination -Force
    $thirdPartyDir = Join-Path $RuntimeDir "ThirdParty"
    if (Test-Path -LiteralPath $thirdPartyDir -PathType Container) {
        Get-ChildItem -LiteralPath $thirdPartyDir -Recurse -File -Filter "*.dll" |
            Copy-Item -Destination $destination -Force
    }
    Copy-Item -LiteralPath $importLibrary -Destination $destination -Force
    Write-Host "海康 SDK 已同步：$destination"
}

function Initialize-WormVisionBuild {
    param(
        [string]$VcpkgRoot = "",
        [string]$BuildDir = "build",
        [string]$MvsRuntimeDir = "",
        [string]$MvsLibDir = "",
        [string]$CMakePath = "",
        [string]$NinjaPath = "",
        [switch]$UseInstalledDependencies
    )

    $projectRoot = Split-Path -Parent $PSScriptRoot
    if (-not [IO.Path]::IsPathRooted($BuildDir)) {
        $BuildDir = Join-Path $projectRoot $BuildDir
    }
    $BuildDir = [IO.Path]::GetFullPath($BuildDir)
    if (-not $VcpkgRoot) {
        $VcpkgRoot = if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { "C:\vcpkg" }
    }
    $toolchainFile = Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"
    if (-not (Test-Path -LiteralPath $toolchainFile -PathType Leaf)) {
        throw "未找到 vcpkg 工具链：$toolchainFile。请安装 vcpkg，并设置 VCPKG_ROOT 或传入 -VcpkgRoot。"
    }
    $VcpkgRoot = (Resolve-Path -LiteralPath $VcpkgRoot).Path
    $toolchainFile = Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
        throw "未找到 vswhere.exe。请安装 Visual Studio 或 Build Tools 的 C++ 桌面开发工作负载。"
    }
    $installPaths = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0 -or -not $installPaths) {
        throw "未找到 Visual Studio MSVC x64 工具。请安装 C++ 桌面开发工作负载。"
    }
    $installPath = ($installPaths | Select-Object -First 1).Trim()
    $devShellModule = Join-Path $installPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
    if (-not (Test-Path -LiteralPath $devShellModule -PathType Leaf)) {
        throw "未找到 Visual Studio 开发环境模块：$devShellModule"
    }
    Import-Module $devShellModule
    Enter-VsDevShell -VsInstallPath $installPath -SkipAutomaticLocation `
        -DevCmdArguments "-arch=x64 -host_arch=x64" | Out-Null
    Get-Command cl.exe -CommandType Application -ErrorAction Stop | Out-Null

    $cmake = Find-BuildTool "cmake" $CMakePath $installPath $VcpkgRoot
    $ninja = Find-BuildTool "ninja" $NinjaPath $installPath $VcpkgRoot
    $env:Path = "$(Split-Path -Parent $cmake);$(Split-Path -Parent $ninja);$env:Path"
    Write-Host "CMake：$cmake"
    Write-Host "Ninja：$ninja"
    if (-not $MvsRuntimeDir) {
        $MvsRuntimeDir = Join-Path ${env:CommonProgramFiles(x86)} "MVS\Runtime\Win64_x64"
    }
    if (-not $MvsLibDir) {
        $MvsLibDir = Join-Path ${env:ProgramFiles(x86)} "MVS\Development\Libraries\win64"
    }
    Sync-HikvisionSdk $projectRoot $MvsRuntimeDir $MvsLibDir

    $installedDir = if ($UseInstalledDependencies) {
        Join-Path $VcpkgRoot "installed"
    } else {
        Join-Path $BuildDir "vcpkg_installed"
    }
    return [PSCustomObject]@{
        ProjectRoot = $projectRoot
        BuildDir = $BuildDir
        CMake = $cmake
        Ninja = $ninja
        ToolchainFile = $toolchainFile
        InstalledDir = $installedDir
        ManifestMode = $(if ($UseInstalledDependencies) { "OFF" } else { "ON" })
    }
}

function Invoke-WormVisionConfigure {
    param($Build)

    Write-Host "配置 CMake：$($Build.BuildDir)"
    Invoke-BuildCommand $Build.CMake @(
        "-B", $Build.BuildDir, "-S", $Build.ProjectRoot, "-G", "Ninja",
        "-DCMAKE_TOOLCHAIN_FILE=$($Build.ToolchainFile)",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_MAKE_PROGRAM=$($Build.Ninja)",
        "-DVCPKG_TARGET_TRIPLET=x64-windows",
        "-DVCPKG_INSTALLED_DIR=$($Build.InstalledDir)",
        "-DVCPKG_MANIFEST_MODE=$($Build.ManifestMode)"
    )
}
