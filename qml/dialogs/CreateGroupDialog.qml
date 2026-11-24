import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Dialog {
    id: dlg
    required property QtObject theme

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min((parent ? parent.width : 400) - 48, 340)
    modal: true
    focus: true
    padding: 22

    onOpened: {
        groupNameField.text = ""
        groupPinField.text = ""
        groupNameField.forceActiveFocus()
    }

    background: Rectangle {
        radius: 20
        color: dlg.theme.surfaceBg
        border.width: 1
        border.color: dlg.theme.divider
    }

    Overlay.modal: Rectangle { color: "#440F172A" }

    contentItem: ColumnLayout {
        spacing: 14

        Text {
            Layout.fillWidth: true
            text: qsTr("新建群聊")
            color: dlg.theme.textPrimary
            font.pixelSize: 16
            font.weight: Font.DemiBold
        }

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 42
            radius: 12; color: dlg.theme.inputBg
            border.width: 1
            border.color: groupNameField.activeFocus ? dlg.theme.accent : dlg.theme.divider

            TextInput {
                id: groupNameField
                anchors.fill: parent; anchors.leftMargin: 14; anchors.rightMargin: 14
                verticalAlignment: TextInput.AlignVCenter
                color: dlg.theme.textPrimary; font.pixelSize: 14
                maximumLength: 24; selectByMouse: true

                Text {
                    anchors.fill: parent; verticalAlignment: Text.AlignVCenter
                    text: qsTr("群聊名称（如：会议资料群）")
                    color: dlg.theme.textMuted; font.pixelSize: 14
                    visible: groupNameField.text.length === 0
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 42
            radius: 12; color: dlg.theme.inputBg
            border.width: 1
            border.color: groupPinField.activeFocus ? dlg.theme.accent : dlg.theme.divider

            TextInput {
                id: groupPinField
                anchors.fill: parent; anchors.leftMargin: 14; anchors.rightMargin: 14
                verticalAlignment: TextInput.AlignVCenter
                color: dlg.theme.textPrimary; font.pixelSize: 14
                maximumLength: 6; selectByMouse: true

                Text {
                    anchors.fill: parent; verticalAlignment: Text.AlignVCenter
                    text: qsTr("加入口令（可选，留空则需群主确认）")
                    color: dlg.theme.textMuted; font.pixelSize: 13
                    visible: groupPinField.text.length === 0
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
                text: qsTr("创建")
                onClicked: {
                    groupManager.createGroup(groupNameField.text, groupPinField.text)
                    dlg.close()
                }
                background: Rectangle { radius: 12; color: parent.hovered ? dlg.theme.accentHover : dlg.theme.accent }
                contentItem: Text {
                    text: parent.text; color: "#FFFFFF"; font.pixelSize: 14; font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
