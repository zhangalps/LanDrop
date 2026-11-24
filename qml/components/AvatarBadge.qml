import QtQuick

Item {
    id: badge
    required property QtObject theme
    property string seed: ""
    property string labelText: ""
    property bool isGroup: false
    property int size: 40
    property bool showStatusDot: false
    property color statusColor: theme.onlineDot
    property color dotBorderColor: theme.pageBg

    width: size
    height: size
    implicitWidth: size
    implicitHeight: size

    Rectangle {
        anchors.fill: parent
        radius: badge.isGroup ? Math.round(badge.size * 0.31) : badge.size / 2
        color: badge.theme.avatarBg(badge.seed)

        Text {
            anchors.centerIn: parent
            text: badge.isGroup ? "#" : badge.theme.avatarInitial(badge.labelText)
            color: badge.theme.avatarFg(badge.seed)
            font.pixelSize: Math.round(badge.size * (badge.isGroup ? 0.44 : 0.38))
            font.weight: badge.isGroup ? Font.Bold : Font.DemiBold
        }
    }

    Rectangle {
        visible: badge.showStatusDot
        width: Math.max(10, Math.round(badge.size * 0.29))
        height: width
        radius: width / 2
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        color: badge.statusColor
        border.width: 2
        border.color: badge.dotBorderColor
    }
}
