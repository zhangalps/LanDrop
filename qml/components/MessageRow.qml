import QtQuick
import QtQuick.Layouts

Item {
    id: rowItem
    property QtObject theme
    property var msgData
    property bool isGroupChat: false

    signal copyBubbleRequested(string text)
    signal imageClicked(string url, string path, string name)
    signal videoClicked(string path)
    signal downloadFileRequested(string msgId)
    signal cancelTransferRequested(string msgId)
    signal openFileRequested(string path)
    signal revealFileRequested(string path)
    signal saveFileRequested(string path, string name)

    readonly property bool isSystem: msgData.kind === "system"
    readonly property bool outgoing: msgData.isOutgoing
    readonly property string msgKind: msgData.kind
    readonly property string senderName: outgoing
                                         ? deviceDiscovery.localDeviceName
                                         : (isGroupChat && msgData.senderName.length > 0
                                            ? msgData.senderName
                                            : transferManager.activePeerName)
    readonly property string senderAvatarSeed: outgoing
                                               ? deviceDiscovery.localDeviceId
                                               : (isGroupChat && msgData.senderId.length > 0
                                                  ? msgData.senderId
                                                  : transferManager.activePeerId)

    height: isSystem ? sysNoticePill.implicitHeight : Math.max(36, bubbleColumn.implicitHeight)

    // System Notice Pill (Member Joined / Left / Disbanded)
    Rectangle {
        id: sysNoticePill
        visible: rowItem.isSystem
        anchors.horizontalCenter: parent.horizontalCenter
        implicitWidth: sysText.implicitWidth + 24
        implicitHeight: 24
        radius: 12
        color: rowItem.theme.badgeBg

        Text {
            id: sysText
            anchors.centerIn: parent
            text: rowItem.msgData.text
            color: rowItem.theme.textMuted
            font.pixelSize: 11
        }
    }

    // Left / Right Avatar
    AvatarBadge {
        visible: !rowItem.isSystem && !rowItem.outgoing
        anchors.left: parent.left
        anchors.top: parent.top
        theme: rowItem.theme
        size: 36
        seed: rowItem.senderAvatarSeed
        labelText: rowItem.senderName
    }

    AvatarBadge {
        visible: !rowItem.isSystem && rowItem.outgoing
        anchors.right: parent.right
        anchors.top: parent.top
        theme: rowItem.theme
        size: 36
        seed: rowItem.senderAvatarSeed
        labelText: rowItem.senderName
    }

    ColumnLayout {
        id: bubbleColumn
        visible: !rowItem.isSystem
        width: Math.min(rowItem.width - 92, rowItem.msgKind === "text" ? 460 : 310)
        anchors.right: rowItem.outgoing ? parent.right : undefined
        anchors.rightMargin: rowItem.outgoing ? 46 : 0
        anchors.left: rowItem.outgoing ? undefined : parent.left
        anchors.leftMargin: rowItem.outgoing ? 0 : 46
        spacing: 4

        Text {
            visible: rowItem.isGroupChat && !rowItem.outgoing && rowItem.senderName.length > 0
            Layout.alignment: Qt.AlignLeft
            text: rowItem.senderName
            color: rowItem.theme.textMuted
            font.pixelSize: 11
        }

        // 1. Text Message Bubble
        Rectangle {
            visible: rowItem.msgKind === "text"
            Layout.alignment: rowItem.outgoing ? Qt.AlignRight : Qt.AlignLeft
            implicitWidth: Math.min(bubbleColumn.width, textContent.implicitWidth + 28)
            implicitHeight: textContent.implicitHeight + 22
            radius: 16
            color: rowItem.outgoing ? rowItem.theme.outgoingBubble : rowItem.theme.incomingBubble
            border.width: rowItem.outgoing ? 0 : 1
            border.color: rowItem.theme.bubbleBorder

            TextEdit {
                id: textContent
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                anchors.topMargin: 11
                anchors.bottomMargin: 11
                text: rowItem.msgData.text
                color: rowItem.outgoing ? rowItem.theme.outgoingText : rowItem.theme.textBody
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
                    if (mouse.button === Qt.RightButton) rowItem.copyBubbleRequested(rowItem.msgData.text)
                    else mouse.accepted = false
                }
                onPressAndHold: rowItem.copyBubbleRequested(rowItem.msgData.text)
            }
        }

        // 2. Image or Video Card
        MediaCard {
            visible: rowItem.msgKind === "image" || rowItem.msgKind === "video"
            Layout.alignment: rowItem.outgoing ? Qt.AlignRight : Qt.AlignLeft
            theme: rowItem.theme
            mediaKind: rowItem.msgKind
            fileUrl: rowItem.msgData.fileUrl
            filePath: rowItem.msgData.filePath
            fileName: rowItem.msgData.fileName
            fileExtension: rowItem.msgData.fileExtension
            fileSizeText: rowItem.msgData.fileSizeText
            outgoing: rowItem.outgoing
            done: rowItem.msgData.status === "completed"
            transferring: rowItem.msgData.status === "transferring"
            progress: rowItem.msgData.progress
            maxWidth: bubbleColumn.width
            onImageClicked: function(u, p, n) { rowItem.imageClicked(u, p, n) }
            onVideoClicked: function(p) { rowItem.videoClicked(p) }
        }

        // 3. Generic File Card
        FileCard {
            visible: rowItem.msgKind === "file"
            Layout.alignment: rowItem.outgoing ? Qt.AlignRight : Qt.AlignLeft
            theme: rowItem.theme
            msgId: rowItem.msgData.msgId
            fileName: rowItem.msgData.fileName
            filePath: rowItem.msgData.filePath
            fileExtension: rowItem.msgData.fileExtension
            fileSizeText: rowItem.msgData.fileSizeText
            status: rowItem.msgData.status
            progress: rowItem.msgData.progress
            outgoing: rowItem.outgoing
            isGroupChat: rowItem.isGroupChat
            maxWidth: bubbleColumn.width
            onDownloadRequested: function(id) { rowItem.downloadFileRequested(id) }
            onCancelRequested: function(id) { rowItem.cancelTransferRequested(id) }
            onOpenRequested: function(p) { rowItem.openFileRequested(p) }
            onRevealRequested: function(p) { rowItem.revealFileRequested(p) }
            onSaveRequested: function(p, n) { rowItem.saveFileRequested(p, n) }
        }

        Text {
            Layout.alignment: rowItem.outgoing ? Qt.AlignRight : Qt.AlignLeft
            text: rowItem.msgData.timestamp
            color: rowItem.theme.textMuted
            font.pixelSize: 11
        }
    }
}
