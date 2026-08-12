import QtQuick
import QtQuick.Controls
import org.deepin.ds 1.0
import org.deepin.lyricsdock 1.0

AppletItem {
    // Dock 右侧区域接收 dockOrder 位于 21 到 30 的 Applet。
    // The right Dock area accepts Applets with a dockOrder between 21 and 30.
    property int dockOrder: 24

    implicitWidth: LyricsTokens.dockLyricWidthDefault
    implicitHeight: Panel.rootObject ? Panel.rootObject.dockSize : 36

    Rectangle {
        id: visual

        width: parent.width - LyricsTokens.space2
        height: Math.min(LyricsTokens.dockVisualHeight, parent.height)
        anchors.centerIn: parent
        color: LyricsTokens.surfaceDock
        radius: LyricsTokens.dockLyricRadius
        border.color: LyricsTokens.borderSubtle
        border.width: 1

        Behavior on color {
            ColorAnimation {
                duration: LyricsTokens.motionFast
            }
        }

        Text {
            id: tokenTitle

            anchors.left: parent.left
            anchors.leftMargin: LyricsTokens.space3
            anchors.right: closeButton.left
            anchors.rightMargin: LyricsTokens.space1
            anchors.verticalCenter: parent.verticalCenter
            color: LyricsTokens.dockLyricCurrentColor
            elide: Text.ElideRight
            font.pixelSize: LyricsTokens.dockCurrentFontSize
            text: qsTr("Lyrics theme ready")
            verticalAlignment: Text.AlignVCenter

            Behavior on color {
                ColorAnimation {
                    duration: LyricsTokens.motionFast
                }
            }
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
            Accessible.name: qsTr("Hide lyric preview")

            background: Rectangle {
                color: LyricsTokens.surfaceDockHover
                opacity: closeButton.hovered || closeButton.down ? 1 : 0
                radius: LyricsTokens.dockLyricRadius

                Behavior on opacity {
                    NumberAnimation {
                        duration: LyricsTokens.motionFast
                    }
                }
            }

            ToolTip.visible: closeButton.hovered
            ToolTip.text: qsTr("Hide preview")
            ToolTip.delay: 500

            // S01 只隐藏令牌预览；S06 改为调用后台服务保存会话隐藏状态。
            // S01 only hides this token preview; S06 will call the daemon for session hiding.
            onClicked: visual.visible = false
        }
    }
}
