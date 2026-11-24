# Project Specification: LanDrop (Qt 6 Cross-Platform Transfer)

## 1. Role & Objective
你是一位精通现代 C++20、Qt 6 网络编程与 QML 现代跨平台界面设计的资深客户端架构师。你的任务是在当前工作目录下从零构建一个适用于 **Windows** 与 **Android** 的局域网极简传输工具（`LanDrop`），支持同局域网设备自动发现、对话连接确认，以及文字、图片、视频与任意文件的流式互传。

## 2. Tech Stack & Architecture
- **Core**: C++20, Qt 6.5+ (`Core`, `Gui`, `Qml`, `Quick`, `QuickControls2`, `Network`)
- **Build System**: CMake (统一管理 Windows 与 Android 构建目标及资源打包)
- **Network Protocol**:
  - **Discovery**: `QUdpSocket` 局域网广播（默认端口 `45454`，遍历所有活动的非回环 IPv4 网卡广播地址发送 JSON 心跳包；超时 6 秒自动下线）。
  - **Transport**: `QTcpServer` / `QTcpSocket` 长连接，严格使用 **13 字节固定二进制包头**（`4B Magic: 0x4C445250` + `1B MsgType` + `8B PayloadLength`）解决 TCP 粘包问题。
  - **Streaming**: 文件/图片/视频禁止一次性读入内存。必须采用 `64KB` 分块读写，通过 `QTcpSocket::bytesWritten` 信号驱动背压发送，接收端直接流式落盘至系统下载目录。

## 3. UI/UX Design Mandates (Strictly Enforced — Clean & Harmonious)
**设计哲学：温润、呼吸感、现代原生 App 质感（参考 Apple AirDrop / iOS 设置页 / 微信清爽版），拒绝生硬边框与多余装饰。**
- **去重与克制（Anti-Clutter）**：
  - **禁止**在界面上堆砌任何废话引导语（如“欢迎使用”、“点击下方设备开始局域网极速传输”等教学文案）。
  - **禁止**使用任何 Emoji、刺眼的纯黑纯白硬边框、荧光渐变或厚重阴影。
  - **减少图标数量**：不滥用杂乱图标，仅在核心操作（如顶部返回 `<`、发送附件 `+`、发送按钮）保留极简细线条矢量图标或简洁文字，其余全部通过**字号层级、留白与圆角卡片色块**区分。
- **柔和和谐的配色系统（Soft & Warm Palette）**：
  - **浅色主调（默认）**：
    - 页面底色：`#F5F6F8`（柔和浅灰底，不刺眼）
    - 卡片与气泡背景：`#FFFFFF`（纯净白，圆角 `16px`，无边框或极淡分隔 `#ECEEF2`）
    - 己方消息气泡：`#E8F1FF`（柔和晴空蓝底）搭配 `#1E293B` 深灰蓝文字（比纯蓝底白字更耐看、更高级）
    - 主操作按钮 / 在线状态点：`#3B82F6`（温润晴空蓝）/ `#10B981`（柔和薄荷绿，8px 圆点）
    - 字体层级：主标题 `#1E293B`，正文 `#334155`，次要信息/时间/大小 `#94A3B8`
  - **深色适配（跟随系统）**：底色 `#111317`，卡片 `#1B1E24`，己方气泡 `#1E3A5F`，保持低对比度护眼质感。
- **直观便捷的交互布局**：
  - **移动端 (Android)**：单屏焦点流。首页顶部仅显示本机名称与在线状态小圆点，下方直接展示圆角白色卡片式的“附近设备列表”（点击整张卡片即可发起对话申请）；连接建立后平滑推入聊天页，底部输入栏保持与手机系统键盘适配的安全边距（单手拇指易触达）。
  - **桌面端 (Windows)**：宽屏双栏（左侧 `280px` 浅灰底设备列表 + 右侧纯净白对话区），保持与移动端完全一致的圆角卡片与柔和配色语言。
  - **媒体与文件展示**：图片与视频直接以 `12px` 圆角卡片嵌入对话流（视频左下角带极小透明时长标签，点击调用系统默认相册/播放器打开）；文件展示为清爽圆角白卡（左侧简明后缀徽标如 `PDF` / `ZIP`，中间文件名与大小，底部 `3px` 圆角细进度条，完成后自动变为“打开”轻按钮）。

## 4. Platform-Specific Requirements
- **Android**:
  - 必须在 `AndroidManifest.xml` 声明 `INTERNET`、`ACCESS_NETWORK_STATE`、`ACCESS_WIFI_STATE`、`CHANGE_WIFI_MULTICAST_STATE`。
  - C++ 初始化时通过 `QJniObject` 获取 `WifiManager.MulticastLock` 并调用 `acquire()`，确保息屏或省电模式下不丢弃 UDP 广播包。
  - 文件选择直接使用 `QFile` 读取 Android `FileDialog` 返回的 `content://` URI；接收文件默认存入 `QStandardPaths::DownloadLocation` 下的 `LanDrop` 子目录。
- **Windows**:
  - 接收完成的文件卡片提供“打开文件夹”轻按钮（调用 `explorer /select,` 定位到具体文件）。

## 5. Directory Structure & Execution Rules
请按以下模块化结构组织代码，禁止把所有逻辑塞进 `main.cpp`：
```text
├── CMakeLists.txt
├── android/AndroidManifest.xml
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── Protocol.h          # 帧定义与消息类型枚举
│   │   ├── PacketCodec.h/.cpp  # 二进制封包与状态机解包器
│   │   ├── DeviceDiscovery.h/.cpp # UDP 广播与在线设备模型 (QAbstractListModel)
│   │   └── TransferSession.h/.cpp # TCP 连接管理、握手申请与 64KB 流式文件收发
│   └── platform/
│       └── AndroidHelper.h/.cpp # JNI MulticastLock 与系统路径适配
└── qml/
    ├── Main.qml                # 响应式主窗口（自动适配桌面双栏/移动单栏）
    ├── DeviceListPane.qml      # 圆角卡片设备列表与轻量化连接确认弹窗
    └── ChatPane.qml            # 清爽会话流、媒体圆角预览卡片与底部输入栏