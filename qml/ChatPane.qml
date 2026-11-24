import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

Rectangle {
    id: chatPane
    required property QtObject theme
    property bool showBackButton: false

    signal backRequested()

    readonly property bool isGroupChat: groupManager.activeGroupId.length > 0
    readonly property bool hasActivePeer: isGroupChat || transferManager.activePeerId.length > 0
    readonly property bool isConnected: isGroupChat
                                        ? groupManager.activeGroupIsJoined
                                        : (transferManager.activePeerState === 3)
    readonly property bool isPendingOutgoing: !isGroupChat && transferManager.activePeerState === 1

    color: theme.chatBg

    FileDialog {
        id: filePicker
        fileMode: FileDialog.OpenFiles
        onAccepted: {
            if (selectedFiles.length > 0) {
                if (chatPane.isGroupChat) groupManager.sendFiles(selectedFiles)
                else transferManager.sendFiles(selectedFiles)
            }
        }
    }

    Item {
        anchors.fill: parent
        visible: !chatPane.hasActivePeer

        Text {
            anchors.centerIn: parent
            text: "LanDrop"
            color: chatPane.theme.divider
            font.pixelSize: 24
            font.weight: Font.Medium
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        visible: chatPane.hasActivePeer

        ChatHeaderBar {
            Layout.fillWidth: true
            theme: chatPane.theme
            showBackButton: chatPane.showBackButton
            isGroupChat: chatPane.isGroupChat
            isConnected: chatPane.isConnected
            isPendingOutgoing: chatPane.isPendingOutgoing
            isGroupHost: groupManager.activeGroupIsHost
            avatarSeed: chatPane.isGroupChat ? groupManager.activeGroupId : transferManager.activePeerId
            titleText: chatPane.isGroupChat ? groupManager.activeGroupName : transferManager.activePeerName
            subtitleText: {
                if (chatPane.isGroupChat) {
                    return chatPane.isConnected
                           ? (groupManager.activeGroupMemberCount + qsTr(" 位成员在线"))
                           : qsTr("群聊已断开")
                }
                var st = chatPane.isConnected
                         ? qsTr("已连接")
                         : (chatPane.isPendingOutgoing ? qsTr("等待对方确认...") : qsTr("已断开"))
                return transferManager.activePeerIp.length > 0 ? (st + " · " + transferManager.activePeerIp) : st
            }
            onBackClicked: chatPane.backRequested()
            onMembersClicked: membersDialog.open()
            onLeaveOrDisconnectClicked: {
                if (chatPane.isGroupChat) {
                    groupManager.leaveOrDisbandActiveGroup()
                    if (chatPane.showBackButton) chatPane.backRequested()
                } else {
                    transferManager.disconnectPeer(transferManager.activePeerId)
                }
            }
        }

        ListView {
            id: messageList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: chatPane.isGroupChat ? groupManager.activeGroupMessageModel : transferManager.activeMessageModel
            spacing: 14
            topMargin: 18; bottomMargin: 18
            leftMargin: 20; rightMargin: 20

            onCountChanged: Qt.callLater(function() {
                if (messageList.count > 0) messageList.positionViewAtEnd()
            })

            delegate: MessageRow {
                width: messageList.width - messageList.leftMargin - messageList.rightMargin
                theme: chatPane.theme
                msgData: model
                isGroupChat: chatPane.isGroupChat
                onCopyBubbleRequested: function(txt) {
                    bubbleCopyMenu.copyTargetText = txt
                    bubbleCopyMenu.popup()
                }
                onImageClicked: function(u, p, n) { imageViewer.openImage(u, p, n) }
                onVideoClicked: function(p) { platformHelper.openFile(p, "video") }
                onDownloadFileRequested: function(id) { groupManager.downloadGroupFile(id) }
                onCancelTransferRequested: function(id) { transferManager.cancelTransfer(id) }
                onOpenFileRequested: function(p) { platformHelper.openFile(p, "file") }
                onRevealFileRequested: function(p) { platformHelper.revealInFolder(p) }
                onSaveFileRequested: function(p, n) {
                    var saved = platformHelper.saveToPublicStorage(p, n, "file")
                    chatToast.show(saved.length > 0 ? qsTr("已保存到 Download/LanDrop") : qsTr("保存失败"))
                }
            }
        }

        ChatInputBar {
            Layout.fillWidth: true
            theme: chatPane.theme
            connected: chatPane.isConnected
            onAttachClicked: {
                if (platformHelper.isAndroid) platformHelper.pickFiles()
                else filePicker.open()
            }
            onSendTextRequested: function(text) {
                if (chatPane.isGroupChat) groupManager.sendText(text)
                else transferManager.sendText(text)
            }
            onToastRequested: function(msg) { chatToast.show(msg) }
        }
    }

    Menu {
        id: bubbleCopyMenu
        property string copyTargetText: ""
        padding: 6
        focus: false
        modal: false

        background: Rectangle {
            implicitWidth: 112
            radius: 14
            color: chatPane.theme.surfaceBg
            border.width: 1
            border.color: chatPane.theme.divider
        }

        MenuItem {
            text: qsTr("复制")
            focusPolicy: Qt.NoFocus
            onTriggered: {
                platformHelper.copyToClipboard(bubbleCopyMenu.copyTargetText)
                chatToast.show(qsTr("已复制"))
            }
            contentItem: Text {
                text: parent.text; color: chatPane.theme.textPrimary; font.pixelSize: 14
                verticalAlignment: Text.AlignVCenter; leftPadding: 10
            }
            background: Rectangle { radius: 8; color: parent.highlighted ? chatPane.theme.pageBg : "transparent" }
        }
    }

    DropArea {
        id: fileDropArea
        anchors.fill: parent
        enabled: chatPane.hasActivePeer && chatPane.isConnected
        onDropped: function(drop) {
            if (drop.hasUrls && drop.urls.length > 0) {
                if (chatPane.isGroupChat) groupManager.sendFiles(drop.urls)
                else transferManager.sendFiles(drop.urls)
                drop.acceptProposedAction()
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 12
        radius: 16
        color: chatPane.theme.accentSoft
        opacity: fileDropArea.containsDrag ? 0.85 : 0.0
        visible: opacity > 0
        border.width: 1.5
        border.color: chatPane.theme.accent
        Behavior on opacity { NumberAnimation { duration: 120 } }

        Text {
            anchors.centerIn: parent
            text: qsTr("松开以发送文件")
            color: chatPane.theme.accent
            font.pixelSize: 15
            font.weight: Font.Medium
        }
    }

    GroupMembersDialog { id: membersDialog; theme: chatPane.theme }
    ImageLightbox { id: imageViewer; theme: chatPane.theme; onToastRequested: function(msg) { chatToast.show(msg) } }

    Rectangle {
        id: chatToast
        parent: Overlay.overlay
        property string message: ""
        function show(msg) { message = msg; chatToastTimer.restart(); opacity = 1 }

        anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
        anchors.top: parent ? parent.top : undefined
        anchors.topMargin: 72
        height: 34
        width: chatToastText.implicitWidth + 28
        radius: 17
        color: "#DD1E293B"
        opacity: 0
        visible: opacity > 0
        z: 999

        Behavior on opacity { NumberAnimation { duration: 160 } }
        Text { id: chatToastText; anchors.centerIn: parent; text: chatToast.message; color: "#FFFFFF"; font.pixelSize: 13 }
        Timer { id: chatToastTimer; interval: 2200; onTriggered: chatToast.opacity = 0 }
    }
}
