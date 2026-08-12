import QtQuick
import QtQuick.Controls
import org.deepin.lyricsdock 1.0

Rectangle {
    id: root

    property string currentText: ""
    property string secondaryText: ""
    property real lineProgress: 0
    property bool progressVisible: false
    // 歌词滚动用于展示被裁切的内容，因此不能被宿主误判的装饰动画偏好关闭。
    // Lyric scrolling reveals clipped content, so a host's decorative-motion preference must not disable it.
    property bool motionEnabled: true
    property bool progressSmoothingEnabled: true
    property int progressAnimationDuration: 220
    property int marqueeStartDelay: 800
    property real animatedProgress: 0

    signal activated()
    signal hideRequested()

    color: lyricArea.containsMouse ? LyricsTokens.surfaceDockHover : LyricsTokens.surfaceDock
    radius: LyricsTokens.dockLyricRadius
    border.color: LyricsTokens.borderSubtle
    border.width: 1
    clip: true

    function updateProgress() {
        var next = Math.max(0, Math.min(1, lineProgress))
        // 仅补间连续播放样本；Seek、换行或回退必须立即复位。
        // Interpolate continuous playback samples only; seeks, line changes, and rewinds reset immediately.
        var advancesNaturally = next >= animatedProgress && next - animatedProgress <= 0.20
        // 功能性时间进度不跟随“减少动画”；该设置只关闭跑马灯等装饰性运动。
        // Functional time progress ignores "reduce motion"; it only disables decorative motion such as the marquee.
        if (!progressSmoothingEnabled || !progressVisible || !advancesNaturally) {
            progressAnimation.stop()
            animatedProgress = next
            return
        }
        progressAnimation.stop()
        progressAnimation.from = animatedProgress
        progressAnimation.to = next
        progressAnimation.start()
    }

    onLineProgressChanged: updateProgress()
    onCurrentTextChanged: {
        progressAnimation.stop()
        animatedProgress = Math.max(0, Math.min(1, lineProgress))
    }
    onProgressVisibleChanged: updateProgress()

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

        MarqueeText {
            id: currentLine

            objectName: "currentLyricLine"

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 2
            height: LyricsTokens.dockLineHeight
            color: LyricsTokens.dockLyricCurrentColor
            progressColor: LyricsTokens.dockLyricProgressColor
            progress: root.animatedProgress
            progressVisible: root.progressVisible
            pixelSize: LyricsTokens.dockCurrentFontSize
            weight: Font.Medium
            marqueeEnabled: true
            motionEnabled: root.motionEnabled
            startDelay: root.marqueeStartDelay
            text: root.currentText
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

    NumberAnimation {
        id: progressAnimation

        target: root
        property: "animatedProgress"
        duration: root.progressAnimationDuration
        easing.type: Easing.Linear
    }
}
