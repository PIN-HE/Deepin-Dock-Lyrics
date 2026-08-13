import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import org.deepin.ds 1.0
import org.deepin.lyricsdock 1.0

PanelPopup {
    id: root

    property string previousText: ""
    property string currentText: ""
    property string nextText: ""
    property string sourceText: ""
    property string timingText: ""
    property bool lyricsAvailable: false
    property bool visualizerVisible: false
    property var visualizerLevels: []
    property real positionMs: 0
    property real durationMs: -1
    property string artUrl: ""
    property string displayedCurrentText: ""
    property bool lyricTransitionActive: false
    property int lyricTransitionDuration: 220

    readonly property real playbackProgress: durationMs > 0
                                               ? Math.max(0, Math.min(1, positionMs / durationMs))
                                               : 0

    signal openSettingsRequested()

    function resetLyricTransition() {
        popupLyricTransition.stop()
        lyricTransitionActive = false
        displayedCurrentText = currentText
        popupCurrentLine.y = 0
    }

    function transitionLyricText() {
        if (!currentText.length || !displayedCurrentText.length || lyricTransitionDuration <= 0) {
            resetLyricTransition()
            return
        }
        popupLyricTransition.stop()
        displayedCurrentText = currentText
        lyricTransitionActive = true
        popupCurrentLine.y = 48
        popupLyricTransition.start()
    }

    onCurrentTextChanged: transitionLyricText()
    onVisualizerVisibleChanged: resetLyricTransition()

    function formatTime(milliseconds) {
        var totalSeconds = Math.max(0, Math.floor(milliseconds / 1000))
        var minutes = Math.floor(totalSeconds / 60)
        var seconds = totalSeconds % 60
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }

    width: 340
    height: 214
    windowTitle: "deepin-dock-lyrics/details"

    Rectangle {
        anchors.fill: parent
        color: LyricsTokens.surfaceDock
        radius: LyricsTokens.dockLyricRadius
        border.color: LyricsTokens.borderSubtle
        border.width: 1

        Shortcut {
            sequence: StandardKey.Cancel
            context: Qt.WindowShortcut
            onActivated: root.close()
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: LyricsTokens.space3
            spacing: LyricsTokens.space2

            Text {
                Layout.fillWidth: true
                color: LyricsTokens.textSecondary
                elide: Text.ElideRight
                font.pixelSize: LyricsTokens.dockSecondaryFontSize
                text: root.previousText
                visible: text.length > 0
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 82
                Layout.minimumHeight: 82
                Layout.maximumHeight: 82

                Image {
                    id: popupCoverImage

                    anchors.left: parent.left
                    anchors.leftMargin: 2
                    anchors.verticalCenter: parent.verticalCenter
                    width: 60
                    height: 60
                    source: root.artUrl
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    visible: status === Image.Ready
                    layer.enabled: true
                    layer.effect: OpacityMask {
                        maskSource: Rectangle {
                            width: popupCoverImage.width
                            height: popupCoverImage.height
                            radius: 7
                        }
                    }
                }

                Item {
                    id: popupLyricViewport

                    anchors.left: parent.left
                    anchors.leftMargin: popupCoverImage.visible ? 72 : 0
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    visible: !root.visualizerVisible
                    clip: true

                    Item {
                        id: popupCurrentViewport

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        height: 48
                        clip: true

                        Text {
                            id: popupCurrentLine

                            anchors.left: parent.left
                            anchors.right: parent.right
                            y: 0
                            height: 48
                            color: LyricsTokens.textPrimary
                            font.pixelSize: LyricsTokens.bodyFontSize
                            font.weight: Font.Medium
                            horizontalAlignment: Text.AlignHCenter
                            maximumLineCount: 3
                            text: root.displayedCurrentText
                            verticalAlignment: Text.AlignVCenter
                            wrapMode: Text.Wrap
                        }
                    }

                    Text {
                        id: popupSecondaryLine

                        anchors.left: parent.left
                        anchors.right: parent.right
                        y: 50
                        height: 28
                        color: LyricsTokens.textSecondary
                        font.pixelSize: LyricsTokens.dockSecondaryFontSize
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        text: root.nextText
                        verticalAlignment: Text.AlignVCenter
                        visible: text.length > 0
                    }

                }

                Row {
                    anchors.left: popupLyricViewport.left
                    anchors.right: popupLyricViewport.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 5
                    visible: root.visualizerVisible

                    Repeater {
                        model: 16

                        Rectangle {
                            readonly property real level: index < root.visualizerLevels.length
                                                              ? Math.max(0, Math.min(1,
                                                                  Number(root.visualizerLevels[index]))) : 0
                            width: 4
                            height: Math.max(8, level * 52)
                            anchors.verticalCenter: parent.verticalCenter
                            color: LyricsTokens.dockLyricVisualizerColor
                            radius: width / 2

                            Behavior on height {
                                NumberAnimation { duration: 70; easing.type: Easing.OutCubic }
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: LyricsTokens.space2

                Rectangle {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    height: 5
                    radius: height / 2
                    color: LyricsTokens.dockLyricTrackColor

                    Rectangle {
                        width: parent.width * root.playbackProgress
                        height: parent.height
                        radius: height / 2
                        color: LyricsTokens.dockLyricProgressColor
                    }
                }

                Text {
                    color: LyricsTokens.textSecondary
                    font.pixelSize: LyricsTokens.dockSecondaryFontSize
                    text: root.formatTime(root.positionMs) + " / "
                          + (root.durationMs > 0 ? root.formatTime(root.durationMs) : "--:--")
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: LyricsTokens.borderSubtle
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: LyricsTokens.space2

                Text {
                    Layout.fillWidth: true
                    color: LyricsTokens.textSecondary
                    elide: Text.ElideRight
                    font.pixelSize: LyricsTokens.dockSecondaryFontSize
                    text: root.sourceText.length > 0
                          ? root.sourceText + " · " + root.timingText
                          : root.timingText
                }

                ToolButton {
                    id: settingsButton

                    width: LyricsTokens.dockCloseHitSize
                    height: LyricsTokens.dockCloseHitSize
                    icon.source: Qt.resolvedUrl("icons/settings.svg")
                    icon.width: 16
                    icon.height: 16
                    hoverEnabled: true
                    focusPolicy: Qt.TabFocus
                    Accessible.name: qsTr("Open lyric settings")

                    background: Rectangle {
                        radius: 4
                        color: settingsButton.hovered ? Qt.rgba(0, 0, 0, 0.08) : "transparent"
                    }

                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Open lyric settings")
                    ToolTip.delay: 500
                    onClicked: root.openSettingsRequested()
                }
            }

        }  // ColumnLayout

        SequentialAnimation {
            id: popupLyricTransition

            ParallelAnimation {
                NumberAnimation {
                    target: popupCurrentLine
                    property: "y"
                    to: 0
                    duration: root.lyricTransitionDuration
                    easing.type: Easing.Linear
                }
            }
            ScriptAction {
                script: {
                    root.lyricTransitionActive = false
                }
            }
        }
    }
}
