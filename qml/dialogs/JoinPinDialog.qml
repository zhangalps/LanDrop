import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Dialog {
    id: dlg
    required property QtObject theme

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

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min((parent ? parent.width : 400) - 48, 320)
    modal: true
    focus: true
    padding: 22

    background: Rectangle {
        radius: 20
        color: dlg.theme.surfaceBg
        border.width: 1
        border.color: dlg.theme.divider
    }

    Overlay.modal: Rectangle { color: "#440F172A" }

    contentItem: ColumnLayout {
        spacing: 16

        Text {
            Layout.fillWidth: true
            text: qsTr("加入「%1」").arg(dlg.targetGroupName)
            color: dlg.theme.textPrimary
            font.pixelSize: 16
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 42
            radius: 12; color: dlg.theme.inputBg
            border.width: 1
            border.color: joinPinField.activeFocus ? dlg.theme.accent : dlg.theme.divider

            TextInput {
                id: joinPinField
                anchors.fill: parent; anchors.leftMargin: 14; anchors.rightMargin: 14
                verticalAlignment: TextInput.AlignVCenter
                color: dlg.theme.textPrimary; font.pixelSize: 14
                maximumLength: 6
                onAccepted: {
                    if (text.trim().length > 0) {
                        groupManager.joinGroup(
                            dlg.targetGroupId, dlg.targetGroupName, dlg.targetHostId,
                            dlg.targetHostName, dlg.targetHostIp, dlg.targetHostPort, text.trim())
                        dlg.close()
                    }
                }

                Text {
                    anchors.fill: parent; verticalAlignment: Text.AlignVCenter
                    text: qsTr("输入群口令")
                    color: dlg.theme.textMuted; font.pixelSize: 14
                    visible: joinPinField.text.length === 0
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
                Layout.fillWidth: true; Layout.preferredHeight: 38
                text: qsTr("取消")
                onClicked: dlg.close()
                background: Rectangle { radius: 12; color: parent.hovered ? dlg.theme.divider : dlg.theme.pageBg }
                contentItem: Text {
                    text: parent.text; color: dlg.theme.textBody; font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                Layout.fillWidth: true; Layout.preferredHeight: 38
                enabled: joinPinField.text.trim().length > 0
                text: qsTr("加入")
                onClicked: {
                    groupManager.joinGroup(
                        dlg.targetGroupId, dlg.targetGroupName, dlg.targetHostId,
                        dlg.targetHostName, dlg.targetHostIp, dlg.targetHostPort, joinPinField.text.trim())
                    dlg.close()
                }
                background: Rectangle {
                    radius: 12
                    color: !parent.enabled ? dlg.theme.divider : (parent.hovered ? dlg.theme.accentHover : dlg.theme.accent)
                }
                contentItem: Text {
                    text: parent.text; color: parent.enabled ? "#FFFFFF" : dlg.theme.textMuted
                    font.pixelSize: 14; font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
