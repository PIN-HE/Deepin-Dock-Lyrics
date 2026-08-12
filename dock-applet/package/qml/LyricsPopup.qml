import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.deepin.ds 1.0
import org.deepin.lyricsdock 1.0

PanelPopup {
    id: root

    property string previousText: ""
    property string currentText: ""
    property string nextText: ""
    property string sourceText: ""
    property string timingText: ""

    signal openSettingsRequested()

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
                color: LyricsTokens.textTertiary
                elide: Text.ElideRight
                font.pixelSize: LyricsTokens.dockSecondaryFontSize
                text: root.previousText
                visible: text.length > 0
            }

            Text {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: LyricsTokens.textPrimary
                font.pixelSize: LyricsTokens.bodyFontSize
                font.weight: Font.Medium
                horizontalAlignment: Text.AlignHCenter
                maximumLineCount: 3
                text: root.currentText
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.Wrap
            }

            Text {
                Layout.fillWidth: true
                color: LyricsTokens.textSecondary
                elide: Text.ElideRight
                font.pixelSize: LyricsTokens.dockSecondaryFontSize
                horizontalAlignment: Text.AlignHCenter
                text: root.nextText
                visible: text.length > 0
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
                    color: LyricsTokens.textTertiary
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
                    icon.name: "preferences-system"
                    icon.width: 16
                    icon.height: 16
                    hoverEnabled: true
                    focusPolicy: Qt.TabFocus
                    Accessible.name: qsTr("Open lyric settings")

                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Open lyric settings")
                    ToolTip.delay: 500
                    onClicked: root.openSettingsRequested()
                }
            }
        }
    }
}
