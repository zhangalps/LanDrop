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
                if (chatPane.isGroupChat) {
                    groupManager.sendFiles(selectedFiles)
                } else {
                    transferManager.sendFiles(selectedFiles)
                }
            }
        }
    }

    // Quiet empty state when no peer is selected on desktop
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

        // Top Session Header (Avatar + Peer Name + Status & IP + Disconnect)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 66
            color: chatPane.theme.chatBarBg

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 16
                spacing: 10

                // Minimalist back "<" button for mobile
                Button {
                    visible: chatPane.showBackButton
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    onClicked: chatPane.backRequested()

                    background: Rectangle {
                        radius: 18
                        color: parent.hovered ? chatPane.theme.pageBg : "transparent"
                    }

                    contentItem: Canvas {
                        id: backCanvas
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.strokeStyle = chatPane.theme.textPrimary
                            ctx.lineWidth = 1.8
                            ctx.lineCap = "round"
                            ctx.lineJoin = "round"
                            ctx.beginPath()
                            ctx.moveTo(width * 0.58, height * 0.30)
                            ctx.lineTo(width * 0.38, height * 0.50)
                            ctx.lineTo(width * 0.58, height * 0.70)
                            ctx.stroke()
                        }
                        Connections {
                            target: chatPane.theme
                            function onTextPrimaryChanged() { backCanvas.requestPaint() }
                        }
                    }
                }

                Item {
                    visible: !chatPane.showBackButton
                    Layout.preferredWidth: 6
                }

                // Avatar with Status Dot (Group Rounded Square vs 1v1 Circle)
                Item {
                    Layout.preferredWidth: 38
                    Layout.preferredHeight: 38

                    Rectangle {
                        anchors.fill: parent
                        radius: chatPane.isGroupChat ? 12 : 19
                        color: chatPane.isGroupChat
                               ? chatPane.theme.avatarBg(groupManager.activeGroupId)
                               : chatPane.theme.avatarBg(transferManager.activePeerId)

                        Text {
                            anchors.centerIn: parent
                            text: chatPane.isGroupChat
                                  ? "#"
                                  : chatPane.theme.avatarInitial(transferManager.activePeerName)
                            color: chatPane.isGroupChat
                                   ? chatPane.theme.avatarFg(groupManager.activeGroupId)
                                   : chatPane.theme.avatarFg(transferManager.activePeerId)
                            font.pixelSize: chatPane.isGroupChat ? 17 : 15
                            font.weight: Font.DemiBold
                        }
                    }

                    Rectangle {
                        width: 11
                        height: 11
                        radius: 5.5
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        color: chatPane.isConnected
                               ? chatPane.theme.onlineDot
                               : (chatPane.isPendingOutgoing ? chatPane.theme.pendingDot : chatPane.theme.textMuted)
                        border.width: 2
                        border.color: chatPane.theme.chatBarBg
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        Layout.fillWidth: true
                        text: chatPane.isGroupChat ? groupManager.activeGroupName : transferManager.activePeerName
                        color: chatPane.theme.textPrimary
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: {
                            if (chatPane.isGroupChat) {
                                return chatPane.isConnected
                                       ? (groupManager.activeGroupMemberCount + qsTr(" 位成员在线"))
                                       : qsTr("群聊已断开")
                            }
                            var st = chatPane.isConnected
                                     ? qsTr("已连接")
                                     : (chatPane.isPendingOutgoing ? qsTr("等待对方确认...") : qsTr("已断开"))
                            return transferManager.activePeerIp.length > 0
                                   ? (st + " · " + transferManager.activePeerIp)
                                   : st
                        }
                        color: chatPane.theme.textMuted
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }

                Button {
                    visible: chatPane.isGroupChat
                    text: qsTr("成员")
                    Layout.preferredHeight: 32
                    leftPadding: 10
                    rightPadding: 10
                    onClicked: membersDialog.open()

                    background: Rectangle {
                        radius: 10
                        color: parent.hovered ? chatPane.theme.pageBg : "transparent"
                    }
                    contentItem: Text {
                        text: parent.text
                        color: chatPane.theme.accent
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Button {
                    visible: chatPane.isGroupChat || chatPane.isConnected || chatPane.isPendingOutgoing
                    text: chatPane.isGroupChat
                          ? (groupManager.activeGroupIsHost ? qsTr("解散") : qsTr("退出"))
                          : qsTr("断开")
                    Layout.preferredHeight: 32
                    leftPadding: 10
                    rightPadding: 10
                    onClicked: {
                        if (chatPane.isGroupChat) {
                            groupManager.leaveOrDisbandActiveGroup()
                            if (chatPane.showBackButton) {
                                chatPane.backRequested()
                            }
                        } else {
                            transferManager.disconnectPeer(transferManager.activePeerId)
                        }
                    }

                    background: Rectangle {
                        radius: 10
                        color: parent.hovered ? chatPane.theme.pageBg : "transparent"
                    }
                    contentItem: Text {
                        text: parent.text
                        color: chatPane.theme.textMuted
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: chatPane.theme.divider
            }
        }

        // Message Stream
        ListView {
            id: messageList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: chatPane.isGroupChat ? groupManager.activeGroupMessageModel : transferManager.activeMessageModel
            spacing: 14
            topMargin: 18
            bottomMargin: 18
            leftMargin: 20
            rightMargin: 20

            onCountChanged: {
                Qt.callLater(function() {
                    if (messageList.count > 0) {
                        messageList.positionViewAtEnd()
                    }
                })
            }

            delegate: Item {
                id: rowItem
                width: messageList.width - messageList.leftMargin - messageList.rightMargin
                height: isSystem ? sysNoticePill.implicitHeight : Math.max(36, bubbleColumn.implicitHeight)

                readonly property bool isSystem: model.kind === "system"
                readonly property bool outgoing: model.isOutgoing
                readonly property string msgKind: model.kind
                readonly property bool availableToDownload: model.status === "available"
                readonly property bool done: model.status === "completed"
                readonly property bool transferring: model.status === "transferring"
                readonly property string senderName: outgoing
                                                     ? deviceDiscovery.localDeviceName
                                                     : (chatPane.isGroupChat && model.senderName.length > 0
                                                        ? model.senderName
                                                        : transferManager.activePeerName)
                readonly property string senderAvatarSeed: outgoing
                                                           ? deviceDiscovery.localDeviceId
                                                           : (chatPane.isGroupChat && model.senderId.length > 0
                                                              ? model.senderId
                                                              : transferManager.activePeerId)

                // System Notification Pill (e.g. Member Joined / Left)
                Rectangle {
                    id: sysNoticePill
                    visible: rowItem.isSystem
                    anchors.horizontalCenter: parent.horizontalCenter
                    implicitWidth: sysText.implicitWidth + 24
                    implicitHeight: 24
                    radius: 12
                    color: chatPane.theme.badgeBg

                    Text {
                        id: sysText
                        anchors.centerIn: parent
                        text: model.text
                        color: chatPane.theme.textMuted
                        font.pixelSize: 11
                    }
                }

                // Incoming Peer Avatar (Left)
                Rectangle {
                    visible: !rowItem.isSystem && !rowItem.outgoing
                    anchors.left: parent.left
                    anchors.top: parent.top
                    width: 36
                    height: 36
                    radius: 18
                    color: chatPane.theme.avatarBg(rowItem.senderAvatarSeed)

                    Text {
                        anchors.centerIn: parent
                        text: chatPane.theme.avatarInitial(rowItem.senderName)
                        color: chatPane.theme.avatarFg(rowItem.senderAvatarSeed)
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }
                }

                // Outgoing Local Avatar (Right)
                Rectangle {
                    visible: !rowItem.isSystem && rowItem.outgoing
                    anchors.right: parent.right
                    anchors.top: parent.top
                    width: 36
                    height: 36
                    radius: 18
                    color: chatPane.theme.avatarBg(rowItem.senderAvatarSeed)

                    Text {
                        anchors.centerIn: parent
                        text: chatPane.theme.avatarInitial(rowItem.senderName)
                        color: chatPane.theme.avatarFg(rowItem.senderAvatarSeed)
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }
                }

                ColumnLayout {
                    id: bubbleColumn
                    visible: !rowItem.isSystem
                    width: Math.min(rowItem.width - 92, msgKind === "text" ? 460 : 310)
                    anchors.right: rowItem.outgoing ? parent.right : undefined
                    anchors.rightMargin: rowItem.outgoing ? 46 : 0
                    anchors.left: rowItem.outgoing ? undefined : parent.left
                    anchors.leftMargin: rowItem.outgoing ? 0 : 46
                    spacing: 4

                    // Sender nickname in Group Chat
                    Text {
                        visible: chatPane.isGroupChat && !rowItem.outgoing && rowItem.senderName.length > 0
                        Layout.alignment: Qt.AlignLeft
                        text: rowItem.senderName
                        color: chatPane.theme.textMuted
                        font.pixelSize: 11
                    }

                    // 1. Text Message Bubble (with right-click / long-press Copy)
                    Rectangle {
                        visible: rowItem.msgKind === "text"
                        Layout.alignment: rowItem.outgoing ? Qt.AlignRight : Qt.AlignLeft
                        implicitWidth: Math.min(bubbleColumn.width, textContent.implicitWidth + 28)
                        implicitHeight: textContent.implicitHeight + 22
                        radius: 16
                        color: rowItem.outgoing ? chatPane.theme.outgoingBubble : chatPane.theme.incomingBubble
                        border.width: rowItem.outgoing ? 0 : 1
                        border.color: chatPane.theme.bubbleBorder

                        TextEdit {
                            id: textContent
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            anchors.topMargin: 11
                            anchors.bottomMargin: 11
                            text: model.text
                            color: rowItem.outgoing ? chatPane.theme.outgoingText : chatPane.theme.textBody
                            font.pixelSize: 14
                            wrapMode: TextEdit.Wrap
                            readOnly: true
                            selectByMouse: !platformHelper.isAndroid
                        }

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.RightButton | (platformHelper.isAndroid ? Qt.LeftButton : Qt.NoButton)
                            propagateComposedEvents: true
                            onClicked: function(mouse) {
                                if (mouse.button === Qt.RightButton) {
                                    bubbleCopyMenu.copyTargetText = model.text
                                    bubbleCopyMenu.popup()
                                } else {
                                    mouse.accepted = false
                                }
                            }
                            onPressAndHold: {
                                bubbleCopyMenu.copyTargetText = model.text
                                bubbleCopyMenu.popup()
                            }
                        }
                    }

                    // 2. Image Media Card (12px rounded corners)
                    Rectangle {
                        visible: rowItem.msgKind === "image"
                        Layout.alignment: rowItem.outgoing ? Qt.AlignRight : Qt.AlignLeft
                        Layout.preferredWidth: Math.min(bubbleColumn.width, 240)
                        Layout.preferredHeight: 180
                        radius: 12
                        color: chatPane.theme.pageBg
                        clip: true

                        Image {
                            id: imgPreview
                            anchors.fill: parent
                            source: (rowItem.done || rowItem.outgoing) ? model.fileUrl : ""
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: false
                            sourceSize.width: 480
                            sourceSize.height: 360
                        }

                        // Subtle progress overlay while transferring
                        Rectangle {
                            visible: rowItem.transferring
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 4
                            color: "#33000000"

                            Rectangle {
                                width: parent.width * model.progress
                                height: parent.height
                                color: chatPane.theme.accent
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            enabled: rowItem.done || rowItem.outgoing
                            cursorShape: Qt.PointingHandCursor
                            onClicked: imageViewer.openImage(model.fileUrl, model.filePath, model.fileName)
                        }
                    }

                    // 3. Video Media Card (12px rounded corners + subtle corner badge)
                    Rectangle {
                        visible: rowItem.msgKind === "video"
                        Layout.alignment: rowItem.outgoing ? Qt.AlignRight : Qt.AlignLeft
                        Layout.preferredWidth: Math.min(bubbleColumn.width, 240)
                        Layout.preferredHeight: 150
                        radius: 12
                        color: "#1E2430"
                        clip: true

                        // Minimalist play triangle
                        Rectangle {
                            anchors.centerIn: parent
                            width: 44
                            height: 44
                            radius: 22
                            color: "#44FFFFFF"

                            Canvas {
                                anchors.fill: parent
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.clearRect(0, 0, width, height)
                                    ctx.fillStyle = "#FFFFFF"
                                    ctx.beginPath()
                                    ctx.moveTo(width * 0.42, height * 0.32)
                                    ctx.lineTo(width * 0.68, height * 0.50)
                                    ctx.lineTo(width * 0.42, height * 0.68)
                                    ctx.closePath()
                                    ctx.fill()
                                }
                            }
                        }

                        // Subtle translucent bottom-left format/size badge
                        Rectangle {
                            anchors.left: parent.left
                            anchors.bottom: parent.bottom
                            anchors.leftMargin: 10
                            anchors.bottomMargin: 10
                            height: 22
                            width: videoBadgeLabel.implicitWidth + 14
                            radius: 6
                            color: "#66000000"

                            Text {
                                id: videoBadgeLabel
                                anchors.centerIn: parent
                                text: model.fileExtension + " · " + model.fileSizeText
                                color: "#F1F5F9"
                                font.pixelSize: 11
                            }
                        }

                        Rectangle {
                            visible: rowItem.transferring
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 3
                            color: "#33FFFFFF"

                            Rectangle {
                                width: parent.width * model.progress
                                height: parent.height
                                color: chatPane.theme.accent
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            enabled: rowItem.done || rowItem.outgoing
                            cursorShape: Qt.PointingHandCursor
                            onClicked: platformHelper.openFile(model.filePath, "video")
                        }
                    }

                    // 4. Generic File Card
                    Rectangle {
                        visible: rowItem.msgKind === "file"
                        Layout.alignment: rowItem.outgoing ? Qt.AlignRight : Qt.AlignLeft
                        Layout.preferredWidth: Math.min(bubbleColumn.width, 300)
                        implicitHeight: fileCardCol.implicitHeight + 24
                        radius: 16
                        color: chatPane.theme.surfaceBg
                        border.width: 1
                        border.color: chatPane.theme.divider

                        ColumnLayout {
                            id: fileCardCol
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 10

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                // Compact extension badge (e.g. PDF / ZIP)
                                Rectangle {
                                    Layout.preferredWidth: 44
                                    Layout.preferredHeight: 44
                                    radius: 12
                                    color: chatPane.theme.badgeBg

                                    Text {
                                        anchors.centerIn: parent
                                        text: model.fileExtension.length > 4
                                              ? model.fileExtension.slice(0, 4)
                                              : model.fileExtension
                                        color: chatPane.theme.accent
                                        font.pixelSize: 11
                                        font.weight: Font.Bold
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3

                                    Text {
                                        Layout.fillWidth: true
                                        text: model.fileName
                                        color: chatPane.theme.textPrimary
                                        font.pixelSize: 14
                                        font.weight: Font.Medium
                                        elide: Text.ElideMiddle
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: {
                                            if (model.status === "available")
                                                return model.fileSizeText + " · " + qsTr("点击下载")
                                            if (model.status === "transferring")
                                                return model.fileSizeText + " · " + Math.round(model.progress * 100) + "%"
                                            if (model.status === "canceled")
                                                return model.fileSizeText + " · " + qsTr("已取消")
                                            if (model.status === "failed")
                                                return model.fileSizeText + " · " + qsTr("传输失败")
                                            return model.fileSizeText
                                        }
                                        color: chatPane.theme.textMuted
                                        font.pixelSize: 12
                                    }
                                }
                            }

                            // 3px rounded progress bar while transferring
                            Rectangle {
                                visible: rowItem.transferring
                                Layout.fillWidth: true
                                Layout.preferredHeight: 3
                                radius: 1.5
                                color: chatPane.theme.divider

                                Rectangle {
                                    width: parent.width * model.progress
                                    height: parent.height
                                    radius: 1.5
                                    color: chatPane.theme.accent
                                }
                            }

                            // Action row when available (Download), transferring (Cancel), or completed (Open / Reveal)
                            RowLayout {
                                visible: rowItem.availableToDownload || rowItem.transferring || rowItem.done
                                Layout.fillWidth: true
                                spacing: 8

                                Item { Layout.fillWidth: true }

                                Button {
                                    visible: rowItem.availableToDownload && chatPane.isGroupChat
                                    text: qsTr("下载")
                                    Layout.preferredHeight: 28
                                    leftPadding: 12
                                    rightPadding: 12
                                    onClicked: groupManager.downloadGroupFile(model.msgId)

                                    background: Rectangle {
                                        radius: 8
                                        color: parent.hovered ? chatPane.theme.divider : chatPane.theme.accentSoft
                                    }
                                    contentItem: Text {
                                        text: parent.text
                                        color: chatPane.theme.accent
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }

                                Button {
                                    visible: rowItem.transferring
                                    text: qsTr("取消")
                                    Layout.preferredHeight: 28
                                    leftPadding: 10
                                    rightPadding: 10
                                    onClicked: transferManager.cancelTransfer(model.msgId)

                                    background: Rectangle {
                                        radius: 8
                                        color: parent.hovered ? chatPane.theme.pageBg : "transparent"
                                    }
                                    contentItem: Text {
                                        text: parent.text
                                        color: chatPane.theme.textMuted
                                        font.pixelSize: 12
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }

                                Button {
                                    visible: rowItem.done && !rowItem.outgoing && platformHelper.isWindows
                                    text: qsTr("打开文件夹")
                                    Layout.preferredHeight: 28
                                    leftPadding: 10
                                    rightPadding: 10
                                    onClicked: platformHelper.revealInFolder(model.filePath)

                                    background: Rectangle {
                                        radius: 8
                                        color: parent.hovered ? chatPane.theme.divider : chatPane.theme.pageBg
                                    }
                                    contentItem: Text {
                                        text: parent.text
                                        color: chatPane.theme.textBody
                                        font.pixelSize: 12
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }

                                Button {
                                    visible: rowItem.done && !rowItem.outgoing && platformHelper.isAndroid
                                    text: qsTr("保存")
                                    Layout.preferredHeight: 28
                                    leftPadding: 10
                                    rightPadding: 10
                                    onClicked: {
                                        var saved = platformHelper.saveToPublicStorage(model.filePath, model.fileName, "file")
                                        chatToast.show(saved.length > 0 ? qsTr("已保存到 Download/LanDrop") : qsTr("保存失败"))
                                    }

                                    background: Rectangle {
                                        radius: 8
                                        color: parent.hovered ? chatPane.theme.divider : chatPane.theme.pageBg
                                    }
                                    contentItem: Text {
                                        text: parent.text
                                        color: chatPane.theme.textBody
                                        font.pixelSize: 12
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }

                                Button {
                                    visible: rowItem.done
                                    text: qsTr("打开")
                                    Layout.preferredHeight: 28
                                    leftPadding: 12
                                    rightPadding: 12
                                    onClicked: platformHelper.openFile(model.filePath, "file")

                                    background: Rectangle {
                                        radius: 8
                                        color: parent.hovered ? chatPane.theme.divider : chatPane.theme.accentSoft
                                    }
                                    contentItem: Text {
                                        text: parent.text
                                        color: chatPane.theme.accent
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }
                            }
                        }
                    }

                    // Timestamp
                    Text {
                        Layout.alignment: rowItem.outgoing ? Qt.AlignRight : Qt.AlignLeft
                        text: model.timestamp
                        color: chatPane.theme.textMuted
                        font.pixelSize: 11
                    }
                }
            }
        }

        // Bottom Input Bar (white surface + full-area touch target + mobile safe bottom margin)
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: inputRow.implicitHeight + (platformHelper.isAndroid ? 30 : 22)
            color: chatPane.theme.chatBarBg

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 1
                color: chatPane.theme.divider
            }

            RowLayout {
                id: inputRow
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                anchors.topMargin: 11
                spacing: 10

                // Minimalist "+" Attachment Button
                Button {
                    enabled: chatPane.isConnected
                    Layout.preferredWidth: 44
                    Layout.preferredHeight: 44
                    Layout.alignment: Qt.AlignBottom
                    onClicked: {
                        if (platformHelper.isAndroid) {
                            platformHelper.pickFiles()
                        } else {
                            filePicker.open()
                        }
                    }

                    background: Rectangle {
                        radius: 22
                        color: parent.hovered ? chatPane.theme.divider : chatPane.theme.inputBg
                    }

                    contentItem: Canvas {
                        id: plusCanvas
                        opacity: chatPane.isConnected ? 1.0 : 0.4
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.strokeStyle = chatPane.theme.textPrimary
                            ctx.lineWidth = 1.8
                            ctx.lineCap = "round"
                            ctx.beginPath()
                            ctx.moveTo(width * 0.50, height * 0.30)
                            ctx.lineTo(width * 0.50, height * 0.70)
                            ctx.moveTo(width * 0.30, height * 0.50)
                            ctx.lineTo(width * 0.70, height * 0.50)
                            ctx.stroke()
                        }
                        Connections {
                            target: chatPane.theme
                            function onTextPrimaryChanged() { plusCanvas.requestPaint() }
                        }
                    }
                }

                // Multi-line Input Box (100% clickable area + Copy/Paste menu)
                Rectangle {
                    id: inputCapsule
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(116, Math.max(44, inputArea.implicitHeight + 18))
                    radius: 22
                    color: chatPane.theme.inputBg
                    border.width: inputArea.activeFocus ? 1.2 : 1
                    border.color: inputArea.activeFocus ? chatPane.theme.accent : chatPane.theme.divider

                    // Ensure tapping anywhere inside the capsule (including top/bottom padding) focuses the editor
                    MouseArea {
                        anchors.fill: parent
                        enabled: chatPane.isConnected
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        cursorShape: Qt.IBeamCursor
                        onClicked: function(mouse) {
                            inputArea.forceActiveFocus()
                            if (mouse.button === Qt.RightButton) {
                                inputActionBar.open()
                            } else if (platformHelper.isAndroid) {
                                Qt.inputMethod.show()
                            }
                        }
                        onPressAndHold: {
                            inputArea.forceActiveFocus()
                            inputActionBar.open()
                        }
                    }

                    // Non-focus-stealing floating pill bar above input (keeps Android soft keyboard open!)
                    Popup {
                        id: inputActionBar
                        x: Math.max(0, Math.min(inputCapsule.width - width, 8))
                        y: -height - 8
                        padding: 4
                        modal: false
                        focus: false
                        closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape

                        background: Rectangle {
                            radius: 14
                            color: chatPane.theme.surfaceBg
                            border.width: 1
                            border.color: chatPane.theme.divider
                        }

                        contentItem: Row {
                            spacing: 2

                            Button {
                                text: qsTr("粘贴")
                                height: 34
                                leftPadding: 14
                                rightPadding: 14
                                focusPolicy: Qt.NoFocus
                                onClicked: {
                                    var clip = platformHelper.clipboardText()
                                    if (clip && clip.length > 0) {
                                        inputArea.insert(inputArea.cursorPosition, clip)
                                    }
                                    inputActionBar.close()
                                }
                                background: Rectangle {
                                    radius: 10
                                    color: parent.hovered ? chatPane.theme.pageBg : "transparent"
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: chatPane.theme.accent
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            Rectangle {
                                width: 1
                                height: 16
                                anchors.verticalCenter: parent.verticalCenter
                                color: chatPane.theme.divider
                            }

                            Button {
                                text: qsTr("复制")
                                height: 34
                                leftPadding: 14
                                rightPadding: 14
                                focusPolicy: Qt.NoFocus
                                enabled: inputArea.text.length > 0
                                onClicked: {
                                    var toCopy = inputArea.selectedText.length > 0 ? inputArea.selectedText : inputArea.text
                                    platformHelper.copyToClipboard(toCopy)
                                    chatToast.show(qsTr("已复制"))
                                    inputActionBar.close()
                                }
                                background: Rectangle {
                                    radius: 10
                                    color: parent.hovered ? chatPane.theme.pageBg : "transparent"
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: parent.enabled ? chatPane.theme.textPrimary : chatPane.theme.textMuted
                                    font.pixelSize: 13
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            Rectangle {
                                width: 1
                                height: 16
                                anchors.verticalCenter: parent.verticalCenter
                                color: chatPane.theme.divider
                            }

                            Button {
                                text: qsTr("全选")
                                height: 34
                                leftPadding: 14
                                rightPadding: 14
                                focusPolicy: Qt.NoFocus
                                enabled: inputArea.text.length > 0
                                onClicked: {
                                    inputArea.selectAll()
                                    inputActionBar.close()
                                }
                                background: Rectangle {
                                    radius: 10
                                    color: parent.hovered ? chatPane.theme.pageBg : "transparent"
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: parent.enabled ? chatPane.theme.textPrimary : chatPane.theme.textMuted
                                    font.pixelSize: 13
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }

                            Rectangle {
                                width: 1
                                height: 16
                                anchors.verticalCenter: parent.verticalCenter
                                color: chatPane.theme.divider
                            }

                            Button {
                                text: qsTr("清空")
                                height: 34
                                leftPadding: 14
                                rightPadding: 14
                                focusPolicy: Qt.NoFocus
                                enabled: inputArea.text.length > 0
                                onClicked: {
                                    inputArea.clear()
                                    inputActionBar.close()
                                }
                                background: Rectangle {
                                    radius: 10
                                    color: parent.hovered ? chatPane.theme.pageBg : "transparent"
                                }
                                contentItem: Text {
                                    text: parent.text
                                    color: parent.enabled ? chatPane.theme.dangerText : chatPane.theme.textMuted
                                    font.pixelSize: 13
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
                    }

                    Flickable {
                        id: inputFlick
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        anchors.topMargin: 6
                        anchors.bottomMargin: 6
                        contentWidth: width
                        contentHeight: Math.max(height, inputArea.implicitHeight)
                        clip: true

                        TextArea {
                            id: inputArea
                            width: inputFlick.width
                            height: Math.max(inputFlick.height, implicitHeight)
                            enabled: chatPane.isConnected
                            color: chatPane.theme.textPrimary
                            placeholderText: chatPane.isConnected ? qsTr("输入消息") : ""
                            placeholderTextColor: chatPane.theme.textMuted
                            font.pixelSize: 14
                            wrapMode: TextArea.Wrap
                            verticalAlignment: TextEdit.AlignVCenter
                            selectByMouse: !platformHelper.isAndroid
                            background: null
                            padding: 0

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.RightButton | (platformHelper.isAndroid ? Qt.LeftButton : Qt.NoButton)
                                propagateComposedEvents: true
                                onClicked: function(mouse) {
                                    if (mouse.button === Qt.RightButton) {
                                        inputArea.forceActiveFocus()
                                        inputActionBar.open()
                                    } else {
                                        mouse.accepted = false
                                    }
                                }
                                onPressAndHold: {
                                    inputArea.forceActiveFocus()
                                    inputActionBar.open()
                                }
                            }

                            Keys.onReturnPressed: function(event) {
                                if (event.modifiers & Qt.ShiftModifier) {
                                    event.accepted = false
                                } else {
                                    event.accepted = true
                                    if (inputArea.text.trim().length > 0) {
                                        if (chatPane.isGroupChat) {
                                            groupManager.sendText(inputArea.text)
                                        } else {
                                            transferManager.sendText(inputArea.text)
                                        }
                                        inputArea.clear()
                                    }
                                }
                            }
                        }
                    }
                }

                // Send Button
                Button {
                    enabled: chatPane.isConnected && inputArea.text.trim().length > 0
                    Layout.preferredWidth: 68
                    Layout.preferredHeight: 44
                    Layout.alignment: Qt.AlignBottom
                    text: qsTr("发送")
                    onClicked: {
                        if (chatPane.isGroupChat) {
                            groupManager.sendText(inputArea.text)
                        } else {
                            transferManager.sendText(inputArea.text)
                        }
                        inputArea.clear()
                    }

                    background: Rectangle {
                        radius: 22
                        color: !parent.enabled
                               ? chatPane.theme.divider
                               : (parent.hovered ? chatPane.theme.accentHover : chatPane.theme.accent)
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? "#FFFFFF" : chatPane.theme.textMuted
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    // Minimalist Copy Menu for Text Bubbles (non-focus-stealing)
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
                text: parent.text
                color: chatPane.theme.textPrimary
                font.pixelSize: 14
                verticalAlignment: Text.AlignVCenter
                leftPadding: 10
            }
            background: Rectangle {
                radius: 8
                color: parent.highlighted ? chatPane.theme.pageBg : "transparent"
            }
        }
    }

    // Desktop Drag & Drop File Support
    DropArea {
        id: fileDropArea
        anchors.fill: parent
        enabled: chatPane.hasActivePeer && chatPane.isConnected
        onDropped: function(drop) {
            if (drop.hasUrls && drop.urls.length > 0) {
                if (chatPane.isGroupChat) {
                    groupManager.sendFiles(drop.urls)
                } else {
                    transferManager.sendFiles(drop.urls)
                }
                drop.acceptProposedAction()
            }
        }
    }

    // Group Members Drawer / Modal
    Dialog {
        id: membersDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min((parent ? parent.width : chatPane.width) - 48, 360)
        modal: true
        focus: true
        padding: 20

        background: Rectangle {
            radius: 20
            color: chatPane.theme.surfaceBg
            border.width: 1
            border.color: chatPane.theme.divider
        }

        Overlay.modal: Rectangle { color: "#440F172A" }

        contentItem: ColumnLayout {
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: qsTr("群成员 (%1)").arg(groupManager.activeGroupMemberCount)
                    color: chatPane.theme.textPrimary
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }
                Button {
                    text: qsTr("关闭")
                    Layout.preferredHeight: 28
                    leftPadding: 10
                    rightPadding: 10
                    onClicked: membersDialog.close()
                    background: Rectangle {
                        radius: 8
                        color: parent.hovered ? chatPane.theme.pageBg : "transparent"
                    }
                    contentItem: Text {
                        text: parent.text
                        color: chatPane.theme.textMuted
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(280, Math.max(60, contentHeight))
                clip: true
                model: groupManager.activeGroupMemberModel
                spacing: 8

                delegate: RowLayout {
                    width: ListView.view.width
                    height: 44
                    spacing: 10

                    Rectangle {
                        Layout.preferredWidth: 34
                        Layout.preferredHeight: 34
                        radius: 17
                        color: chatPane.theme.avatarBg(model.deviceId)

                        Text {
                            anchors.centerIn: parent
                            text: chatPane.theme.avatarInitial(model.deviceName)
                            color: chatPane.theme.avatarFg(model.deviceId)
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1

                        Text {
                            Layout.fillWidth: true
                            text: model.deviceName
                            color: chatPane.theme.textPrimary
                            font.pixelSize: 14
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                        }

                        Text {
                            Layout.fillWidth: true
                            text: model.ipAddress
                            color: chatPane.theme.textMuted
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }

                    Rectangle {
                        visible: model.isHost
                        implicitWidth: hostTag.implicitWidth + 12
                        height: 20
                        radius: 6
                        color: chatPane.theme.accentSoft

                        Text {
                            id: hostTag
                            anchors.centerIn: parent
                            text: qsTr("群主")
                            color: chatPane.theme.accent
                            font.pixelSize: 11
                            font.weight: Font.Medium
                        }
                    }
                }
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

        Behavior on opacity {
            NumberAnimation { duration: 120 }
        }

        Text {
            anchors.centerIn: parent
            text: qsTr("松开以发送文件")
            color: chatPane.theme.accent
            font.pixelSize: 15
            font.weight: Font.Medium
        }
    }

    // Built-in Fullscreen Image Lightbox (Zoom + Save to Gallery / Open Folder)
    Popup {
        id: imageViewer
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: parent ? parent.width : chatPane.width
        height: parent ? parent.height : chatPane.height
        modal: true
        focus: true
        padding: 0
        closePolicy: Popup.CloseOnEscape

        property string imageUrl: ""
        property string localPath: ""
        property string imageName: ""

        function openImage(url, path, name) {
            imageUrl = url
            localPath = path
            imageName = name
            previewImg.scale = 1.0
            previewImg.x = 0
            previewImg.y = 0
            open()
        }

        background: Rectangle {
            color: "#F00D1017"
        }

        contentItem: Item {
            anchors.fill: parent

            // Tap dark backdrop to dismiss
            MouseArea {
                anchors.fill: parent
                onClicked: imageViewer.close()
            }

            // Interactive zoomable / pannable image canvas
            Item {
                id: canvasContainer
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: bottomActionBar.top
                anchors.topMargin: platformHelper.isAndroid ? 36 : 16
                clip: true

                Image {
                    id: previewImg
                    anchors.centerIn: parent
                    width: canvasContainer.width - 24
                    height: canvasContainer.height - 24
                    source: imageViewer.imageUrl
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    smooth: true
                    transformOrigin: Item.Center

                    PinchHandler {
                        target: previewImg
                        minimumScale: 1.0
                        maximumScale: 4.0
                        xAxis.enabled: true
                        yAxis.enabled: true
                    }

                    DragHandler {
                        target: previewImg
                        enabled: previewImg.scale > 1.01
                    }

                    TapHandler {
                        onDoubleTapped: {
                            if (previewImg.scale > 1.2) {
                                previewImg.scale = 1.0
                                previewImg.x = 0
                                previewImg.y = 0
                            } else {
                                previewImg.scale = 2.2
                            }
                        }
                    }

                    WheelHandler {
                        onWheel: function(event) {
                            var factor = event.angleDelta.y > 0 ? 1.15 : 0.87
                            var next = Math.max(1.0, Math.min(4.0, previewImg.scale * factor))
                            previewImg.scale = next
                            if (next <= 1.01) {
                                previewImg.x = 0
                                previewImg.y = 0
                            }
                        }
                    }
                }
            }

            // Unified Bottom Bar: Filename + Action Buttons (avoids Android top status bar)
            Rectangle {
                id: bottomActionBar
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: platformHelper.isAndroid ? 112 : 92
                color: "#99000000"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    anchors.topMargin: 12
                    anchors.bottomMargin: platformHelper.isAndroid ? 26 : 14
                    spacing: 12

                    Text {
                        Layout.fillWidth: true
                        text: imageViewer.imageName
                        color: "#CBD5E1"
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideMiddle
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 14

                        Item { Layout.fillWidth: true }

                        Button {
                            visible: platformHelper.isWindows
                            text: qsTr("打开文件夹")
                            Layout.preferredHeight: 38
                            leftPadding: 18
                            rightPadding: 18
                            onClicked: platformHelper.revealInFolder(imageViewer.localPath)

                            background: Rectangle {
                                radius: 19
                                color: parent.hovered ? "#44FFFFFF" : "#24FFFFFF"
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "#FFFFFF"
                                font.pixelSize: 13
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        Button {
                            visible: platformHelper.isAndroid
                            text: qsTr("保存到相册")
                            Layout.preferredHeight: 38
                            leftPadding: 20
                            rightPadding: 20
                            onClicked: {
                                var saved = platformHelper.saveToPublicStorage(
                                    imageViewer.localPath, imageViewer.imageName, "image")
                                chatToast.show(saved.length > 0 ? qsTr("已保存到相册") : qsTr("保存失败"))
                            }

                            background: Rectangle {
                                radius: 19
                                color: parent.hovered ? chatPane.theme.accentHover : chatPane.theme.accent
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "#FFFFFF"
                                font.pixelSize: 13
                                font.weight: Font.Medium
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        Button {
                            text: qsTr("关闭")
                            Layout.preferredHeight: 38
                            leftPadding: 22
                            rightPadding: 22
                            onClicked: imageViewer.close()

                            background: Rectangle {
                                radius: 19
                                color: parent.hovered ? "#44FFFFFF" : "#24FFFFFF"
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "#FFFFFF"
                                font.pixelSize: 13
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }
    }

    // Lightweight feedback toast (above fullscreen overlay)
    Rectangle {
        id: chatToast
        parent: Overlay.overlay
        property string message: ""
        function show(msg) {
            message = msg
            chatToastTimer.restart()
            opacity = 1
        }

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

        Behavior on opacity {
            NumberAnimation { duration: 160 }
        }

        Text {
            id: chatToastText
            anchors.centerIn: parent
            text: chatToast.message
            color: "#FFFFFF"
            font.pixelSize: 13
        }

        Timer {
            id: chatToastTimer
            interval: 2200
            onTriggered: chatToast.opacity = 0
        }
    }
}
