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
        ipField.forceActiveFocus()
        ipField.selectAll()
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

        Text {
            Layout.fillWidth: true
            text: qsTr("手动连接")
            color: dlg.theme.textPrimary
            font.pixelSize: 16
            font.weight: Font.DemiBold
        }

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 42
            radius: 12; color: dlg.theme.inputBg
            border.width: 1
            border.color: ipField.activeFocus ? dlg.theme.accent : dlg.theme.divider

            TextInput {
                id: ipField
                anchors.fill: parent; anchors.leftMargin: 14; anchors.rightMargin: 14
                verticalAlignment: TextInput.AlignVCenter
                color: dlg.theme.textPrimary; font.pixelSize: 14
                selectByMouse: true
                onAccepted: {
                    if (text.trim().length > 0) {
                        transferManager.connectByAddress(text)
                        dlg.close()
                    }
                }

                Text {
                    anchors.fill: parent; verticalAlignment: Text.AlignVCenter
                    text: "10.150.23.x"
                    color: dlg.theme.textMuted; font.pixelSize: 14
                    visible: ipField.text.length === 0
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
                enabled: ipField.text.trim().length > 0
                text: qsTr("连接")
                onClicked: {
                    transferManager.connectByAddress(ipField.text)
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
