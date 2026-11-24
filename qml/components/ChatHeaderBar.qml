import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: headerBar
    required property QtObject theme
    property bool showBackButton: false
    property bool isGroupChat: false
    property bool isConnected: false
    property bool isPendingOutgoing: false
    property string avatarSeed: ""
    property string titleText: ""
    property string subtitleText: ""
    property bool isGroupHost: false

    signal backClicked()
    signal membersClicked()
    signal leaveOrDisconnectClicked()

    height: 66
    implicitHeight: 66
    color: theme.chatBarBg

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 16
        spacing: 10

        Button {
            visible: headerBar.showBackButton
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            onClicked: headerBar.backClicked()

            background: Rectangle {
                radius: 18
                color: parent.hovered ? headerBar.theme.pageBg : "transparent"
            }

            contentItem: Canvas {
                id: backCanvas
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = headerBar.theme.textPrimary
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
                    target: headerBar.theme
                    function onTextPrimaryChanged() { backCanvas.requestPaint() }
                }
            }
        }

        Item {
            visible: !headerBar.showBackButton
            Layout.preferredWidth: 6
        }

        AvatarBadge {
            theme: headerBar.theme
            size: 38
            isGroup: headerBar.isGroupChat
            seed: headerBar.avatarSeed
            labelText: headerBar.titleText
            showStatusDot: true
            statusColor: headerBar.isConnected
                         ? headerBar.theme.onlineDot
                         : (headerBar.isPendingOutgoing ? headerBar.theme.pendingDot : headerBar.theme.textMuted)
            dotBorderColor: headerBar.theme.chatBarBg
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: headerBar.titleText
                color: headerBar.theme.textPrimary
                font.pixelSize: 15
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: headerBar.subtitleText
                color: headerBar.theme.textMuted
                font.pixelSize: 12
                elide: Text.ElideRight
            }
        }

        Button {
            visible: headerBar.isGroupChat
            text: qsTr("成员")
            Layout.preferredHeight: 32
            leftPadding: 10
            rightPadding: 10
            onClicked: headerBar.membersClicked()

            background: Rectangle {
                radius: 10
                color: parent.hovered ? headerBar.theme.pageBg : "transparent"
            }
            contentItem: Text {
                text: parent.text
                color: headerBar.theme.accent
                font.pixelSize: 13
                font.weight: Font.Medium
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        Button {
            visible: headerBar.isGroupChat || headerBar.isConnected || headerBar.isPendingOutgoing
            text: headerBar.isGroupChat
                  ? (headerBar.isGroupHost ? qsTr("解散") : qsTr("退出"))
                  : qsTr("断开")
            Layout.preferredHeight: 32
            leftPadding: 10
            rightPadding: 10
            onClicked: headerBar.leaveOrDisconnectClicked()

            background: Rectangle {
                radius: 10
                color: parent.hovered ? headerBar.theme.pageBg : "transparent"
            }
            contentItem: Text {
                text: parent.text
                color: headerBar.theme.textMuted
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
        color: headerBar.theme.divider
    }
}
