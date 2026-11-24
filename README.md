# LanDrop — 局域网极速快传与群聊

**LanDrop** 是一款基于 **Qt 6 / QML / C++20** 开发的跨平台（**Windows** & **Android**）局域网即时通信与文件传输工具。无需连接外网、无需中心服务器、无需登录账号，同一 Wi-Fi / 局域网下的设备打开即可自动发现彼此，实现文字聊天、图片预览、大文件直传与多人局域网群聊。

---

## 核心功能

### 1. 局域网自动发现与可见性控制
- **零配置自动发现**：通过局域网 UDP 广播（端口 `45454`）自动扫描并展示同一网络内的在线设备与公开群组。
- **一键上线 / 下线开关**：首页顶部提供滑动开关，关闭后立即向局域网广播下线通知并从附近设备列表隐藏，保护隐私不打扰。
- **自定义设备昵称**：点击首页顶部本机卡片即可随时修改设备名称（实时同步至局域网其他设备与已打开的聊天窗口），并按唯一设备 ID 生成区分度高的头像配色。

### 2. 1v1 极速传输与聊天
- **文字与图片消息**：支持即时文字消息、聊天内图片缩略图预览及全屏灯箱查看大图（支持双指/滚轮缩放、保存图片）。
- **大文件流式直传**：基于 13 字节二进制帧协议与 64KB 背压分片流式传输（TCP 端口 `45455` 起自适应分配），不爆内存、实时显示传输进度。
- **桌面端快捷操作**：Windows 端支持直接**拖放文件/图片**到聊天窗口发送，也支持 `Ctrl+V` 直接粘贴剪贴板文字或截图发送。

### 3. 局域网群聊（Host-Relay 模式）
- **快速建群与加入**：任意设备可点击 `+ 新建群聊` 创建群组，支持设置可选 **4 位数字口令** 或 **免确认直接加入**。
- **入群审批与主动邀请**：未开启免确认的群聊由群主弹窗审批申请；群主也可在成员面板中直接勾选在线设备发起入群邀请。
- **群文件按需拉取**：群聊中的大文件由群主广播轻量化元数据，群成员按需点击下载时直接向发送方建立点对点流式拉取，避免占用群主上行带宽。

### 4. 移动端体验与后台保活
- **Android 后台防断联**：内置轻量级 Android 前台保活服务（`LanDropKeepAliveService`），切到后台时保持局域网连接与消息接收不中断，划掉任务卡片自动退出清理。
- **系统级文件读写**：接入 Android 原生 `ACTION_OPEN_DOCUMENT` 文件选择器与 `MediaStore` 媒体库自动去重导出，接收图片/文件后可直接查看或打开。
- **手势与返回键适配**：手机窄屏模式下进入聊天二级页面时，系统侧滑返回或点击返回键会平滑退回列表首页（或优先关闭图片灯箱/弹窗），不会直接退出桌面。

---

## 安装与使用

### Windows 端
1. 获取安装包 `LanDrop-Setup-1.0.0.exe`（位于 `installer/output/` 或发布页）。
2. 双击运行安装向导，选择安装目录（默认安装至 `%LOCALAPPDATA%\LanDrop`，无需管理员权限），按需勾选桌面与开始菜单快捷方式即可完成安装。
3. 卸载可通过开始菜单中的「卸载 LanDrop」或 Windows「设置 → 应用 → 已安装的应用」完成。

### Android 端
1. 获取安装包 `LanDrop-Android-1.0.0-debug.apk` 并安装到手机。
2. 确保手机与电脑/其他手机连接在**同一局域网（同一 Wi-Fi 或同一路由器网段）**。
3. 首次启动时请允许「本地网络权限」（Android 17+ 设备必需）与「通知权限」（用于后台保活与新消息提醒）。

### 日常操作指南
- **修改设备名称**：点击首页顶部本机头像/名称区域，输入新名称确认即可。
- **隐藏自己（下线）**：点击首页顶部右侧的绿色滑动开关关闭即可下线；需要被发现时再打开。
- **开始 1v1 传输**：在「附近设备」列表中点击目标设备，进入对话页后可直接发文字、选图片/文件（Windows 支持拖入文件或 `Ctrl+V` 粘贴）。
- **创建或加入群聊**：在首页「群聊」区域点击 `+ 新建群聊`，或点击附近设备发布的群卡片申请加入。
- **查看已接收文件**：点击聊天气泡中的文件卡片或「打开所在目录」，桌面端直接定位到文件，Android 端调起系统关联应用打开。

---

## 项目结构

```text
SimpleTrans/
├── src/
│   ├── main.cpp                  # 应用入口、QML 上下文绑定与生命周期管理
│   ├── core/
│   │   ├── Protocol.h            # 二进制帧头定义与消息类型常量
│   │   ├── PacketCodec.*         # 13B 帧编解码与粘包/半包解析
│   │   ├── DeviceDiscovery.*     # UDP 45454 局域网发现、上线下线与群摘要广播
│   │   ├── model/                # MessageListModel / NearbyGroupModel / GroupMemberListModel
│   │   └── session/              # PeerSession / TransferManager / GroupRoom / GroupManager
│   └── platform/                 # Android 网络、存储、Intent 与保活服务 JNI 桥接
├── qml/
│   ├── Theme.qml                 # 浅色 / 深色自适应主题与莫兰迪头像色板
│   ├── Main.qml                  # 响应式双栏（桌面）/ StackView（移动）主框架
│   ├── DeviceListPane.qml        # 本机状态栏、上线开关、群聊与附近设备列表
│   ├── ChatPane.qml              # 对话消息流、输入栏与成员/图片浮层调度
│   ├── components/               # 头像、气泡、文件卡片、输入栏等独立 UI 组件
│   └── dialogs/                  # 改名、新建群聊、入群口令、审批与邀请等独立弹窗
├── android/                      # AndroidManifest.xml、应用图标与 LanDropKeepAliveService.java
├── installer/                    # Windows 单文件安装向导源码与多平台一键打包脚本
├── tests/                        # QtTest 自动化单元测试套件
└── request.md                    # 需求追踪与变更日志
```

---

## 开发者构建与打包

### 环境要求
- **Qt 6.5+**（当前验证环境为 **Qt 6.12.0 MinGW 64-bit**）
- **CMake 3.21+**、**C++20** 编译器（MinGW GCC 13+）
- 如需编译 Android APK：Android SDK + NDK `27.2.12479018` + JDK 17

### 编译与运行单元测试（Windows）
```powershell
$env:Path = "F:\Qt\Tools\mingw1310_64\bin;F:\Qt\6.12.0\mingw_64\bin;F:\Qt\Tools\CMake_64\bin;" + $env:Path

cmake -S . -B build/landrop_mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build/landrop_mingw -j 8

# 运行全量自动化单元测试（协议编解码、握手、大文件流式传输、改名同步、群聊中继、上下线切换）
.\build\landrop_mingw\LanDropTests.exe
```

### 一键打包多平台分发产物
运行以下命令会自动完成 Release 编译、`windeployqt` 依赖收集、生成 Windows 单文件图形化安装程序，并自动搜集最新编译的 Android APK 到 `installer/output/`：

```powershell
powershell -ExecutionPolicy Bypass -File .\installer\build_installer.ps1
```

输出目录 `installer/output/` 包含：
- `LanDrop-Setup-1.0.0.exe` — Windows 单文件图形化安装程序
- `LanDrop-Android-1.0.0[-debug].apk` — 最新 Android APK 安装包
