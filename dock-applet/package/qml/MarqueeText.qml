import QtQuick
import org.deepin.lyricsdock 1.0

Item {
    id: root

    property string text: ""
    property color color: "transparent"
    property color progressColor: color
    property real progress: 0
    property bool progressVisible: false
    property int pixelSize: 12
    property int weight: Font.Normal
    property bool marqueeEnabled: true
    property int startDelay: 800
    property int endDelay: 800
    property real pixelsPerSecond: 28
    property bool motionEnabled: !LyricsTokens.reduceMotion
    property real contentOffset: 0
    readonly property real maximumOffset: Math.max(0, baseText.implicitWidth - width)

    clip: true

    function restartMarquee() {
        contentOffset = 0
        if (marqueeAnimation.running)
            marqueeAnimation.restart()
    }

    Text {
        id: baseText

        objectName: "marqueeBaseText"
        x: -root.contentOffset
        width: implicitWidth
        height: parent.height
        color: root.color
        elide: Text.ElideNone
        font.pixelSize: root.pixelSize
        font.weight: root.weight
        text: root.text
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.NoWrap
    }

    Item {
        id: progressClip

        objectName: "marqueeProgressClip"
        width: Math.max(0, Math.min(1, root.progress)) * root.width
        height: parent.height
        clip: true
        visible: root.progressVisible && root.text.length > 0

        Text {
            // 与底层文字共享水平位移，确保蓝色进度层始终覆盖同一字形。
            // Share the base text offset so the blue progress layer always covers the same glyphs.
            x: baseText.x
            width: baseText.width
            height: parent.height
            color: root.progressColor
            elide: Text.ElideNone
            font: baseText.font
            text: root.text
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.NoWrap
        }
    }

    SequentialAnimation {
        id: marqueeAnimation

        loops: Animation.Infinite
        running: root.marqueeEnabled && root.motionEnabled && root.maximumOffset > 0

        PauseAnimation { duration: root.startDelay }
        NumberAnimation {
            target: root
            property: "contentOffset"
            from: 0
            to: root.maximumOffset
            duration: Math.max(800, root.maximumOffset / root.pixelsPerSecond * 1000)
            easing.type: Easing.Linear
        }
        PauseAnimation { duration: root.endDelay }
        NumberAnimation {
            target: root
            property: "contentOffset"
            from: root.maximumOffset
            to: 0
            duration: Math.max(800, root.maximumOffset / root.pixelsPerSecond * 1000)
            easing.type: Easing.Linear
        }
    }

    onTextChanged: Qt.callLater(restartMarquee)
    onWidthChanged: Qt.callLater(restartMarquee)
    onMotionEnabledChanged: {
        if (!motionEnabled)
            contentOffset = 0
    }
}
