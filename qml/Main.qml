import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 960
    height: 640
    minimumWidth: 360
    minimumHeight: 540
    visible: true
    title: "LanDrop"

    readonly property bool isDark: Application.styleHints.colorScheme === Qt.Dark
    readonly property bool isWide: width >= 760

    readonly property QtObject theme: QtObject {
        readonly property color pageBg:          root.isDark ? "#111317" : "#F5F6F8"
        readonly property color surfaceBg:       root.isDark ? "#1B1E24" : "#FFFFFF"
        readonly property color chatBg:          root.isDark ? "#111317" : "#F5F6F8"
        readonly property color chatBarBg:       root.isDark ? "#1B1E24" : "#FFFFFF"
        readonly property color outgoingBubble:  root.isDark ? "#1E3A5F" : "#E8F1FF"
        readonly property color outgoingText:    root.isDark ? "#E2E8F0" : "#1E293B"
        readonly property color incomingBubble:  root.isDark ? "#1E222B" : "#FFFFFF"
        readonly property color bubbleBorder:    root.isDark ? "#2A303C" : "#E8ECF2"
        readonly property color divider:         root.isDark ? "#262B34" : "#ECEEF2"
        readonly property color accent:          "#3B82F6"
        readonly property color accentHover:     "#2563EB"
        readonly property color accentSoft:      root.isDark ? "#1E2D45" : "#EFF6FF"
        readonly property color onlineDot:       "#10B981"
        readonly property color pendingDot:      "#F59E0B"
        readonly property color textPrimary:     root.isDark ? "#F1F5F9" : "#1E293B"
        readonly property color textBody:        root.isDark ? "#CBD5E1" : "#334155"
        readonly property color textMuted:       root.isDark ? "#64748B" : "#94A3B8"
        readonly property color inputBg:         root.isDark ? "#13161C" : "#F3F5F8"
        readonly property color badgeBg:         root.isDark ? "#262C38" : "#F1F5F9"
        readonly property color dangerText:      "#EF4444"

        function avatarInitial(name) {
            if (!name || name.trim().length === 0)
                return "L"
            var trimmed = name.trim()
            // Split by common separators (-, _, space) to form a crisp 2-char monogram (e.g. zhg-Xiaomi13 -> ZX)
            var parts = trimmed.split(/[\s\-_]+/).filter(function(p) { return p.length > 0 })
            if (parts.length >= 2) {
                return (parts[0].charAt(0) + parts[1].charAt(0)).toUpperCase()
            }
            // Single ASCII token with 2+ chars: first + last char (e.g. Zhg01 -> Z1)
            if (/^[A-Za-z0-9]+$/.test(trimmed) && trimmed.length >= 2) {
                return (trimmed.charAt(0) + trimmed.charAt(trimmed.length - 1)).toUpperCase()
            }
            // CJK or single char
            return trimmed.charAt(0).toUpperCase()
        }

        function _avatarIndex(seed) {
            var s = seed ? String(seed) : "LanDrop"
            // FNV-1a 32-bit hash for low collision across UUIDs
            var h = 2166136261
            for (var i = 0; i < s.length; ++i) {
                h ^= s.charCodeAt(i)
                h = Math.imul(h, 16777619)
            }
            return (h >>> 0) % 12
        }

        function avatarBg(seed) {
            var lightBgs = [
                "#DBEAFE", "#D1FAE5", "#EDE9FE", "#FEF3C7", "#FFE4E6", "#E0F2FE",
                "#CCFBF1", "#E0E7FF", "#FFEDD5", "#DCFCE7", "#FAE8FF", "#CFFAFE"
            ]
            var darkBgs = [
                "#1E3352", "#14352A", "#2D2246", "#3D2E14", "#3F1D28", "#163244",
                "#133835", "#24284E", "#422714", "#173822", "#3A1C42", "#143640"
            ]
            var idx = _avatarIndex(seed)
            return root.isDark ? darkBgs[idx] : lightBgs[idx]
        }

        function avatarFg(seed) {
            var lightFgs = [
                "#2563EB", "#059669", "#7C3AED", "#D97706", "#E11D48", "#0284C7",
                "#0D9488", "#4F46E5", "#EA580C", "#16A34A", "#C026D3", "#0891B2"
            ]
            var darkFgs = [
                "#93C5FD", "#6EE7B7", "#C4B5FD", "#FCD34D", "#FDA4AF", "#7DD3FC",
                "#5EEAD4", "#A5B4FC", "#FDBA74", "#86EFAC", "#F0ABFC", "#67E8F9"
            ]
            var idx = _avatarIndex(seed)
            return root.isDark ? darkFgs[idx] : lightFgs[idx]
        }
    }

    color: theme.pageBg

    function handleMobileBack() {
        if (incomingDialog.visible || incomingGroupJoinDialog.visible) {
            return true
        }
        if (!root.isWide && mobileStack.depth > 1) {
            mobileStack.pop()
            transferManager.clearActivePeer()
            groupManager.clearActiveGroup()
            return true
        }
        return false
    }

    // Handle Android hardware/gesture Back button & window close cleanly
    onClosing: function(close) {
        if (root.handleMobileBack()) {
            close.accepted = false
        }
    }

    Shortcut {
        sequence: StandardKey.Back
        enabled: !root.isWide && mobileStack.depth > 1
        onActivated: root.handleMobileBack()
    }

    Connections {
        target: transferManager
        function onActivePeerChanged() {
            if (!root.isWide && transferManager.activePeerId.length > 0 && mobileStack.depth === 1) {
                mobileStack.push(mobileChatComponent)
            }
        }
        function onHandshakeRejected(peerId, reason) {
            toastBanner.show(qsTr("对方已拒绝连接"))
        }
        function onConnectionFailed(reason) {
            toastBanner.show(qsTr("连接失败: ") + reason)
        }
    }

    Connections {
        target: groupManager
        function onActiveGroupChanged() {
            if (!root.isWide && groupManager.activeGroupId.length > 0 && mobileStack.depth === 1) {
                mobileStack.push(mobileChatComponent)
            }
        }
        function onGroupNotice(message) {
            toastBanner.show(message)
        }
    }

    Connections {
        target: platformHelper
        function onFilesPicked(urls) {
            if (urls && urls.length > 0) {
                if (groupManager.activeGroupId.length > 0) {
                    groupManager.sendFiles(urls)
                } else {
                    transferManager.sendFiles(urls)
                }
            }
        }
    }

    // Desktop Wide Two-Pane Layout
    RowLayout {
        anchors.fill: parent
        spacing: 0
        visible: root.isWide

        DeviceListPane {
            Layout.preferredWidth: 280
            Layout.fillHeight: true
            theme: root.theme
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            color: root.theme.divider
        }

        ChatPane {
            Layout.fillWidth: true
            Layout.fillHeight: true
            theme: root.theme
            showBackButton: false
        }
    }

    // Mobile Single-Pane StackView Layout
    StackView {
        id: mobileStack
        anchors.fill: parent
        visible: !root.isWide
        focus: !root.isWide

        Keys.onReleased: function(event) {
            if (event.key === Qt.Key_Back) {
                if (root.handleMobileBack()) {
                    event.accepted = true
                }
            }
        }

        initialItem: DeviceListPane {
            theme: root.theme
        }

        pushEnter: Transition {
            PropertyAnimation { property: "x"; from: mobileStack.width * 0.25; to: 0; duration: 200; easing.type: Easing.OutCubic }
            PropertyAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 180 }
        }
        pushExit: Transition {
            PropertyAnimation { property: "opacity"; from: 1.0; to: 0.85; duration: 180 }
        }
        popEnter: Transition {
            PropertyAnimation { property: "opacity"; from: 0.85; to: 1.0; duration: 180 }
        }
        popExit: Transition {
            PropertyAnimation { property: "x"; from: 0; to: mobileStack.width * 0.25; duration: 180; easing.type: Easing.InCubic }
            PropertyAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 160 }
        }
    }

    Component {
        id: mobileChatComponent
        ChatPane {
            theme: root.theme
            showBackButton: true
            onBackRequested: {
                if (mobileStack.depth > 1) {
                    mobileStack.pop()
                }
                transferManager.clearActivePeer()
                groupManager.clearActiveGroup()
            }
        }
    }

    // Lightweight Incoming Connection Confirmation Modal
    Dialog {
        id: incomingDialog
        modal: true
        dim: true
        anchors.centerIn: parent
        width: Math.min(root.width - 48, 340)
        padding: 24
        closePolicy: Popup.NoAutoClose
        visible: transferManager.hasIncomingRequest

        background: Rectangle {
            radius: 20
            color: root.theme.surfaceBg
            border.width: 1
            border.color: root.theme.divider
        }

        Overlay.modal: Rectangle {
            color: root.isDark ? "#88000000" : "#440F172A"
        }

        contentItem: ColumnLayout {
            spacing: 20

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    Layout.fillWidth: true
                    text: transferManager.incomingPeerName
                    color: root.theme.textPrimary
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("请求建立局域网传输连接")
                    color: root.theme.textMuted
                    font.pixelSize: 13
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    text: qsTr("拒绝")
                    onClicked: transferManager.rejectIncomingRequest(transferManager.incomingPeerId)

                    background: Rectangle {
                        radius: 12
                        color: parent.hovered ? root.theme.divider : root.theme.pageBg
                    }
                    contentItem: Text {
                        text: parent.text
                        color: root.theme.textBody
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    text: qsTr("接受")
                    onClicked: transferManager.acceptIncomingRequest(transferManager.incomingPeerId)

                    background: Rectangle {
                        radius: 12
                        color: parent.hovered ? root.theme.accentHover : root.theme.accent
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#FFFFFF"
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    // Lightweight Incoming Group Join Confirmation Modal
    Dialog {
        id: incomingGroupJoinDialog
        modal: true
        dim: true
        anchors.centerIn: parent
        width: Math.min(root.width - 48, 340)
        padding: 24
        closePolicy: Popup.NoAutoClose
        visible: !transferManager.hasIncomingRequest && groupManager.hasIncomingJoinRequest

        background: Rectangle {
            radius: 20
            color: root.theme.surfaceBg
            border.width: 1
            border.color: root.theme.divider
        }

        Overlay.modal: Rectangle {
            color: root.isDark ? "#88000000" : "#440F172A"
        }

        contentItem: ColumnLayout {
            spacing: 20

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    Layout.fillWidth: true
                    text: groupManager.incomingJoinPeerName
                    color: root.theme.textPrimary
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("请求加入群聊「%1」").arg(groupManager.incomingJoinGroupName)
                    color: root.theme.textMuted
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    text: qsTr("拒绝")
                    onClicked: groupManager.rejectIncomingJoin()

                    background: Rectangle {
                        radius: 12
                        color: parent.hovered ? root.theme.divider : root.theme.pageBg
                    }
                    contentItem: Text {
                        text: parent.text
                        color: root.theme.textBody
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    text: qsTr("同意")
                    onClicked: groupManager.acceptIncomingJoin()

                    background: Rectangle {
                        radius: 12
                        color: parent.hovered ? root.theme.accentHover : root.theme.accent
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#FFFFFF"
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    // Subtle ephemeral toast
    Rectangle {
        id: toastBanner
        property string message: ""
        function show(msg) {
            message = msg
            toastTimer.restart()
            opacity = 1
        }

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 20
        height: 36
        width: toastLabel.implicitWidth + 32
        radius: 18
        color: root.isDark ? "#262B34" : "#1E293B"
        opacity: 0
        visible: opacity > 0
        z: 100

        Behavior on opacity {
            NumberAnimation { duration: 180 }
        }

        Text {
            id: toastLabel
            anchors.centerIn: parent
            text: toastBanner.message
            color: "#FFFFFF"
            font.pixelSize: 13
        }

        Timer {
            id: toastTimer
            interval: 2200
            onTriggered: toastBanner.opacity = 0
        }
    }
}
