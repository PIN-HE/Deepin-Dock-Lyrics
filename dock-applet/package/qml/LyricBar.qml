import QtQuick
import QtQuick.Controls
import org.deepin.lyricsdock 1.0

Rectangle {
    id: root

    property string currentText: ""
    property string secondaryText: ""
    property real lineProgress: 0
    property bool progressVisible: false

    signal activated()
    signal hideRequested()

    color: lyricArea.containsMouse ? LyricsTokens.surfaceDockHover : LyricsTokens.surfaceDock
    radius: LyricsTokens.dockLyricRadius
    border.color: LyricsTokens.borderSubtle
    border.width: 1
    clip: true

    Behavior on color {
        ColorAnimation { duration: LyricsTokens.motionFast }
    }

    Item {
        id: textArea

        anchors.left: parent.left
        anchors.leftMargin: LyricsTokens.space3
        anchors.right: closeButton.left
        anchors.rightMargin: LyricsTokens.space1
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        Text {
            id: currentLine

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 2
            height: LyricsTokens.dockLineHeight
            color: LyricsTokens.dockLyricCurrentColor
            elide: Text.ElideRight
            font.pixelSize: LyricsTokens.dockCurrentFontSize
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignLeft
            text: root.currentText
            verticalAlignment: Text.AlignVCenter
        }

        Item {
            anchors.left: currentLine.left
            anchors.top: currentLine.top
            width: Math.max(0, Math.min(1, root.lineProgress)) * currentLine.width
            height: currentLine.height
            clip: true
            visible: root.progressVisible && root.currentText.length > 0

            Text {
                width: currentLine.width
                height: currentLine.height
                color: LyricsTokens.dockLyricProgressColor
                elide: Text.ElideRight
                font: currentLine.font
                horizontalAlignment: Text.AlignLeft
                text: root.currentText
                verticalAlignment: Text.AlignVCenter
            }
        }

        Text {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 2
            height: LyricsTokens.dockLineHeight
            color: LyricsTokens.dockLyricSecondaryColor
            elide: Text.ElideRight
            font.pixelSize: LyricsTokens.dockSecondaryFontSize
            horizontalAlignment: Text.AlignLeft
            text: root.secondaryText
            verticalAlignment: Text.AlignVCenter
            visible: text.length > 0
        }

        MouseArea {
            id: lyricArea

            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            Accessible.name: root.currentText
            onClicked: root.activated()
        }

        ToolTip.visible: lyricArea.containsMouse && root.currentText.length > 0
        ToolTip.text: root.currentText
        ToolTip.delay: 500
    }

    ToolButton {
        id: closeButton

        anchors.right: parent.right
        anchors.rightMargin: LyricsTokens.space1
        anchors.verticalCenter: parent.verticalCenter
        width: LyricsTokens.dockCloseHitSize
        height: LyricsTokens.dockCloseHitSize
        icon.name: "window-close"
        icon.width: 16
        icon.height: 16
        hoverEnabled: true
        focusPolicy: Qt.TabFocus
        Accessible.name: qsTr("Hide Dock lyrics")

        background: Rectangle {
            color: LyricsTokens.surfaceDockHover
            opacity: closeButton.hovered || closeButton.down ? 1 : 0
            radius: LyricsTokens.dockLyricRadius
        }

        ToolTip.visible: closeButton.hovered
        ToolTip.text: qsTr("Hide Dock lyrics")
        ToolTip.delay: 500
        onClicked: root.hideRequested()
    }
}
