import QtQuick

Item {
    id: mediaCard
    required property QtObject theme
    property string mediaKind: "image" // "image" | "video"
    property string fileUrl: ""
    property string filePath: ""
    property string fileName: ""
    property string fileExtension: ""
    property string fileSizeText: ""
    property bool outgoing: false
    property bool done: false
    property bool transferring: false
    property real progress: 0.0
    property real maxWidth: 240

    signal imageClicked(string url, string path, string name)
    signal videoClicked(string path)

    implicitWidth: Math.min(maxWidth, 240)
    implicitHeight: mediaKind === "image" ? 180 : 150

    // Image Preview Card (12px rounded corners)
    Rectangle {
        visible: mediaCard.mediaKind === "image"
        anchors.fill: parent
        radius: 12
        color: mediaCard.theme.pageBg
        clip: true

        Image {
            anchors.fill: parent
            source: (mediaCard.done || mediaCard.outgoing) ? mediaCard.fileUrl : ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: false
            sourceSize.width: 480
            sourceSize.height: 360
        }

        Rectangle {
            visible: mediaCard.transferring
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 4
            color: "#33000000"

            Rectangle {
                width: parent.width * mediaCard.progress
                height: parent.height
                color: mediaCard.theme.accent
            }
        }

        MouseArea {
            anchors.fill: parent
            enabled: mediaCard.done || mediaCard.outgoing
            cursorShape: Qt.PointingHandCursor
            onClicked: mediaCard.imageClicked(mediaCard.fileUrl, mediaCard.filePath, mediaCard.fileName)
        }
    }

    // Video Card (12px rounded corners + subtle corner badge)
    Rectangle {
        visible: mediaCard.mediaKind === "video"
        anchors.fill: parent
        radius: 12
        color: "#1E2430"
        clip: true

        Rectangle {
            anchors.centerIn: parent
            width: 44
            height: 44
            radius: 22
            color: "#44FFFFFF"

            Canvas {
                anchors.fill: parent
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.fillStyle = "#FFFFFF"
                    ctx.beginPath()
                    ctx.moveTo(width * 0.42, height * 0.32)
                    ctx.lineTo(width * 0.68, height * 0.50)
                    ctx.lineTo(width * 0.42, height * 0.68)
                    ctx.closePath()
                    ctx.fill()
                }
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.leftMargin: 10
            anchors.bottomMargin: 10
            height: 22
            width: videoBadgeLabel.implicitWidth + 14
            radius: 6
            color: "#66000000"

            Text {
                id: videoBadgeLabel
                anchors.centerIn: parent
                text: mediaCard.fileExtension + " · " + mediaCard.fileSizeText
                color: "#F1F5F9"
                font.pixelSize: 11
            }
        }

        Rectangle {
            visible: mediaCard.transferring
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 3
            color: "#33FFFFFF"

            Rectangle {
                width: parent.width * mediaCard.progress
                height: parent.height
                color: mediaCard.theme.accent
            }
        }

        MouseArea {
            anchors.fill: parent
            enabled: mediaCard.done || mediaCard.outgoing
            cursorShape: Qt.PointingHandCursor
            onClicked: mediaCard.videoClicked(mediaCard.filePath)
        }
    }
}
