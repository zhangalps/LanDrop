import QtQuick
import QtQuick.Layouts

Rectangle {
    id: card
    property QtObject theme
    property bool isGroup: false
    property bool isSelected: false
    property string avatarSeed: ""
    property string titleText: ""
    property string subtitleText: ""
    property color subtitleColor: theme.textMuted
    property int unreadCount: 0
    property bool showDot: false
    property color dotColor: theme.divider
    property string trailingActionText: ""

    signal clicked()

    implicitHeight: isGroup ? 68 : 70
    radius: 16
    color: isSelected ? theme.accentSoft : theme.surfaceBg
    border.width: 1
    border.color: isSelected ? theme.accent : theme.divider

    Behavior on color { ColorAnimation { duration: 140 } }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 16
        spacing: 12

        AvatarBadge {
            theme: card.theme
            size: 42
            isGroup: card.isGroup
            seed: card.avatarSeed
            labelText: card.titleText
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Text {
                Layout.fillWidth: true
                text: card.titleText
                color: card.theme.textPrimary
                font.pixelSize: 15
                font.weight: Font.Medium
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: card.subtitleText
                color: card.subtitleColor
                font.pixelSize: 12
                elide: Text.ElideRight
            }
        }

        Rectangle {
            visible: card.unreadCount > 0
            Layout.preferredWidth: Math.max(20, badgeText.implicitWidth + 10)
            Layout.preferredHeight: 20
            radius: 10
            color: card.theme.accent

            Text {
                id: badgeText
                anchors.centerIn: parent
                text: card.unreadCount > 99 ? "99+" : String(card.unreadCount)
                color: "#FFFFFF"
                font.pixelSize: 11
                font.weight: Font.Medium
            }
        }

        Text {
            visible: card.unreadCount === 0 && card.trailingActionText.length > 0
            text: card.trailingActionText
            color: card.theme.accent
            font.pixelSize: 12
            font.weight: Font.Medium
        }

        Rectangle {
            visible: card.unreadCount === 0 && card.showDot
            Layout.preferredWidth: 8
            Layout.preferredHeight: 8
            radius: 4
            color: card.dotColor
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: card.clicked()
    }
}
