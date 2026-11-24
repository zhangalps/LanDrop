# LanDrop 需求与变更追踪 (Request & Changelog)

> 本文件作为 **LanDrop** 功能新建、优化与变更的唯一需求入口，采用「编号 + 状态流转 + 设计方案 + 变更追溯」的流程化管理。

## 流程与状态说明

- **状态流转**：`Draft (待构思)` → `Proposed (待确认)` → `Approved (已确认)` → `In Progress (开发中)` → `Verified (已验证)` → `Done (已归档)`
- **新增需求规范**：在下方按 `REQ-XXX` 递增编号追加需求描述；确认方案后将状态更新为 `Approved` 再进入代码实现，完成后记录关联文件与验证结果。

---

## 需求索引

- **REQ-001**：自定义用户名（设备昵称）修改功能 — `Verified (已验证)`
- **REQ-002**：个人聊天与群聊功能 — `Verified (已验证)`
- **REQ-003**：手机端二级界面系统返回键/手势直接退回桌面问题修复 — `Verified (已验证)`
- **REQ-004**：定制简约分享主题应用图标（Windows & Android 全端替换） — `Verified (已验证)`

---

## REQ-001：自定义用户名（设备昵称）修改功能

- **当前状态**：`Verified (已验证)`
- **提出时间**：2026-11-24
- **原始需求**：增加用户名修改功能，因为有编号相同的首字母或者数字。

### 1. 问题与痛点
多台同品牌/同型号设备（或前缀相同的设备名，如多台 `Xiaomi`、`DESKTOP-xxx`）在设备列表与聊天气泡中首字母一致，仅靠首字母头像和默认主机名难以快速分辨具体设备。

### 2. 产品与交互设计
1. **极简触发入口**：首页顶部「本机头像与名称」区域支持点击（或在右侧提供极简编辑轻按钮），弹出圆角改名弹窗。
2. **输入与兜底**：
   - 输入框预填当前名称，限制长度 `1~24` 字符，自动去除首尾空白。
   - 提供「恢复默认」轻按钮，可一键还原为系统设备型号/主机名。
3. **头像区分度优化**：头像莫兰迪色系哈希种子改为使用唯一的 `deviceId`（而非 `deviceName`），即便设备首字母相同，也能通过不同背景色快速区分。
4. **本地持久化与实时同步**：
   - 使用 `QSettings` 持久化存储自定义名称与稳定的 `deviceId`。
   - 修改确认后，立即更新 `DeviceDiscovery` 广播内容并主动发送一次心跳；同时向所有已连接的 `PeerSession` 推送 `DeviceInfoUpdate` 控制帧，对方列表与聊天顶栏实时刷新，无需断开重连。

### 3. 验收标准
- [x] 修改用户名后，本地顶部与重启应用后均保持新名称。
- [x] 修改用户名后，局域网内其他设备的「附近设备列表」及已打开的「聊天顶栏」在 2 秒内自动刷新为新名称。
- [x] 点击「恢复默认」可正确还原系统原始设备名称。

