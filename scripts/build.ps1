$ErrorActionPreference = "Stop"

# Use project root as base (Phase 5: removed hardcoded d:\projects path)
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build"
$LibsBinDir = Join-Path $ProjectRoot "libs\hikvision\bin"

# ============================================================================
# Step 0: Sync Hikvision MVS SDK DLLs to libs/hikvision/bin
#   .gitignore blocks *.dll/*.lib so after a fresh clone libs/hikvision/bin
#   is empty -> auto-copy from the MVS install directory.
#   IMPORTANT: must include the ThirdParty subdir (avutil/swscale/libwinpthread)
#   otherwise MV_CC_InputOneFrame returns 0x8000000c (MV_E_LOAD_LIBRARY) and
#   recordings come out 0 bytes.
# ============================================================================
$MvsRuntimeDir = "C:\Program Files (x86)\Common Files\MVS\Runtime\Win64_x64"
$MvsLibDir = "C:\Program Files (x86)\MVS\Development\Libraries\win64"
$keyDlls = @("MvCameraControl.dll", "MvRender.dll", "MediaProcess.dll",
             "FormatConversion.dll", "MVGigEVisionSDK.dll", "MvUsb3vTL.dll",
             "SuperRender.dll", "swscale-9.dll", "avutil-60.dll",
             "libwinpthread-1.dll")
$missingDlls = $keyDlls | Where-Object { -not (Test-Path (Join-Path $LibsBinDir $_)) }

if ($missingDlls.Count -gt 0) {
    Write-Host "Syncing Hikvision SDK DLLs (missing: $($missingDlls.Count)) ..."
    if (-not (Test-Path $LibsBinDir)) {
        New-Item -ItemType Directory -Path $LibsBinDir -Force | Out-Null
    }
    if (Test-Path $MvsRuntimeDir) {
        Copy-Item "$MvsRuntimeDir\*.dll" -Destination $LibsBinDir -Force
        if (Test-Path "$MvsRuntimeDir\ThirdParty") {
            Copy-Item "$MvsRuntimeDir\ThirdParty\*.dll" -Destination $LibsBinDir -Force
        }
        if (Test-Path "$MvsLibDir\MvCameraControl.lib") {
            Copy-Item "$MvsLibDir\MvCameraControl.lib" -Destination $LibsBinDir -Force
        }
        Write-Host "SDK DLLs synced."
    } else {
        Write-Error "MVS SDK not found at $MvsRuntimeDir. Install MVS client first."
        exit 1
    }
}

# 1. Locate Visual Studio
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath

if (-not $installPath) {
    Write-Error "Error: Visual Studio C++ tools not found. Please install MSVC build tools."
    exit 1
}

Write-Host "Found Visual Studio at: $installPath"

# 2. Load dev environment
$devShellModule = Join-Path $installPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
if (Test-Path $devShellModule) {
    Import-Module $devShellModule
    Enter-VsDevShell -VsInstallPath $installPath -SkipAutomaticLocation -DevCmdArguments "-arch=x64"
} else {
    Write-Error "DevShell module not found at $devShellModule"
    exit 1
}

# 3. Configure tool paths (CMake & Ninja)
$ninjaPath = "C:\vcpkg\downloads\tools\ninja-1.13.2-windows"
$cmakePath = "C:\vcpkg\downloads\tools\cmake-3.31.10-windows\cmake-3.31.10-windows-x86_64\bin"
$env:Path = "$ninjaPath;$cmakePath;$env:Path"

Set-Location $ProjectRoot

# 4. Run CMake configure if build directory doesn't have build.ninja yet
if (-not (Test-Path (Join-Path $BuildDir "build.ninja"))) {
    Write-Host "Configuring CMake..."
    cmake -B $BuildDir -S $ProjectRoot -G Ninja `
        -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake `
        -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_MAKE_PROGRAM="$ninjaPath/ninja.exe"

    if ($LASTEXITCODE -ne 0) {
        Write-Error "CMake configuration failed."
        exit 1
    }
}

# 5. Build
Write-Host "Starting build..."
cmake --build $BuildDir --config Release

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build successful!"

    # 6. Run windeployqt
    $windeployqt = "C:\vcpkg\installed\x64-windows\tools\Qt6\bin\windeployqt.exe"
    $exePath = Join-Path $BuildDir "WormVision.exe"

    if (Test-Path $windeployqt) {
        Write-Host "Deploying Qt dependencies..."
        & $windeployqt $exePath --no-translations --no-opengl-sw

        # qsqlite.dll plugin needs sqlite3.dll next to the exe
        $sqliteDll = "C:\vcpkg\installed\x64-windows\bin\sqlite3.dll"
        if (Test-Path $sqliteDll) {
            Copy-Item $sqliteDll $BuildDir -Force
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
