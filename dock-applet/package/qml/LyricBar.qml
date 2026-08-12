import QtQuick
import QtQuick.Controls
import org.deepin.lyricsdock 1.0

Rectangle {
    id: root

    property string currentText: ""
    property string secondaryText: ""
    property real lineProgress: 0
    property bool progressVisible: false
    property bool visualizerVisible: false
    property var visualizerLevels: []
    // 歌词滚动用于展示被裁切的内容，因此不能被宿主误判的装饰动画偏好关闭。
    // Lyric scrolling reveals clipped content, so a host's decorative-motion preference must not disable it.
    property bool motionEnabled: true
    property bool progressSmoothingEnabled: true
    property int progressAnimationDuration: 220
    property int marqueeStartDelay: 800
    property int lyricTransitionDuration: 280
    property real animatedProgress: 0
    property string displayedCurrentText: ""
    property string displayedSecondaryText: ""
    property string outgoingCurrentText: ""
    property bool lyricTransitionActive: false

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

    function resetLyricLines() {
        lyricTransition.stop()
        lyricTransitionActive = false
        displayedCurrentText = currentText
        displayedSecondaryText = secondaryText
        outgoingCurrentText = ""
        currentLine.y = textArea.currentLineRestY
        currentLine.opacity = 1
        currentLine.scale = 1
        secondaryLine.y = textArea.secondaryLineRestY
        secondaryLine.opacity = secondaryText.length > 0 ? 1 : 0
        outgoingLine.y = textArea.currentLineRestY
        outgoingLine.opacity = 0
        outgoingLine.visible = false
    }

    function transitionLyricLines() {
        // 非歌词状态和首次内容直接显示，避免状态文案或初次加载产生无意义的位移动画。
        // Render non-lyric states and initial content directly so status text never has a meaningless transition.
        if (!progressVisible || !currentText.length || !displayedCurrentText.length
                || !motionEnabled || lyricTransitionDuration <= 0) {
            resetLyricLines()
            return
        }

        lyricTransition.stop()
        outgoingCurrentText = displayedCurrentText
        displayedCurrentText = currentText
        displayedSecondaryText = secondaryText
        lyricTransitionActive = true

        outgoingLine.visible = outgoingCurrentText.length > 0
        outgoingLine.y = textArea.currentLineRestY
        outgoingLine.opacity = 1
        currentLine.y = textArea.secondaryLineRestY
        currentLine.opacity = 1
        currentLine.scale = 0.96
        secondaryLine.y = textArea.secondaryLineRestY + LyricsTokens.dockLineHeight
        secondaryLine.opacity = 0
        lyricTransition.start()
    }

    onLineProgressChanged: updateProgress()
    onCurrentTextChanged: {
        progressAnimation.stop()
        animatedProgress = Math.max(0, Math.min(1, lineProgress))
        transitionLyricLines()
    }
    onSecondaryTextChanged: {
        if (lyricTransitionActive)
            displayedSecondaryText = secondaryText
        else
            resetLyricLines()
    }
    onProgressVisibleChanged: {
        updateProgress()
        resetLyricLines()
    }

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

        readonly property real currentLineRestY: 2
        readonly property real secondaryLineRestY: height - LyricsTokens.dockLineHeight - 2

        MarqueeText {
            id: currentLine

            objectName: "currentLyricLine"

            x: 0
            width: parent.width
            y: textArea.currentLineRestY
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
            text: root.displayedCurrentText
            visible: !root.visualizerVisible
        }

        Text {
            id: secondaryLine

            objectName: "secondaryLyricLine"

            x: 0
            width: parent.width
            y: textArea.secondaryLineRestY
            height: LyricsTokens.dockLineHeight
            color: LyricsTokens.dockLyricSecondaryColor
            elide: Text.ElideRight
            font.pixelSize: LyricsTokens.dockSecondaryFontSize
            horizontalAlignment: Text.AlignLeft
            text: root.displayedSecondaryText
            verticalAlignment: Text.AlignVCenter
            visible: text.length > 0
                     && !root.visualizerVisible
        }

        Item {
            id: visualizer

            objectName: "audioVisualizer"

            anchors.fill: parent
            visible: root.visualizerVisible

            Row {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 2
                anchors.rightMargin: 2
                spacing: 3

                Repeater {
                    model: 16

                    Rectangle {
                        readonly property real level: index < root.visualizerLevels.length
                                                      ? Math.max(0, Math.min(1,
                                                          Number(root.visualizerLevels[index]))) : 0
                        width: Math.max(2, (visualizer.width - 4 - 15 * 3) / 16)
                        height: Math.max(3, level * (LyricsTokens.dockVisualHeight - 10))
                        anchors.verticalCenter: parent.verticalCenter
                        color: LyricsTokens.dockLyricProgressColor
                        radius: 1

                        Behavior on height {
                            NumberAnimation { duration: 50; easing.type: Easing.OutQuad }
                        }
                    }
                }
            }
        }

        MarqueeText {
            id: outgoingLine

            objectName: "outgoingLyricLine"

            x: 0
            width: parent.width
            y: textArea.currentLineRestY
            height: LyricsTokens.dockLineHeight
            color: LyricsTokens.dockLyricCurrentColor
            pixelSize: LyricsTokens.dockCurrentFontSize
            weight: Font.Medium
            marqueeEnabled: false
            text: root.outgoingCurrentText
            visible: false
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

    SequentialAnimation {
        id: lyricTransition

        ParallelAnimation {
            NumberAnimation {
                target: outgoingLine
                property: "y"
                to: textArea.currentLineRestY - LyricsTokens.dockLineHeight * 0.45
                duration: root.lyricTransitionDuration
                easing.type: Easing.InCubic
            }
            NumberAnimation {
                target: outgoingLine
                property: "opacity"
                to: 0
                duration: root.lyricTransitionDuration
                easing.type: Easing.InQuad
            }
            NumberAnimation {
                target: currentLine
                property: "y"
                to: textArea.currentLineRestY
                duration: root.lyricTransitionDuration
                // 使用超过终点再回落的贝塞尔曲线，形成轻微的上弹感。
                // A Bezier curve that overshoots its endpoint creates the subtle upward bounce.
                easing.type: Easing.BezierSpline
                easing.bezierCurve: [0.18, 0.90, 0.28, 1.16, 1.0, 1.0]
            }
            NumberAnimation {
                target: currentLine
                property: "scale"
                to: 1
                duration: root.lyricTransitionDuration
                easing.type: Easing.BezierSpline
                easing.bezierCurve: [0.18, 0.90, 0.28, 1.16, 1.0, 1.0]
            }
            NumberAnimation {
                target: secondaryLine
                property: "y"
                to: textArea.secondaryLineRestY
                duration: root.lyricTransitionDuration
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: secondaryLine
                property: "opacity"
                to: root.displayedSecondaryText.length > 0 ? 1 : 0
                duration: root.lyricTransitionDuration
                easing.type: Easing.OutQuad
            }
        }
        ScriptAction {
            script: {
                root.lyricTransitionActive = false
                outgoingLine.visible = false
            }
        }
    }
}
