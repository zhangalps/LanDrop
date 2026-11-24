import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: fileCard
    required property QtObject theme
    property string msgId: ""
    property string fileName: ""
    property string filePath: ""
    property string fileExtension: "FILE"
    property string fileSizeText: ""
    property string status: "completed"
    property real progress: 0.0
    property bool outgoing: false
    property bool isGroupChat: false
    property real maxWidth: 300

    signal downloadRequested(string msgId)
    signal cancelRequested(string msgId)
    signal openRequested(string filePath)
    signal revealRequested(string filePath)
    signal saveRequested(string filePath, string fileName)

    readonly property bool available: status === "available"
    readonly property bool transferring: status === "transferring"
    readonly property bool done: status === "completed"

    implicitWidth: Math.min(maxWidth, 300)
    implicitHeight: cardCol.implicitHeight + 24
    radius: 16
    color: theme.surfaceBg
    border.width: 1
    border.color: theme.divider

    ColumnLayout {
        id: cardCol
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Rectangle {
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
                radius: 12
                color: fileCard.theme.badgeBg

                Text {
                    anchors.centerIn: parent
                    text: fileCard.fileExtension.length > 4
                          ? fileCard.fileExtension.slice(0, 4)
                          : fileCard.fileExtension
                    color: fileCard.theme.accent
                    font.pixelSize: 11
                    font.weight: Font.Bold
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                Text {
                    Layout.fillWidth: true
                    text: fileCard.fileName
                    color: fileCard.theme.textPrimary
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    elide: Text.ElideMiddle
                }

                Text {
                    Layout.fillWidth: true
                    text: {
                        if (fileCard.available) return fileCard.fileSizeText + " · " + qsTr("点击下载")
                        if (fileCard.transferring) return fileCard.fileSizeText + " · " + Math.round(fileCard.progress * 100) + "%"
                        if (fileCard.status === "canceled") return fileCard.fileSizeText + " · " + qsTr("已取消")
                        if (fileCard.status === "failed") return fileCard.fileSizeText + " · " + qsTr("传输失败")
                        return fileCard.fileSizeText
                    }
                    color: fileCard.theme.textMuted
                    font.pixelSize: 12
                }
            }
        }

        Rectangle {
            visible: fileCard.transferring
            Layout.fillWidth: true
            Layout.preferredHeight: 3
            radius: 1.5
            color: fileCard.theme.divider

            Rectangle {
                width: parent.width * fileCard.progress
                height: parent.height
                radius: 1.5
                color: fileCard.theme.accent
            }
        }

        RowLayout {
            visible: fileCard.available || fileCard.transferring || fileCard.done
            Layout.fillWidth: true
            spacing: 8

            Item { Layout.fillWidth: true }

            Button {
                visible: fileCard.available && fileCard.isGroupChat
                text: qsTr("下载")
                Layout.preferredHeight: 28
                leftPadding: 12
                rightPadding: 12
                onClicked: fileCard.downloadRequested(fileCard.msgId)

                background: Rectangle {
                    radius: 8
                    color: parent.hovered ? fileCard.theme.divider : fileCard.theme.accentSoft
                }
                contentItem: Text {
                    text: parent.text
                    color: fileCard.theme.accent
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                visible: fileCard.transferring
                text: qsTr("取消")
                Layout.preferredHeight: 28
                leftPadding: 10
                rightPadding: 10
                onClicked: fileCard.cancelRequested(fileCard.msgId)

                background: Rectangle {
                    radius: 8
                    color: parent.hovered ? fileCard.theme.pageBg : "transparent"
                }
                contentItem: Text {
                    text: parent.text
                    color: fileCard.theme.textMuted
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                visible: fileCard.done && !fileCard.outgoing && platformHelper.isWindows
                text: qsTr("打开文件夹")
                Layout.preferredHeight: 28
                leftPadding: 10
                rightPadding: 10
                onClicked: fileCard.revealRequested(fileCard.filePath)

                background: Rectangle {
                    radius: 8
                    color: parent.hovered ? fileCard.theme.divider : fileCard.theme.pageBg
                }
                contentItem: Text {
                    text: parent.text
                    color: fileCard.theme.textBody
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                visible: fileCard.done && !fileCard.outgoing && platformHelper.isAndroid
                text: qsTr("保存")
                Layout.preferredHeight: 28
                leftPadding: 10
                rightPadding: 10
                onClicked: fileCard.saveRequested(fileCard.filePath, fileCard.fileName)

                background: Rectangle {
                    radius: 8
                    color: parent.hovered ? fileCard.theme.divider : fileCard.theme.pageBg
                }
                contentItem: Text {
                    text: parent.text
                    color: fileCard.theme.textBody
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Button {
                visible: fileCard.done
                text: qsTr("打开")
                Layout.preferredHeight: 28
                leftPadding: 12
                rightPadding: 12
                onClicked: fileCard.openRequested(fileCard.filePath)

                background: Rectangle {
                    radius: 8
                    color: parent.hovered ? fileCard.theme.divider : fileCard.theme.accentSoft
                }
                contentItem: Text {
                    text: parent.text
                    color: fileCard.theme.accent
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}
