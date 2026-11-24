import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: pane
    required property QtObject theme

    color: theme.pageBg

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Local Device Header (Avatar + Online Dot + Editable Name + Local IP)
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 78

            Rectangle {
                anchors.fill: parent
                anchors.margins: 8
                radius: 16
                color: headerMouse.containsMouse ? pane.theme.surfaceBg : "transparent"

                Behavior on color { ColorAnimation { duration: 120 } }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 12

                    // Local Device Monogram Avatar (seeded by unique deviceId)
                    Item {
                        Layout.preferredWidth: 42
                        Layout.preferredHeight: 42

                        Rectangle {
                            anchors.fill: parent
                            radius: 21
                            color: pane.theme.avatarBg(deviceDiscovery.localDeviceId)

                            Text {
                                anchors.centerIn: parent
                                text: pane.theme.avatarInitial(deviceDiscovery.localDeviceName)
                                color: pane.theme.avatarFg(deviceDiscovery.localDeviceId)
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                            }
                        }

                        Rectangle {
                            width: 12
                            height: 12
                            radius: 6
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            color: deviceDiscovery.online ? pane.theme.onlineDot : pane.theme.textMuted
                            border.width: 2
                            border.color: pane.theme.pageBg
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Text {
                                Layout.fillWidth: true
                                text: deviceDiscovery.localDeviceName
                                color: pane.theme.textPrimary
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }

                            Text {
                                text: qsTr("改名")
                                color: pane.theme.accent
                                font.pixelSize: 12
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: {
                                if (deviceDiscovery.localIpAddress.length === 0)
                                    return qsTr("未获取局域网 IP")
                                return transferManager.serverPort === 45455
                                       ? deviceDiscovery.localIpAddress
                                       : (deviceDiscovery.localIpAddress + ":" + transferManager.serverPort)
                            }
                            color: pane.theme.textMuted
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                    }
                }

                MouseArea {
                    id: headerMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: renameDialog.open()
                }
            }
        }

        // Scrollable Groups + Nearby Devices
        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: width
            contentHeight: listContent.implicitHeight + 24

            ColumnLayout {
                id: listContent
                width: parent.width
                spacing: 0

                // === Group Chat Section ===
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 14

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("群聊")
                            color: pane.theme.textMuted
                            font.pixelSize: 12
                            font.weight: Font.Medium
                        }

                        Button {
                            text: qsTr("+ 新建群聊")
                            Layout.preferredHeight: 26
                            leftPadding: 10
                            rightPadding: 10
                            onClicked: createGroupDialog.open()

                            background: Rectangle {
                                radius: 8
                                color: parent.hovered ? pane.theme.divider : "transparent"
                            }
                            contentItem: Text {
                                text: parent.text
                                color: pane.theme.accent
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }

                Repeater {
                    model: deviceDiscovery.groupModel

                    delegate: Rectangle {
                        id: groupCard
                        Layout.fillWidth: true
                        Layout.leftMargin: 16
                        Layout.rightMargin: 16
                        Layout.topMargin: 4
                        Layout.bottomMargin: 6
                        Layout.preferredHeight: 68
                        radius: 16

                        readonly property bool isSelected: groupManager.activeGroupId === model.groupId

                        color: isSelected ? pane.theme.accentSoft : pane.theme.surfaceBg
                        border.width: 1
                        border.color: isSelected ? pane.theme.accent : pane.theme.divider

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 16
                            spacing: 12

                            // Group Rounded Square Avatar
                            Rectangle {
                                Layout.preferredWidth: 42
                                Layout.preferredHeight: 42
                                radius: 13
                                color: pane.theme.avatarBg(model.groupId)

                                Text {
                                    anchors.centerIn: parent
                                    text: "#"
                                    color: pane.theme.avatarFg(model.groupId)
                                    font.pixelSize: 18
                                    font.weight: Font.Bold
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4

                                Text {
                                    Layout.fillWidth: true
                                    text: model.groupName
                                    color: pane.theme.textPrimary
                                    font.pixelSize: 15
                                    font.weight: Font.Medium
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: {
                                        var base = model.memberCount + qsTr(" 人")
                                        if (model.isHost) return qsTr("我创建的") + " · " + base
                                        if (model.isJoined) return qsTr("已加入") + " · " + base
                                        return (model.hasPin ? qsTr("需口令 · ") : "") + model.hostName
                                    }
                                    color: (model.isJoined || model.isHost) ? pane.theme.onlineDot : pane.theme.textMuted
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                            }

                            Rectangle {
                                visible: model.unreadCount > 0
                                Layout.preferredWidth: Math.max(20, grpUnreadText.implicitWidth + 10)
                                Layout.preferredHeight: 20
                                radius: 10
                                color: pane.theme.accent

                                Text {
                                    id: grpUnreadText
                                    anchors.centerIn: parent
                                    text: model.unreadCount > 99 ? "99+" : String(model.unreadCount)
                                    color: "#FFFFFF"
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                }
                            }

                            Text {
                                visible: model.unreadCount === 0 && !model.isJoined && !model.isHost
                                text: qsTr("加入")
                                color: pane.theme.accent
                                font.pixelSize: 12
                                font.weight: Font.Medium
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (model.isJoined || model.isHost) {
                                    groupManager.selectGroup(model.groupId)
                                } else if (model.hasPin) {
                                    joinPinDialog.openForGroup(
                                        model.groupId, model.groupName,
                                        model.hostId, model.hostName,
                                        model.hostIp, model.hostPort)
                                } else {
                                    groupManager.joinGroup(
                                        model.groupId, model.groupName,
                                        model.hostId, model.hostName,
                                        model.hostIp, model.hostPort, "")
                                }
                            }
                        }
                    }
                }

                // === Nearby Devices (1v1 Personal Chat) Section ===
                Item {
                    Layout.fillWidth: true
                    Layout.topMargin: deviceDiscovery.groupModel.count > 0 ? 8 : 0
                    Layout.preferredHeight: 34

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 14

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("附近设备")
                            color: pane.theme.textMuted
                            font.pixelSize: 12
                            font.weight: Font.Medium
                        }

                        Button {
                            text: qsTr("手动连接")
                            Layout.preferredHeight: 26
                            leftPadding: 10
                            rightPadding: 10
                            onClicked: directConnectDialog.open()

                            background: Rectangle {
                                radius: 8
                                color: parent.hovered ? pane.theme.divider : "transparent"
                            }
                            contentItem: Text {
                                text: parent.text
                                color: pane.theme.accent
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }

                Repeater {
                    model: deviceDiscovery

                    delegate: Rectangle {
                        id: card
                        Layout.fillWidth: true
                        Layout.leftMargin: 16
                        Layout.rightMargin: 16
                        Layout.topMargin: 4
                        Layout.bottomMargin: 6
                        Layout.preferredHeight: 70
                        radius: 16

                        readonly property bool isSelected: transferManager.activePeerId === model.deviceId
                        readonly property bool isConnected: model.connectionState === 3
                        readonly property bool isPending: model.connectionState === 1 || model.connectionState === 2

                        color: isSelected ? pane.theme.accentSoft : pane.theme.surfaceBg
                        border.width: 1
                        border.color: isSelected ? pane.theme.accent : pane.theme.divider

                        Behavior on color {
                            ColorAnimation { duration: 140 }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 16
                            spacing: 12

                            // Peer Monogram Avatar (seeded by unique deviceId)
                            Rectangle {
                                Layout.preferredWidth: 42
                                Layout.preferredHeight: 42
                                radius: 21
                                color: pane.theme.avatarBg(model.deviceId)

                                Text {
                                    anchors.centerIn: parent
                                    text: pane.theme.avatarInitial(model.deviceName)
                                    color: pane.theme.avatarFg(model.deviceId)
                                    font.pixelSize: 16
                                    font.weight: Font.DemiBold
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4

                                Text {
                                    Layout.fillWidth: true
                                    text: model.deviceName
                                    color: pane.theme.textPrimary
                                    font.pixelSize: 15
                                    font.weight: Font.Medium
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: {
                                        if (card.isConnected) return qsTr("已连接") + " · " + model.ipAddress
                                        if (model.connectionState === 1) return qsTr("等待对方确认...")
                                        if (model.connectionState === 2) return qsTr("请求连接...")
                                        return model.ipAddress
                                    }
                                    color: card.isConnected ? pane.theme.onlineDot : pane.theme.textMuted
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                            }

                            // Unread badge
                            Rectangle {
                                visible: model.unreadCount > 0
                                Layout.preferredWidth: Math.max(20, unreadText.implicitWidth + 10)
                                Layout.preferredHeight: 20
                                radius: 10
                                color: pane.theme.accent

                                Text {
                                    id: unreadText
                                    anchors.centerIn: parent
                                    text: model.unreadCount > 99 ? "99+" : String(model.unreadCount)
                                    color: "#FFFFFF"
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                }
                            }

                            // Connection indicator dot
                            Rectangle {
                                visible: model.unreadCount === 0
                                Layout.preferredWidth: 8
                                Layout.preferredHeight: 8
                                radius: 4
                                color: card.isConnected
                                       ? pane.theme.onlineDot
                                       : (card.isPending ? pane.theme.pendingDot : pane.theme.divider)
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                transferManager.connectToDevice(
                                    model.deviceId,
                                    model.deviceName,
                                    model.platform,
                                    model.ipAddress,
                                    model.tcpPort)
                            }
                        }
                    }
                }
            }
        }
    }

    // Rename Local Device Dialog (REQ-001)
    Dialog {
        id: renameDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min((parent ? parent.width : pane.width) - 48, 340)
        modal: true
        padding: 22

        onOpened: {
            renameField.text = deviceDiscovery.localDeviceName
            renameField.forceActiveFocus()
            renameField.selectAll()
        }

        background: Rectangle {
            radius: 20
            color: pane.theme.surfaceBg
            border.width: 1
            border.color: pane.theme.divider
        }

        Overlay.modal: Rectangle { color: "#440F172A" }

        contentItem: ColumnLayout {
            spacing: 16

            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: qsTr("修改设备名称")
                    color: pane.theme.textPrimary
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }
                Button {
                    text: qsTr("恢复默认")
                    Layout.preferredHeight: 28
                    leftPadding: 8
                    rightPadding: 8
                    onClicked: {
                        deviceDiscovery.resetLocalDeviceName()
                        renameDialog.close()
                    }
                    background: Rectangle {
                        radius: 8
                        color: parent.hovered ? pane.theme.pageBg : "transparent"
                    }
                    contentItem: Text {
                        text: parent.text
                        color: pane.theme.textMuted
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                radius: 12
                color: pane.theme.inputBg
                border.width: 1
                border.color: renameField.activeFocus ? pane.theme.accent : pane.theme.divider

                TextInput {
                    id: renameField
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    verticalAlignment: TextInput.AlignVCenter
                    color: pane.theme.textPrimary
                    font.pixelSize: 14
                    maximumLength: 24
                    selectByMouse: true
                    onAccepted: {
                        if (text.trim().length > 0) {
                            deviceDiscovery.setLocalDeviceName(text)
                            renameDialog.close()
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    text: qsTr("取消")
                    onClicked: renameDialog.close()
                    background: Rectangle {
                        radius: 12
                        color: parent.hovered ? pane.theme.divider : pane.theme.pageBg
                    }
                    contentItem: Text {
                        text: parent.text
                        color: pane.theme.textBody
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    enabled: renameField.text.trim().length > 0
                    text: qsTr("保存")
                    onClicked: {
                        deviceDiscovery.setLocalDeviceName(renameField.text)
                        renameDialog.close()
                    }
                    background: Rectangle {
                        radius: 12
                        color: !parent.enabled
                               ? pane.theme.divider
                               : (parent.hovered ? pane.theme.accentHover : pane.theme.accent)
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? "#FFFFFF" : pane.theme.textMuted
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    // Create Group Dialog (REQ-002)
    Dialog {
        id: createGroupDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min((parent ? parent.width : pane.width) - 48, 340)
        modal: true
        padding: 22

        onOpened: {
            groupNameField.text = ""
            groupPinField.text = ""
            groupNameField.forceActiveFocus()
        }

        background: Rectangle {
            radius: 20
            color: pane.theme.surfaceBg
            border.width: 1
            border.color: pane.theme.divider
        }

        Overlay.modal: Rectangle { color: "#440F172A" }

        contentItem: ColumnLayout {
            spacing: 14

            Text {
                Layout.fillWidth: true
                text: qsTr("新建群聊")
                color: pane.theme.textPrimary
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                radius: 12
                color: pane.theme.inputBg
                border.width: 1
                border.color: groupNameField.activeFocus ? pane.theme.accent : pane.theme.divider

                TextInput {
                    id: groupNameField
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    verticalAlignment: TextInput.AlignVCenter
                    color: pane.theme.textPrimary
                    font.pixelSize: 14
                    maximumLength: 24
                    selectByMouse: true

                    Text {
                        anchors.fill: parent
                        verticalAlignment: Text.AlignVCenter
                        text: qsTr("群聊名称（如：会议资料群）")
                        color: pane.theme.textMuted
                        font.pixelSize: 14
                        visible: groupNameField.text.length === 0
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                radius: 12
                color: pane.theme.inputBg
                border.width: 1
                border.color: groupPinField.activeFocus ? pane.theme.accent : pane.theme.divider

                TextInput {
                    id: groupPinField
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    verticalAlignment: TextInput.AlignVCenter
                    color: pane.theme.textPrimary
                    font.pixelSize: 14
                    maximumLength: 6
                    selectByMouse: true

                    Text {
                        anchors.fill: parent
                        verticalAlignment: Text.AlignVCenter
                        text: qsTr("加入口令（可选，留空则需群主确认）")
                        color: pane.theme.textMuted
                        font.pixelSize: 13
                        visible: groupPinField.text.length === 0
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    text: qsTr("取消")
                    onClicked: createGroupDialog.close()
                    background: Rectangle {
                        radius: 12
                        color: parent.hovered ? pane.theme.divider : pane.theme.pageBg
                    }
                    contentItem: Text {
                        text: parent.text
                        color: pane.theme.textBody
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    text: qsTr("创建")
                    onClicked: {
                        groupManager.createGroup(groupNameField.text, groupPinField.text)
                        createGroupDialog.close()
                    }
                    background: Rectangle {
                        radius: 12
                        color: parent.hovered ? pane.theme.accentHover : pane.theme.accent
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

    // Join Group with PIN Dialog
    Dialog {
        id: joinPinDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min((parent ? parent.width : pane.width) - 48, 320)
        modal: true
        padding: 22

        property string targetGroupId: ""
        property string targetGroupName: ""
        property string targetHostId: ""
        property string targetHostName: ""
        property string targetHostIp: ""
        property int targetHostPort: 0

        function openForGroup(gid, gname, hid, hname, hip, hport) {
            targetGroupId = gid
            targetGroupName = gname
            targetHostId = hid
            targetHostName = hname
            targetHostIp = hip
            targetHostPort = hport
            joinPinField.text = ""
            open()
            joinPinField.forceActiveFocus()
        }

        background: Rectangle {
            radius: 20
            color: pane.theme.surfaceBg
            border.width: 1
            border.color: pane.theme.divider
        }

        Overlay.modal: Rectangle { color: "#440F172A" }

        contentItem: ColumnLayout {
            spacing: 16

            Text {
                Layout.fillWidth: true
                text: qsTr("加入「%1」").arg(joinPinDialog.targetGroupName)
                color: pane.theme.textPrimary
                font.pixelSize: 16
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                radius: 12
                color: pane.theme.inputBg
                border.width: 1
                border.color: joinPinField.activeFocus ? pane.theme.accent : pane.theme.divider

                TextInput {
                    id: joinPinField
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    verticalAlignment: TextInput.AlignVCenter
                    color: pane.theme.textPrimary
                    font.pixelSize: 14
                    maximumLength: 6
                    onAccepted: {
                        if (text.trim().length > 0) {
                            groupManager.joinGroup(
                                joinPinDialog.targetGroupId, joinPinDialog.targetGroupName,
                                joinPinDialog.targetHostId, joinPinDialog.targetHostName,
                                joinPinDialog.targetHostIp, joinPinDialog.targetHostPort,
                                text.trim())
                            joinPinDialog.close()
                        }
                    }

                    Text {
                        anchors.fill: parent
                        verticalAlignment: Text.AlignVCenter
                        text: qsTr("输入群口令")
                        color: pane.theme.textMuted
                        font.pixelSize: 14
                        visible: joinPinField.text.length === 0
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    text: qsTr("取消")
                    onClicked: joinPinDialog.close()
                    background: Rectangle {
                        radius: 12
                        color: parent.hovered ? pane.theme.divider : pane.theme.pageBg
                    }
                    contentItem: Text {
                        text: parent.text
                        color: pane.theme.textBody
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    enabled: joinPinField.text.trim().length > 0
                    text: qsTr("加入")
                    onClicked: {
                        groupManager.joinGroup(
                            joinPinDialog.targetGroupId, joinPinDialog.targetGroupName,
                            joinPinDialog.targetHostId, joinPinDialog.targetHostName,
                            joinPinDialog.targetHostIp, joinPinDialog.targetHostPort,
                            joinPinField.text.trim())
                        joinPinDialog.close()
                    }
                    background: Rectangle {
                        radius: 12
                        color: !parent.enabled
                               ? pane.theme.divider
                               : (parent.hovered ? pane.theme.accentHover : pane.theme.accent)
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? "#FFFFFF" : pane.theme.textMuted
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    // Minimalist Direct IP Connect Dialog
    Dialog {
        id: directConnectDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min((parent ? parent.width : pane.width) - 48, 340)
        modal: true
        padding: 22

        onOpened: {
            ipField.forceActiveFocus()
            ipField.selectAll()
        }

        background: Rectangle {
            radius: 20
            color: pane.theme.surfaceBg
            border.width: 1
            border.color: pane.theme.divider
        }

        Overlay.modal: Rectangle {
            color: "#440F172A"
        }

        contentItem: ColumnLayout {
            spacing: 16

            Text {
                Layout.fillWidth: true
                text: qsTr("手动连接")
                color: pane.theme.textPrimary
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 42
                radius: 12
                color: pane.theme.inputBg
                border.width: 1
                border.color: ipField.activeFocus ? pane.theme.accent : pane.theme.divider

                TextInput {
                    id: ipField
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    verticalAlignment: TextInput.AlignVCenter
                    color: pane.theme.textPrimary
                    font.pixelSize: 14
                    selectByMouse: true
                    onAccepted: {
                        if (text.trim().length > 0) {
                            transferManager.connectByAddress(text)
                            directConnectDialog.close()
                        }
                    }

                    Text {
                        anchors.fill: parent
                        verticalAlignment: Text.AlignVCenter
                        text: "10.150.23.x"
                        color: pane.theme.textMuted
                        font.pixelSize: 14
                        visible: ipField.text.length === 0
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    text: qsTr("取消")
                    onClicked: directConnectDialog.close()

                    background: Rectangle {
                        radius: 12
                        color: parent.hovered ? pane.theme.divider : pane.theme.pageBg
                    }
                    contentItem: Text {
                        text: parent.text
                        color: pane.theme.textBody
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Button {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    enabled: ipField.text.trim().length > 0
                    text: qsTr("连接")
                    onClicked: {
                        transferManager.connectByAddress(ipField.text)
                        directConnectDialog.close()
                    }

                    background: Rectangle {
                        radius: 12
                        color: !parent.enabled
                               ? pane.theme.divider
                               : (parent.hovered ? pane.theme.accentHover : pane.theme.accent)
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? "#FFFFFF" : pane.theme.textMuted
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }
}
