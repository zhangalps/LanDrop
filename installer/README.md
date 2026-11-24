# LanDrop Windows 引导安装程序构建说明

本目录包含 Windows 平台的一键构建与单文件图形化引导安装程序源码，所有分发产物统一输出到 `installer/output/`。

## 一键生成单文件安装程序

在项目根目录打开 PowerShell 执行：

```powershell
powershell -ExecutionPolicy Bypass -File .\installer\build_installer.ps1
```

执行完成后，将在 `installer/output/` 下直接生成单个双击可安装的图形化安装程序：

- `installer/output/LanDrop-Setup-1.0.0.exe`

用户下载后直接双击该 `.exe` 即可进入中文图形化安装向导（支持自定义安装路径、创建桌面快捷方式、创建开始菜单快捷方式、注册 Windows「应用和功能」卸载项以及安装后立即启动）。

## 目录结构

- `build_installer.ps1`：一键 Release 编译、收集 Qt6/QML/MinGW 依赖并封装单文件安装程序
- `setup_src/`：零依赖原生 Windows 图形化安装向导源码（`Program.cs`、`InstallEngine.cs`、`InstallerForm.cs`）
- `LanDrop.iss`：可选的 Inno Setup 6 安装脚本（若本机已安装 Inno Setup 6，`build_installer.ps1` 会自动优先使用）
- `output/`：统一输出目录（已加入 `.gitignore`）
