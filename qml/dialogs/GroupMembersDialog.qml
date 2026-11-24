import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Dialog {
    id: dlg
    required property QtObject theme

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min((parent ? parent.width : 400) - 48, 360)
    modal: true
    focus: true
    padding: 20

    background: Rectangle {
        radius: 20
        color: dlg.theme.surfaceBg
        border.width: 1
        border.color: dlg.theme.divider
    }

    Overlay.modal: Rectangle { color: "#440F172A" }

    contentItem: ColumnLayout {
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                text: qsTr("群成员 (%1)").arg(groupManager.activeGroupMemberCount)
                color: dlg.theme.textPrimary
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }
            Button {
                text: qsTr("关闭")
                Layout.preferredHeight: 28
                leftPadding: 10; rightPadding: 10
                onClicked: dlg.close()
                background: Rectangle { radius: 8; color: parent.hovered ? dlg.theme.pageBg : "transparent" }
                contentItem: Text { text: parent.text; color: dlg.theme.textMuted; font.pixelSize: 12; verticalAlignment: Text.AlignVCenter }
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

                AvatarBadge {
                    theme: dlg.theme
                    size: 34
                    seed: model.deviceId
                    labelText: model.deviceName
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Text {
                        Layout.fillWidth: true
                        text: model.deviceName
                        color: dlg.theme.textPrimary
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.fillWidth: true
                        text: model.ipAddress
                        color: dlg.theme.textMuted
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }

                Rectangle {
                    visible: model.isHost
                    implicitWidth: hostTag.implicitWidth + 12
                    height: 20
                    radius: 6
                    color: dlg.theme.accentSoft

                    Text {
                        id: hostTag
                        anchors.centerIn: parent
                        text: qsTr("群主")
                        color: dlg.theme.accent
                        font.pixelSize: 11
                        font.weight: Font.Medium
                    }
                }
            }
        }
    }
}
