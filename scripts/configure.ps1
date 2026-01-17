$ErrorActionPreference = "Stop"

# 1. 定位 Visual Studio 环境
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath

if (-not $installPath) {
    Write-Error "Error: Visual Studio C++ tools not found. Please install MSVC build tools."
    exit 1
}

Write-Host "Found Visual Studio at: $installPath"

# 2. 加载开发环境 (使用 Microsoft.VisualStudio.DevShell.dll)
# 注意：Launch-VsDevShell.ps1 可能会改变当前 shell，但在脚本块中运行最好显式调用
$devShellModule = Join-Path $installPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
if (Test-Path $devShellModule) {
    Import-Module $devShellModule
    Enter-VsDevShell -VsInstallPath $installPath -SkipAutomaticLocation -DevCmdArguments "-arch=x64"
} else {
    # 备用方法：调用 vcvars64.bat 并提取环境变量（较复杂，暂假定 DevShell 可用）
    Write-Error "DevShell module not found at $devShellModule"
    exit 1
}

# 3. 配置工具路径
$ninjaPath = "C:\vcpkg\downloads\tools\ninja-1.13.2-windows"
$cmakePath = "C:\vcpkg\downloads\tools\cmake-3.31.10-windows\cmake-3.31.10-windows-x86_64\bin"
$env:Path = "$ninjaPath;$cmakePath;$env:Path"

Write-Host "Checking build tools..."
Get-Command cl.exe | Select-Object Source
Get-Command ninja.exe | Select-Object Source
cmake --version

# 4. 运行 CMake 配置
Write-Host "Configuring CMake..."
# 确保清理旧的缓存
if (Test-Path "build") {
    Remove-Item -Recurse -Force "build" -ErrorAction SilentlyContinue
}

# 使用 Ninja 生成器 (因为它更快且我们有环境了)
cmake -B build -S . -G Ninja `
    -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_MAKE_PROGRAM="C:\vcpkg\downloads\tools\ninja-1.13.2-windows\ninja.exe"

if ($LASTEXITCODE -eq 0) {
    Write-Host "CMake configuration successful!"
} else {
    Write-Error "CMake configuration failed."
}
