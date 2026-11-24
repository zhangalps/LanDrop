import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Popup {
    id: lightbox
    required property QtObject theme

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: parent ? parent.width : 800
    height: parent ? parent.height : 600
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape

    property string imageUrl: ""
    property string localPath: ""
    property string imageName: ""

    signal toastRequested(string message)

    function openImage(url, path, name) {
        imageUrl = url
        localPath = path
        imageName = name
        previewImg.scale = 1.0
        previewImg.x = 0
        previewImg.y = 0
        open()
    }

    background: Rectangle {
        color: "#F00D1017"
    }

    contentItem: Item {
        anchors.fill: parent

        MouseArea {
            anchors.fill: parent
            onClicked: lightbox.close()
        }

        Item {
            id: canvasContainer
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: bottomActionBar.top
            anchors.topMargin: platformHelper.isAndroid ? 36 : 16
            clip: true

            Image {
                id: previewImg
                anchors.centerIn: parent
                width: canvasContainer.width - 24
                height: canvasContainer.height - 24
                source: lightbox.imageUrl
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                smooth: true
                transformOrigin: Item.Center

                PinchHandler {
                    target: previewImg
                    minimumScale: 1.0
                    maximumScale: 4.0
                    xAxis.enabled: true
                    yAxis.enabled: true
                }

                DragHandler {
                    target: previewImg
                    enabled: previewImg.scale > 1.01
                }

                TapHandler {
                    onDoubleTapped: {
                        if (previewImg.scale > 1.2) {
                            previewImg.scale = 1.0
                            previewImg.x = 0
                            previewImg.y = 0
                        } else {
                            previewImg.scale = 2.2
                        }
                    }
                }

                WheelHandler {
                    onWheel: function(event) {
                        var factor = event.angleDelta.y > 0 ? 1.15 : 0.87
                        var next = Math.max(1.0, Math.min(4.0, previewImg.scale * factor))
                        previewImg.scale = next
                        if (next <= 1.01) {
                            previewImg.x = 0
                            previewImg.y = 0
                        }
                    }
                }
            }
        }

        // Unified Bottom Bar (avoids Android top status bar)
        Rectangle {
            id: bottomActionBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: platformHelper.isAndroid ? 112 : 92
            color: "#99000000"

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                anchors.topMargin: 12
                anchors.bottomMargin: platformHelper.isAndroid ? 26 : 14
                spacing: 12

                Text {
                    Layout.fillWidth: true
                    text: lightbox.imageName
                    color: "#CBD5E1"
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideMiddle
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14

                    Item { Layout.fillWidth: true }

                    Button {
                        visible: platformHelper.isWindows
                        text: qsTr("打开文件夹")
                        Layout.preferredHeight: 38
                        leftPadding: 18; rightPadding: 18
                        onClicked: platformHelper.revealInFolder(lightbox.localPath)
                        background: Rectangle { radius: 19; color: parent.hovered ? "#44FFFFFF" : "#24FFFFFF" }
                        contentItem: Text {
                            text: parent.text; color: "#FFFFFF"; font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Button {
                        visible: platformHelper.isAndroid
                        text: qsTr("保存到相册")
                        Layout.preferredHeight: 38
                        leftPadding: 20; rightPadding: 20
                        onClicked: {
                            var saved = platformHelper.saveToPublicStorage(lightbox.localPath, lightbox.imageName, "image")
                            lightbox.toastRequested(saved.length > 0 ? qsTr("已保存到相册") : qsTr("保存失败"))
                        }
                        background: Rectangle { radius: 19; color: parent.hovered ? lightbox.theme.accentHover : lightbox.theme.accent }
                        contentItem: Text {
                            text: parent.text; color: "#FFFFFF"; font.pixelSize: 13; font.weight: Font.Medium
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Button {
                        text: qsTr("关闭")
                        Layout.preferredHeight: 38
                        leftPadding: 22; rightPadding: 22
                        onClicked: lightbox.close()
                        background: Rectangle { radius: 19; color: parent.hovered ? "#44FFFFFF" : "#24FFFFFF" }
                        contentItem: Text {
                            text: parent.text; color: "#FFFFFF"; font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Item { Layout.fillWidth: true }
                }
            }
        }
    }
}
