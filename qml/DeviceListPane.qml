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

        // Local Device Header
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
                    anchors.leftMargin: 10; anchors.rightMargin: 10
                    spacing: 12

                    AvatarBadge {
                        theme: pane.theme
                        size: 42
                        seed: deviceDiscovery.localDeviceId
                        labelText: deviceDiscovery.localDeviceName
                        showStatusDot: true
                        statusColor: deviceDiscovery.online ? pane.theme.onlineDot : pane.theme.textMuted
                        dotBorderColor: pane.theme.pageBg
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
                                font.pixelSize: 15; font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            Text { text: qsTr("改名"); color: pane.theme.accent; font.pixelSize: 12 }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Text {
                                Layout.fillWidth: true
                                text: {
                                    if (!deviceDiscovery.discoverable) return qsTr("已下线 · 对附近设备不可见")
                                    if (deviceDiscovery.localIpAddress.length === 0) return qsTr("未获取局域网 IP")
                                    return transferManager.serverPort === 45455
                                           ? deviceDiscovery.localIpAddress
                                           : (deviceDiscovery.localIpAddress + ":" + transferManager.serverPort)
                                }
                                color: pane.theme.textMuted; font.pixelSize: 12
                                elide: Text.ElideRight
                            }

                            RowLayout {
                                spacing: 6

                                Text {
                                    text: deviceDiscovery.discoverable ? qsTr("在线") : qsTr("已下线")
                                    color: deviceDiscovery.discoverable ? pane.theme.onlineDot : pane.theme.textMuted
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                }

                                Switch {
                                    id: onlineSwitch
                                    checked: deviceDiscovery.discoverable
                                    padding: 0
                                    implicitWidth: 36
                                    implicitHeight: 20
                                    hoverEnabled: true
                                    onToggled: deviceDiscovery.setDiscoverable(checked)

                                    MouseArea {
                                        anchors.fill: parent
                                        acceptedButtons: Qt.NoButton
                                        cursorShape: Qt.PointingHandCursor
                                    }

                                    indicator: Rectangle {
                                        implicitWidth: 36
                                        implicitHeight: 20
                                        radius: 10
                                        color: onlineSwitch.checked
                                               ? (onlineSwitch.hovered ? "#059669" : pane.theme.onlineDot)
                                               : (onlineSwitch.hovered ? pane.theme.textMuted : pane.theme.divider)
                                        Behavior on color { ColorAnimation { duration: 150 } }

                                        Rectangle {
                                            x: onlineSwitch.checked ? parent.width - width - 2 : 2
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: 16
                                            height: 16
                                            radius: 8
                                            color: "#FFFFFF"
                                            Behavior on x { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                                        }
                                    }

                                    contentItem: Item {}
                                }
                            }
                        }
                    }
                }

                MouseArea {
                    id: headerMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    z: -1
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

                // Group Section Header
                Item {
                    Layout.fillWidth: true; Layout.preferredHeight: 34
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 20; anchors.rightMargin: 14
                        Text { Layout.fillWidth: true; text: qsTr("群聊"); color: pane.theme.textMuted; font.pixelSize: 12; font.weight: Font.Medium }
                        Button {
                            text: qsTr("+ 新建群聊"); Layout.preferredHeight: 26; leftPadding: 10; rightPadding: 10
                            onClicked: createGroupDialog.open()
                            background: Rectangle { radius: 8; color: parent.hovered ? pane.theme.divider : "transparent" }
                            contentItem: Text {
                                text: parent.text; color: pane.theme.accent; font.pixelSize: 12; font.weight: Font.Medium
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }

                Repeater {
                    model: deviceDiscovery.groupModel
                    delegate: DeviceListCard {
                        readonly property bool isJoining: groupManager.joiningGroupId === model.groupId
                        Layout.fillWidth: true
                        Layout.leftMargin: 16; Layout.rightMargin: 16
                        Layout.topMargin: 4; Layout.bottomMargin: 6
                        theme: pane.theme
                        isGroup: true
                        isSelected: groupManager.activeGroupId === model.groupId
                        avatarSeed: model.groupId
                        titleText: model.groupName
                        subtitleText: {
                            var base = model.memberCount + qsTr(" 人")
                            if (model.isHost) return qsTr("我创建的") + " · " + base
                            if (model.isJoined) return qsTr("已加入") + " · " + base
                            if (isJoining) return qsTr("等待群主确认...")
                            return (model.hasPin ? qsTr("需口令 · ") : "") + model.hostName
                        }
                        subtitleColor: (model.isJoined || model.isHost)
                                       ? pane.theme.onlineDot
                                       : (isJoining ? pane.theme.pendingDot : pane.theme.textMuted)
                        unreadCount: model.unreadCount
                        trailingActionText: {
                            if (model.isJoined || model.isHost) return ""
                            return isJoining ? qsTr("申请中") : qsTr("加入")
                        }
                        onClicked: {
                            if (isJoining) return
                            if (model.isJoined || model.isHost) groupManager.selectGroup(model.groupId)
                            else if (model.hasPin) joinPinDialog.openForGroup(model.groupId, model.groupName, model.hostId, model.hostName, model.hostIp, model.hostPort)
                            else groupManager.joinGroup(model.groupId, model.groupName, model.hostId, model.hostName, model.hostIp, model.hostPort, "")
                        }
                    }
                }

                // Nearby Devices Section Header
                Item {
                    Layout.fillWidth: true
                    Layout.topMargin: deviceDiscovery.groupModel.count > 0 ? 8 : 0
                    Layout.preferredHeight: 34
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 20; anchors.rightMargin: 14
                        Text { Layout.fillWidth: true; text: qsTr("附近设备"); color: pane.theme.textMuted; font.pixelSize: 12; font.weight: Font.Medium }
                        Button {
                            text: qsTr("手动连接"); Layout.preferredHeight: 26; leftPadding: 10; rightPadding: 10
                            onClicked: directConnectDialog.open()
                            background: Rectangle { radius: 8; color: parent.hovered ? pane.theme.divider : "transparent" }
                            contentItem: Text {
                                text: parent.text; color: pane.theme.accent; font.pixelSize: 12; font.weight: Font.Medium
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }

                Repeater {
                    model: deviceDiscovery
                    delegate: DeviceListCard {
                        Layout.fillWidth: true
                        Layout.leftMargin: 16; Layout.rightMargin: 16
                        Layout.topMargin: 4; Layout.bottomMargin: 6
                        theme: pane.theme
                        isGroup: false
                        isSelected: transferManager.activePeerId === model.deviceId
                        avatarSeed: model.deviceId
                        titleText: model.deviceName
                        subtitleText: {
                            if (model.connectionState === 3) return qsTr("已连接") + " · " + model.ipAddress
                            if (model.connectionState === 1) return qsTr("等待对方确认...")
                            if (model.connectionState === 2) return qsTr("请求连接...")
                            return model.ipAddress
                        }
                        subtitleColor: model.connectionState === 3 ? pane.theme.onlineDot : pane.theme.textMuted
                        unreadCount: model.unreadCount
                        showDot: true
                        dotColor: model.connectionState === 3
                                  ? pane.theme.onlineDot
                                  : ((model.connectionState === 1 || model.connectionState === 2) ? pane.theme.pendingDot : pane.theme.divider)
                        onClicked: transferManager.connectToDevice(model.deviceId, model.deviceName, model.platform, model.ipAddress, model.tcpPort)
                    }
                }
            }
        }
    }

    RenameDialog { id: renameDialog; theme: pane.theme }
    CreateGroupDialog { id: createGroupDialog; theme: pane.theme }
    JoinPinDialog { id: joinPinDialog; theme: pane.theme }
    DirectConnectDialog { id: directConnectDialog; theme: pane.theme }
}
