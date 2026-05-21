; WormVision Installer (Inno Setup 6)
; 编译方式：ISCC.exe installer\WormVision.iss
; 输出：installer\Output\WormVision-Setup-1.0.0.exe

#define MyAppName "WormVision"
#ifndef MyAppVersion
#define MyAppVersion "1.0.0"
#endif
#define MyAppPublisher "WormLab"
#define MyAppURL "https://github.com/sarysass/wormvision-qt"
#define MyAppExeName "WormVision.exe"
#ifndef MvsRuntimeArgs
#define MvsRuntimeArgs "/S"
#endif

[Setup]
AppId={{8E2F7C4A-3D9B-4F1E-A872-9C5B6E1D8F02}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DisableProgramGroupPage=yes
LicenseFile=
OutputDir=Output
OutputBaseFilename=WormVision-Setup-{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayName={#MyAppName} {#MyAppVersion}
UninstallDisplayIcon={app}\{#MyAppExeName}
SetupIconFile=..\resources\icons\wormvision.ico
; 安装时检查 MVS SDK 是否在目标机器（缺则给提示，但不阻断）
SetupMutex=WormVisionSetupMutex

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
; 简体中文需要单独下载 ChineseSimplified.isl 到 Inno Setup\Languages\ 后启用

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加快捷方式:"

[Files]
; ---- 主程序 + Qt 插件目录（递归打包整个 build 目录）----
Source: "..\build\WormVision.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\sqlite3.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\build\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion recursesubdirs
Source: "..\build\sqldrivers\*"; DestDir: "{app}\sqldrivers"; Flags: ignoreversion recursesubdirs
Source: "..\build\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs
Source: "..\build\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion recursesubdirs
Source: "..\build\tls\*"; DestDir: "{app}\tls"; Flags: ignoreversion recursesubdirs
Source: "..\build\networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
Source: "..\build\generic\*"; DestDir: "{app}\generic"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist
; 海康 SDK ini 配置
Source: "..\build\CommonParameters.ini"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
; VC++ Redistributable 提示文件
Source: "..\build\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: ignoreversion deleteafterinstall skipifsourcedoesntexist
; 海康 MVS SDK Runtime 组件包（可选，由打包脚本通过 /DMvsRuntimeInstaller 注入）
#ifdef MvsRuntimeInstaller
Source: "{#MvsRuntimeInstaller}"; DestDir: "{tmp}"; DestName: "MVS_SDK_Runtime_Setup.exe"; Flags: deleteafterinstall
#endif
; 文档
Source: "..\docs\DLL_DEPENDENCIES.md"; DestDir: "{app}\docs"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
; 如果目标机器缺少 MVS Runtime，则安装随包携带的 Runtime 组件包
#ifdef MvsRuntimeInstaller
Filename: "{tmp}\MVS_SDK_Runtime_Setup.exe"; Parameters: "{#MvsRuntimeArgs}"; \
  StatusMsg: "正在安装 Hikvision MVS SDK Runtime..."; Flags: waituntilterminated; Check: ShouldInstallMvsRuntime
#endif
; 静默安装 VC++ 运行时（如果存在）
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; \
  StatusMsg: "正在安装 Visual C++ 运行时..."; Flags: skipifdoesntexist runascurrentuser
; 安装完成后可选启动
Filename: "{app}\{#MyAppExeName}"; Description: "立即启动 {#MyAppName}"; \
  Flags: nowait postinstall skipifsilent

[Code]
function IsMvsRuntimeInstalled(): Boolean;
begin
  Result :=
    DirExists(ExpandConstant('{commoncf32}\MVS\Runtime')) or
    DirExists(ExpandConstant('{commoncf64}\MVS\Runtime')) or
    DirExists(ExpandConstant('{commonpf32}\MVS')) or
    DirExists(ExpandConstant('{commonpf64}\MVS')) or
    DirExists(ExpandConstant('{pf32}\MVS')) or
    DirExists(ExpandConstant('{pf}\MVS'));
end;

function ShouldInstallMvsRuntime(): Boolean;
begin
  Result := not IsMvsRuntimeInstalled();
end;

function InitializeSetup(): Boolean;
begin
  Result := True;
#ifndef MvsRuntimeInstaller
  // 未内置 Runtime 安装器时，仅提示用户；内置时由 [Run] 自动补装。
  if not IsMvsRuntimeInstalled() then begin
    if MsgBox(
      '检测到此机器未安装 Hikvision MVS 客户端。' + #13#10#13#10 +
      'WormVision 需要相机驱动才能识别相机硬件，' + #13#10 +
      '应用本身可以启动，但无法连接相机。' + #13#10#13#10 +
      '建议先到海康官网下载并安装 MVS 客户端，再使用本软件。' + #13#10 +
      '是否仍然继续安装？',
      mbConfirmation, MB_YESNO) = IDNO then
    begin
      Result := False;
    end;
  end;
#endif
end;
