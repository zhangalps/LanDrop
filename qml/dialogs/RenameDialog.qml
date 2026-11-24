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
        renameField.text = deviceDiscovery.localDeviceName
        renameField.forceActiveFocus()
        renameField.selectAll()
    }

    background: Rectangle {
        radius: 20
        color: dlg.theme.surfaceBg
        border.width: 1
        border.color: dlg.theme.divider
    }

    Overlay.modal: Rectangle { color: "#440F172A" }

    contentItem: ColumnLayout {
        spacing: 16

        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                text: qsTr("修改设备名称")
                color: dlg.theme.textPrimary
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }
            Button {
                text: qsTr("恢复默认")
                Layout.preferredHeight: 28
                leftPadding: 8; rightPadding: 8
                onClicked: {
                    deviceDiscovery.resetLocalDeviceName()
                    dlg.close()
                }
                background: Rectangle { radius: 8; color: parent.hovered ? dlg.theme.pageBg : "transparent" }
                contentItem: Text { text: parent.text; color: dlg.theme.textMuted; font.pixelSize: 12; verticalAlignment: Text.AlignVCenter }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            radius: 12
            color: dlg.theme.inputBg
            border.width: 1
            border.color: renameField.activeFocus ? dlg.theme.accent : dlg.theme.divider

            TextInput {
                id: renameField
                anchors.fill: parent
                anchors.leftMargin: 14; anchors.rightMargin: 14
                verticalAlignment: TextInput.AlignVCenter
                color: dlg.theme.textPrimary
                font.pixelSize: 14
                maximumLength: 24
                selectByMouse: true
                onAccepted: {
                    if (text.trim().length > 0) {
                        deviceDiscovery.setLocalDeviceName(text)
                        dlg.close()
                    }
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
                enabled: renameField.text.trim().length > 0
                text: qsTr("保存")
                onClicked: {
                    deviceDiscovery.setLocalDeviceName(renameField.text)
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
