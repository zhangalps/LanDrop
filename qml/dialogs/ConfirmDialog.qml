import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Dialog {
    id: confirmDialog
    required property QtObject theme
    property bool isDark: false
    property string headingText: ""
    property string bodyText: ""
    property string rejectLabel: qsTr("拒绝")
    property string acceptLabel: qsTr("接受")

    signal rejectedAction()
    signal acceptedAction()

    modal: true
    dim: true
    anchors.centerIn: parent
    width: Math.min((parent ? parent.width : 400) - 48, 340)
    padding: 24
    closePolicy: Popup.NoAutoClose

    background: Rectangle {
        radius: 20
        color: confirmDialog.theme.surfaceBg
        border.width: 1
        border.color: confirmDialog.theme.divider
    }

    Overlay.modal: Rectangle {
        color: confirmDialog.isDark ? "#88000000" : "#440F172A"
    }

    contentItem: ColumnLayout {
        spacing: 20

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                Layout.fillWidth: true
                text: confirmDialog.headingText
                color: confirmDialog.theme.textPrimary
                font.pixelSize: 17
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: confirmDialog.bodyText
                color: confirmDialog.theme.textMuted
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                text: confirmDialog.rejectLabel
                onClicked: confirmDialog.rejectedAction()

                background: Rectangle {
                    radius: 12
                    color: parent.hovered ? confirmDialog.theme.divider : confirmDialog.theme.pageBg
                }
                contentItem: Text {
                    text: parent.text
                    color: confirmDialog.theme.textBody
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                text: confirmDialog.acceptLabel
                onClicked: confirmDialog.acceptedAction()

                background: Rectangle {
                    radius: 12
                    color: parent.hovered ? confirmDialog.theme.accentHover : confirmDialog.theme.accent
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
