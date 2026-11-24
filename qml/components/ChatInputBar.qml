import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: inputBar
    required property QtObject theme
    property bool connected: false

    signal attachClicked()
    signal sendTextRequested(string text)
    signal toastRequested(string message)

    implicitHeight: inputRow.implicitHeight + (platformHelper.isAndroid ? 30 : 22)
    color: theme.chatBarBg

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: inputBar.theme.divider
    }

    RowLayout {
        id: inputRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.topMargin: 11
        spacing: 10

        Button {
            enabled: inputBar.connected
            Layout.preferredWidth: 44
            Layout.preferredHeight: 44
            Layout.alignment: Qt.AlignBottom
            onClicked: inputBar.attachClicked()

            background: Rectangle {
                radius: 22
                color: parent.hovered ? inputBar.theme.divider : inputBar.theme.inputBg
            }

            contentItem: Canvas {
                id: plusCanvas
                opacity: inputBar.connected ? 1.0 : 0.4
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = inputBar.theme.textPrimary
                    ctx.lineWidth = 1.8
                    ctx.lineCap = "round"
                    ctx.beginPath()
                    ctx.moveTo(width * 0.50, height * 0.30)
                    ctx.lineTo(width * 0.50, height * 0.70)
                    ctx.moveTo(width * 0.30, height * 0.50)
                    ctx.lineTo(width * 0.70, height * 0.50)
                    ctx.stroke()
                }
                Connections {
                    target: inputBar.theme
                    function onTextPrimaryChanged() { plusCanvas.requestPaint() }
                }
            }
        }

        // Full-area touch target input capsule
        Rectangle {
            id: inputCapsule
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(116, Math.max(44, inputArea.implicitHeight + 18))
            radius: 22
            color: inputBar.theme.inputBg
            border.width: inputArea.activeFocus ? 1.2 : 1
            border.color: inputArea.activeFocus ? inputBar.theme.accent : inputBar.theme.divider

            MouseArea {
                anchors.fill: parent
                enabled: inputBar.connected
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                cursorShape: Qt.IBeamCursor
                onClicked: function(mouse) {
                    inputArea.forceActiveFocus()
                    if (mouse.button === Qt.RightButton) inputActionBar.open()
                    else if (platformHelper.isAndroid) Qt.inputMethod.show()
                }
                onPressAndHold: {
                    inputArea.forceActiveFocus()
                    inputActionBar.open()
                }
            }

            // Non-focus-stealing floating pill bar above input
            Popup {
                id: inputActionBar
                x: Math.max(0, Math.min(inputCapsule.width - width, 8))
                y: -height - 8
                padding: 4
                modal: false
                focus: false
                closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape

                background: Rectangle {
                    radius: 14
                    color: inputBar.theme.surfaceBg
                    border.width: 1
                    border.color: inputBar.theme.divider
                }

                contentItem: Row {
                    spacing: 2

                    Button {
                        text: qsTr("粘贴")
                        height: 34
                        leftPadding: 14; rightPadding: 14
                        focusPolicy: Qt.NoFocus
                        onClicked: {
                            var clip = platformHelper.clipboardText()
                            if (clip && clip.length > 0) inputArea.insert(inputArea.cursorPosition, clip)
                            inputActionBar.close()
                        }
                        background: Rectangle { radius: 10; color: parent.hovered ? inputBar.theme.pageBg : "transparent" }
                        contentItem: Text {
                            text: parent.text; color: inputBar.theme.accent; font.pixelSize: 13; font.weight: Font.Medium
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Rectangle { width: 1; height: 16; anchors.verticalCenter: parent.verticalCenter; color: inputBar.theme.divider }

                    Button {
                        text: qsTr("复制")
                        height: 34
                        leftPadding: 14; rightPadding: 14
                        focusPolicy: Qt.NoFocus
                        enabled: inputArea.text.length > 0
                        onClicked: {
                            var txt = inputArea.selectedText.length > 0 ? inputArea.selectedText : inputArea.text
                            platformHelper.copyToClipboard(txt)
                            inputBar.toastRequested(qsTr("已复制"))
                            inputActionBar.close()
                        }
                        background: Rectangle { radius: 10; color: parent.hovered ? inputBar.theme.pageBg : "transparent" }
                        contentItem: Text {
                            text: parent.text; color: parent.enabled ? inputBar.theme.textPrimary : inputBar.theme.textMuted
                            font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Rectangle { width: 1; height: 16; anchors.verticalCenter: parent.verticalCenter; color: inputBar.theme.divider }

                    Button {
                        text: qsTr("全选")
                        height: 34
                        leftPadding: 14; rightPadding: 14
                        focusPolicy: Qt.NoFocus
                        enabled: inputArea.text.length > 0
                        onClicked: { inputArea.selectAll(); inputActionBar.close() }
                        background: Rectangle { radius: 10; color: parent.hovered ? inputBar.theme.pageBg : "transparent" }
                        contentItem: Text {
                            text: parent.text; color: parent.enabled ? inputBar.theme.textPrimary : inputBar.theme.textMuted
                            font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Rectangle { width: 1; height: 16; anchors.verticalCenter: parent.verticalCenter; color: inputBar.theme.divider }

                    Button {
                        text: qsTr("清空")
                        height: 34
                        leftPadding: 14; rightPadding: 14
                        focusPolicy: Qt.NoFocus
                        enabled: inputArea.text.length > 0
                        onClicked: { inputArea.clear(); inputActionBar.close() }
                        background: Rectangle { radius: 10; color: parent.hovered ? inputBar.theme.pageBg : "transparent" }
                        contentItem: Text {
                            text: parent.text; color: parent.enabled ? inputBar.theme.dangerText : inputBar.theme.textMuted
                            font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }

            Flickable {
                id: inputFlick
                anchors.fill: parent
                anchors.leftMargin: 16; anchors.rightMargin: 16
                anchors.topMargin: 6; anchors.bottomMargin: 6
                contentWidth: width
                contentHeight: Math.max(height, inputArea.implicitHeight)
                clip: true

                TextArea {
                    id: inputArea
                    width: inputFlick.width
                    height: Math.max(inputFlick.height, implicitHeight)
                    enabled: inputBar.connected
                    color: inputBar.theme.textPrimary
                    placeholderText: inputBar.connected ? qsTr("输入消息") : ""
                    placeholderTextColor: inputBar.theme.textMuted
                    font.pixelSize: 14
                    wrapMode: TextArea.Wrap
                    verticalAlignment: TextEdit.AlignVCenter
                    selectByMouse: !platformHelper.isAndroid
                    background: null
                    padding: 0

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.RightButton | (platformHelper.isAndroid ? Qt.LeftButton : Qt.NoButton)
                        propagateComposedEvents: true
                        onClicked: function(mouse) {
                            if (mouse.button === Qt.RightButton) {
                                inputArea.forceActiveFocus()
                                inputActionBar.open()
                            } else {
                                mouse.accepted = false
                            }
                        }
                        onPressAndHold: {
                            inputArea.forceActiveFocus()
                            inputActionBar.open()
                        }
                    }

                    Keys.onReturnPressed: function(event) {
                        if (event.modifiers & Qt.ShiftModifier) {
                            event.accepted = false
                        } else {
                            event.accepted = true
                            if (inputArea.text.trim().length > 0) {
                                inputBar.sendTextRequested(inputArea.text)
                                inputArea.clear()
                            }
                        }
                    }
                }
            }
        }

        Button {
            enabled: inputBar.connected && inputArea.text.trim().length > 0
            Layout.preferredWidth: 68
            Layout.preferredHeight: 44
            Layout.alignment: Qt.AlignBottom
            text: qsTr("发送")
            onClicked: {
                inputBar.sendTextRequested(inputArea.text)
                inputArea.clear()
            }

            background: Rectangle {
                radius: 22
                color: !parent.enabled
                       ? inputBar.theme.divider
                       : (parent.hovered ? inputBar.theme.accentHover : inputBar.theme.accent)
            }
            contentItem: Text {
                text: parent.text
                color: parent.enabled ? "#FFFFFF" : inputBar.theme.textMuted
                font.pixelSize: 14
                font.weight: Font.Medium
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