### 4. 变更追溯
- 已实现模块：[main.cpp](file:///e:/projects/qt_projects/SimpleTrans/src/main.cpp)、[Protocol.h](file:///e:/projects/qt_projects/SimpleTrans/src/core/Protocol.h)、[DeviceDiscovery.h](file:///e:/projects/qt_projects/SimpleTrans/src/core/DeviceDiscovery.h)/[cpp](file:///e:/projects/qt_projects/SimpleTrans/src/core/DeviceDiscovery.cpp)、[TransferSession.h](file:///e:/projects/qt_projects/SimpleTrans/src/core/TransferSession.h)/[cpp](file:///e:/projects/qt_projects/SimpleTrans/src/core/TransferSession.cpp)、[DeviceListPane.qml](file:///e:/projects/qt_projects/SimpleTrans/qml/DeviceListPane.qml)
- 自动化验证：`LanDropTests::testLiveDeviceRenameSync` 通过。

---

## REQ-002：个人聊天与群聊功能（支持新建群聊与设备加入）

- **当前状态**：`Verified (已验证)`
- **提出时间**：2026-11-24
- **原始需求**：增加群聊功能和个人聊天功能，可以新建群聊，其他设备可以加入。

### 1. 产品定位与核心价值
当前版本已具备 1v1 个人直连传输能力。增加**局域网群聊**后，可覆盖「会议室资料群发」、「多设备协同讨论」、「教室/工作室文件分发」等一对多高频场景，同时保持 LanDrop「无服务器、零配置、即开即用」的极简体验。

### 2. 架构选型：去中心化 Host-Relay（群主中继）模式
在无中心服务器的纯局域网环境下，不建议采用全网状 Mesh 直连（$N$ 台设备需维护 $N(N-1)/2$ 条连接，极易出现部分成员收到、部分漏收的状态分裂），推荐采用 **群主中继（Host-Relay）** 架构：
- **群主（Group Host）**：创建群聊的设备自动成为该群的中心节点，负责维护群成员列表、分配全局递增消息序号 `seqId`、以及消息广播转发。
- **成员（Group Member）**：其他设备只需与群主保持 **1 条 TCP 长连接** 即可完成入群、接收全员消息与发言，移动端省电且消息顺序绝对一致。
- **大文件传输带宽优化（混合模式）**：
  - **文字 / 小图片**：直接由发送方发给群主，群主广播给群内其余成员，实时性高。
  - **大文件 / 视频**：群主仅在群内广播轻量元数据 `GroupFileAnnounce`（含 `fileName`、`fileSize`、`senderIp`、`senderPort`、`transferId`），群聊界面展示统一文件卡片，成员**按需点击下载**时直接向发送方建立点对点流式拉取（或提供“自动接收小文件”选项），避免群主手机上行带宽被大文件打满。

### 3. 核心业务流程设计

```mermaid
flowchart LR
    A["设备 A 点击「新建群聊」\n设置群名 / 可选 4 位口令"] --> B["UDP 45454 心跳附带\n公开群摘要 groups[]"]
    B --> C["附近设备首页「群组」区\n实时展示可加入的群卡片"]
    C --> D["设备 B 点击群卡片\n输入口令（如有）发起入群"]
    D --> E["群主 A 确认或自动放行\n下发当前成员列表"]
    E --> F["全员收到系统提示：\n「设备 B 加入了群聊」"]
```

1. **群组发现与加入（融入现有 UDP 体系）**：
   - 群主在现有 `45454` UDP 心跳包中轻量附带 `groups` 数组（仅含 `groupId`、`groupName`、`memberCount`、`hasPin`）。
   - 首页列表自然划分为两个克制区块：**`群组`**（含右上角 `+ 新建群聊` 轻按钮）与 **`附近设备`**（即 1v1 个人聊天）。
   - **主动加入**：点击附近群卡片，若设置了 4 位数字口令则输入验证，否则群主弹出轻确认框（或群主创建时勾选“免确认直接加入”）。
   - **群主邀请**：群主可在群聊右上角点击「成员/邀请」，直接勾选在线设备发送 `GroupInvite` 邀请弹窗。
2. **个人聊天 vs 群聊的 UI 差异化**：
   - **1v1 个人聊天**：延续当前极简双人对话流，气泡旁仅显示头像，不重复显示昵称。
   - **群聊对话流**：
     - 他人消息气泡上方增加一行 `11px` 次要文字显示**发送者昵称**（同一人连续发言时自动合并隐藏重复昵称）。
     - 成员进出群聊时，消息流居中插入极简浅灰系统提示（如 `Xiaomi 14 加入了群聊`）。
     - 群聊顶部栏显示：`群名称` + `在线人数（如 4人）`，右侧提供「成员」抽屉/弹窗（查看成员 IP、邀请成员、退出/解散群聊）。
3. **边界与异常闭环**：
   - **群主退出/离线**：一期采用清晰可控的「群主退出即解散群聊」策略，所有成员收到 `GroupDisbanded` 提示并保留本地只读历史记录；二期可演进为群主离线自动移交（Host Migration）。
   - **重复加入与断线重连**：以 `deviceId` 为成员唯一键，若某成员短时断线重连，自动替换旧连接并同步当前最新成员列表。

### 4. 分阶段实施建议（Roadmap）
- **Phase 1（群聊基础闭环）**：实现新建群聊、UDP 群广播、免密/口令加入、群文字消息中继、成员进出提示与解散群聊。
- **Phase 2（群媒体与文件按需拉取）**：支持群聊图片发送（自动预览）及大文件元数据广播 + 成员按需流式下载。
- **Phase 3（体验增强）**：群内 `@成员` 提醒、群聊名称修改、群主权限移交。

### 5. 变更追溯
- 已实现模块：[GroupSession.h](file:///e:/projects/qt_projects/SimpleTrans/src/core/GroupSession.h)/[cpp](file:///e:/projects/qt_projects/SimpleTrans/src/core/GroupSession.cpp)、[CMakeLists.txt](file:///e:/projects/qt_projects/SimpleTrans/CMakeLists.txt)、[Protocol.h](file:///e:/projects/qt_projects/SimpleTrans/src/core/Protocol.h)、[DeviceDiscovery.h](file:///e:/projects/qt_projects/SimpleTrans/src/core/DeviceDiscovery.h)/[cpp](file:///e:/projects/qt_projects/SimpleTrans/src/core/DeviceDiscovery.cpp)、[TransferSession.h](file:///e:/projects/qt_projects/SimpleTrans/src/core/TransferSession.h)/[cpp](file:///e:/projects/qt_projects/SimpleTrans/src/core/TransferSession.cpp)、[Main.qml](file:///e:/projects/qt_projects/SimpleTrans/qml/Main.qml)、[DeviceListPane.qml](file:///e:/projects/qt_projects/SimpleTrans/qml/DeviceListPane.qml)、[ChatPane.qml](file:///e:/projects/qt_projects/SimpleTrans/qml/ChatPane.qml)
- 自动化验证：`LanDropTests::testGroupChatHostRelayAndDirectFileFetch` 通过（9/9 全部通过）。

---

## REQ-003：移动端二级界面返回上一级直接退回桌面修复

- **当前状态**：`Verified (已验证)`
- **提出时间**：2026-11-24
- **原始需求**：手机应用进入二级界面后，返回上一级直接退回到桌面了，改一下这个问题。

### 1. 问题原因
在移动端窄屏模式下进入聊天二级界面（`StackView` 深度为 2）时，`ApplicationWindow` 未拦截系统返回键（`Key_Back` / `closing` 信号），导致侧滑返回或点击系统返回键时直接关闭了主窗口并退回手机桌面。

### 2. 实现方案与变更追溯
- 在 [Main.qml](file:///e:/projects/qt_projects/SimpleTrans/qml/Main.qml) 添加 `onClosing` 拦截与 `StandardKey.Back` / `Back` 快捷键处理：二级页面下自动弹出 `StackView` 并清理当前会话选中状态，回到设备与群聊列表首页；弹窗开启时阻止误退出。
- 在 [ChatPane.qml](file:///e:/projects/qt_projects/SimpleTrans/qml/ChatPane.qml) 为全屏图片查看器 `imageViewer` 与 `membersDialog` 开启 `focus: true`，确保按系统返回键时优先关闭浮层弹窗。

---

## REQ-004：简约分享主题应用图标定制与全端替换

- **当前状态**：`Verified (已验证)`
- **提出时间**：2026-11-24
- **原始需求**：目前应用图标还是默认的，生成一张icon图片，内容重在分享，图标简约突出工具内容，修改一下icon图标。

### 1. 设计方案
- **视觉创意**：晴空蓝（`#3B82F6` → `#60A5FA`）柔和渐变圆角矩形底色，居中纯白极简双向传输箭头与双层无线电波弧线，无文字堆砌，小尺寸下依然清晰辨识。

### 2. 变更追溯
- 图标资源生成：[landrop.png](file:///e:/projects/qt_projects/SimpleTrans/resources/icons/landrop.png)（512×512 透明圆角）、[landrop.ico](file:///e:/projects/qt_projects/SimpleTrans/resources/icons/landrop.ico)（多分辨率 Windows 图标）、`android/res/mipmap-{mdpi,hdpi,xhdpi,xxhdpi,xxxhdpi}/ic_launcher.png`。
- 工程配置：[landrop.rc](file:///e:/projects/qt_projects/SimpleTrans/resources/landrop.rc)、[CMakeLists.txt](file:///e:/projects/qt_projects/SimpleTrans/CMakeLists.txt)、[main.cpp](file:///e:/projects/qt_projects/SimpleTrans/src/main.cpp)（设置应用窗口图标）、[AndroidManifest.xml](file:///e:/projects/qt_projects/SimpleTrans/android/AndroidManifest.xml)（`android:icon="@mipmap/ic_launcher"`）。
