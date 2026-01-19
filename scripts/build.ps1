$ErrorActionPreference = "Stop"

# 1. 定位 Visual Studio 环境
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath

if (-not $installPath) {
    Write-Error "Error: Visual Studio C++ tools not found. Please install MSVC build tools."
    exit 1
}

Write-Host "Found Visual Studio at: $installPath"

# 2. 加载开发环境
$devShellModule = Join-Path $installPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
if (Test-Path $devShellModule) {
    Import-Module $devShellModule
    Enter-VsDevShell -VsInstallPath $installPath -SkipAutomaticLocation -DevCmdArguments "-arch=x64"
} else {
    Write-Error "DevShell module not found at $devShellModule"
    exit 1
}

# 3. 配置工具路径 (CMake & Ninja)
$ninjaPath = "C:\vcpkg\downloads\tools\ninja-1.13.2-windows"
$cmakePath = "C:\vcpkg\downloads\tools\cmake-3.31.10-windows\cmake-3.31.10-windows-x86_64\bin"
$env:Path = "$ninjaPath;$cmakePath;$env:Path"

# 4. 如果 build 目录不存在或没有 build.ninja，运行 CMake configure
$buildDir = "build"
if (-not (Test-Path "$buildDir\build.ninja")) {
    Write-Host "Configuring CMake..."
    cmake -B $buildDir -S . -G Ninja `
        -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake `
        -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_MAKE_PROGRAM="$ninjaPath/ninja.exe"
    
    if ($LASTEXITCODE -ne 0) {
        Write-Error "CMake configuration failed."
        exit 1
    }
}

# 5. 运行编译
Write-Host "Starting build..."
cmake --build $buildDir --config Release

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build successful!"
    
    # 6. 运行 windeployqt 进行部署
    $windeployqt = "C:\vcpkg\installed\x64-windows\tools\Qt6\bin\windeployqt.exe"
    $exePath = "d:\projects\wormvision-qt\build\WormVision.exe"
    
    if (Test-Path $windeployqt) {
        Write-Host "Deploying Qt dependencies..."
        & $windeployqt $exePath --no-translations --no-opengl-sw
        
        # Manually copy sqlite3.dll which is required by qsqlite.dll plugin
        $sqliteDll = "C:\vcpkg\installed\x64-windows\bin\sqlite3.dll"
        if (Test-Path $sqliteDll) {
            Copy-Item $sqliteDll "d:\projects\wormvision-qt\build\" -Force
            Write-Host "Copied sqlite3.dll"
        } else {
            Write-Warning "sqlite3.dll not found in vcpkg bin"
        }
    } else {
        Write-Warning "windeployqt not found at $windeployqt"
    }

} else {
    Write-Error "Build failed."
}
