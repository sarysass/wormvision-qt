# 第三方安装器放置目录

将海康 MVS SDK Runtime x64 安装器放到此目录，打包时通过 `-MvsRuntimeInstaller` 指定。

示例:

```powershell
.\scripts\package.ps1 `
  -Version 1.0.4 `
  -MvsRuntimeInstaller .\third_party\installers\MVS_SDK_Runtime_V4.7.0.exe
```

当前官网可下载的 MVS SDK Runtime V4.7.0 安装器为 NSIS 安装包，静默参数通常使用 `/S`。

注意:

- 请先确认海康 Runtime 组件包许可允许随 WormVision 安装包分发。
- 如果 Runtime 安装器不支持静默参数，可通过 `-MvsRuntimeArgs ""` 让安装器正常显示自己的安装界面。
