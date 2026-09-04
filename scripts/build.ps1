param(
    [string]$VcpkgRoot = "",
    [string]$BuildDir = "build",
    [string]$MvsRuntimeDir = "",
    [string]$MvsLibDir = "",
    [string]$CMakePath = "",
    [string]$NinjaPath = "",
    [switch]$UseInstalledDependencies
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "build-common.ps1")

$build = Initialize-WormVisionBuild @PSBoundParameters
Invoke-WormVisionConfigure $build

Write-Host "开始编译..."
Invoke-BuildCommand $build.CMake @("--build", $build.BuildDir, "--config", "Release")

$qtRoot = Join-Path $build.InstalledDir "x64-windows"
$deployCandidates = @(
    (Join-Path $qtRoot "tools\Qt6\bin\windeployqt.exe"),
    (Join-Path $qtRoot "tools\Qt6\windeployqt.exe")
)
$windeployqt = $deployCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
    Select-Object -First 1
if (-not $windeployqt) {
    throw "未找到 windeployqt.exe：$qtRoot\tools\Qt6。请检查 vcpkg qtbase 安装。"
}

$exePath = Join-Path $build.BuildDir "WormVision.exe"
if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) {
    throw "编译输出不存在：$exePath"
}
$env:Path = "$(Join-Path $qtRoot 'bin');$(Split-Path -Parent $windeployqt);$env:Path"
Write-Host "部署 Qt 运行依赖..."
Invoke-BuildCommand $windeployqt @($exePath, "--release", "--no-translations", "--no-opengl-sw")

$sqliteDll = Join-Path $qtRoot "bin\sqlite3.dll"
if (-not (Test-Path -LiteralPath $sqliteDll -PathType Leaf)) {
    throw "未找到 SQLite 运行库：$sqliteDll。请检查 vcpkg qtbase 的 sql-sqlite 功能。"
}
Copy-Item -LiteralPath $sqliteDll -Destination $build.BuildDir -Force
Write-Host "编译和依赖部署完成：$exePath"
