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

    Theme {
        id: appTheme
        isDark: root.isDark
    }

    color: appTheme.pageBg

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

    onClosing: function(close) {
        if (root.handleMobileBack()) {
            close.accepted = false
        }
    }

    Shortcut {
        sequences: [StandardKey.Back]
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
                if (groupManager.activeGroupId.length > 0) groupManager.sendFiles(urls)
                else transferManager.sendFiles(urls)
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
            theme: appTheme
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            color: appTheme.divider
        }

        ChatPane {
            Layout.fillWidth: true
            Layout.fillHeight: true
            theme: appTheme
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
            if (event.key === Qt.Key_Back && root.handleMobileBack()) {
                event.accepted = true
            }
        }

        initialItem: DeviceListPane {
            theme: appTheme
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
            theme: appTheme
            showBackButton: true
            onBackRequested: {
                if (mobileStack.depth > 1) mobileStack.pop()
                transferManager.clearActivePeer()
                groupManager.clearActiveGroup()
            }
        }
    }

    ConfirmDialog {
        id: incomingDialog
        theme: appTheme
        isDark: root.isDark
        visible: transferManager.hasIncomingRequest
        headingText: transferManager.incomingPeerName
        bodyText: qsTr("请求建立局域网传输连接")
        rejectLabel: qsTr("拒绝")
        acceptLabel: qsTr("接受")
        onRejectedAction: transferManager.rejectIncomingRequest(transferManager.incomingPeerId)
        onAcceptedAction: transferManager.acceptIncomingRequest(transferManager.incomingPeerId)
    }

    ConfirmDialog {
        id: incomingGroupJoinDialog
        theme: appTheme
        isDark: root.isDark
        visible: !transferManager.hasIncomingRequest && groupManager.hasIncomingJoinRequest
        headingText: groupManager.incomingJoinPeerName
        bodyText: qsTr("请求加入群聊「%1」").arg(groupManager.incomingJoinGroupName)
        rejectLabel: qsTr("拒绝")
        acceptLabel: qsTr("同意")
        onRejectedAction: groupManager.rejectIncomingJoin()
        onAcceptedAction: groupManager.acceptIncomingJoin()
    }

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

        Behavior on opacity { NumberAnimation { duration: 180 } }

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
