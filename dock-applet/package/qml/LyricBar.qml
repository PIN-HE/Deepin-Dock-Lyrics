import QtQuick
import QtQuick.Controls
import Qt5Compat.GraphicalEffects
import org.deepin.lyricsdock 1.0

Rectangle {
    id: root

    property string currentText: ""
    property string secondaryText: ""
    property real lineProgress: 0
    property bool progressVisible: false
    property bool visualizerVisible: false
    property var visualizerLevels: []
    property string artUrl: ""
    // 歌词滚动用于展示被裁切的内容，因此不能被宿主误判的装饰动画偏好关闭。
    // Lyric scrolling reveals clipped content, so a host's decorative-motion preference must not disable it.
    property bool motionEnabled: true
    property bool progressSmoothingEnabled: true
    // 卡拉 OK 布局：当前行居左（活动槽），下一行居右（非活动槽）。
    // Karaoke layout: current line left-aligned (active slot), next line
    // right-aligned (inactive slot).
    property bool karaokeLayout: false
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
        currentLine.scale = 1
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
    onVisualizerVisibleChanged: {
        // A mode switch must cancel any lyric transition before the lyric layer is hidden.
        // 切换可视化模式时先取消歌词转场，防止旧歌词在 Dock 外残留。
        resetLyricLines()
    }

    Behavior on color {
        ColorAnimation { duration: LyricsTokens.motionFast }
    }

    Item {
        id: coverArtItem

        readonly property int artSize: Math.round(LyricsTokens.dockVisualHeight * 0.72)
        readonly property int artMargin: LyricsTokens.space2

        anchors.left: parent.left
        anchors.leftMargin: artMargin
        anchors.verticalCenter: parent.verticalCenter
        width: artUrl.length > 0 ? artSize : 0
        height: artSize

        Behavior on width {
            NumberAnimation { duration: LyricsTokens.motionFast; easing.type: Easing.OutCubic }
        }

        Rectangle {
            anchors.fill: parent
            radius: 3
            color: LyricsTokens.borderSubtle
            visible: coverImage.status !== Image.Ready && parent.width > 0
        }

        Image {
            id: coverImage

            anchors.fill: parent
            source: root.artUrl
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            visible: status === Image.Ready

            layer.enabled: true
            layer.effect: OpacityMask {
                maskSource: Rectangle {
                    width: coverImage.width
                    height: coverImage.height
                    radius: 4
                }
            }
        }
    }

    Item {
        id: textArea

        anchors.left: coverArtItem.right
        anchors.leftMargin: coverArtItem.width > 0 ? coverArtItem.artMargin : LyricsTokens.space3
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
            // 卡拉 OK 布局下下一行居右，与居左的当前行形成左右呼应。
            // In karaoke layout the next line sits right, mirroring the
            // left-aligned current line.
            horizontalAlignment: root.karaokeLayout ? Text.AlignRight : Text.AlignLeft
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
                id: visualizerRow

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                height: Math.max(8, LyricsTokens.dockVisualHeight - 8)
                anchors.leftMargin: 2
                anchors.rightMargin: 2
                spacing: 4

                Repeater {
                    model: 16

                    Rectangle {
                        readonly property real level: index < root.visualizerLevels.length
                                                      ? Math.max(0, Math.min(1,
                                                          Number(root.visualizerLevels[index]))) : 0
                        width: Math.max(2, Math.min(4,
                            (visualizer.width - 4 - 15 * visualizerRow.spacing) / 16))
                        height: Math.max(4, level * (visualizerRow.height - 4))
                        y: (visualizerRow.height - height) / 2
                        color: LyricsTokens.dockLyricVisualizerColor
                        radius: width / 2

                        Behavior on height {
                            NumberAnimation { duration: 70; easing.type: Easing.OutCubic }
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
                to: textArea.currentLineRestY - LyricsTokens.dockLineHeight
                duration: root.lyricTransitionDuration
                easing.type: Easing.Linear
            }
            NumberAnimation {
                target: outgoingLine
                property: "opacity"
                to: 0
                duration: root.lyricTransitionDuration
                easing.type: Easing.Linear
            }
            NumberAnimation {
                target: currentLine
                property: "y"
                to: textArea.currentLineRestY
                duration: root.lyricTransitionDuration
                easing.type: Easing.Linear
            }
            NumberAnimation {
                target: secondaryLine
                property: "y"
                to: textArea.secondaryLineRestY
                duration: root.lyricTransitionDuration
                easing.type: Easing.Linear
            }
            NumberAnimation {
                target: secondaryLine
                property: "opacity"
                to: root.displayedSecondaryText.length > 0 ? 1 : 0
                duration: root.lyricTransitionDuration
                easing.type: Easing.Linear
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
