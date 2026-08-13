import QtQuick
import QtQuick.Controls
import org.deepin.ds 1.0
import org.deepin.ds.dock 1.0
import org.deepin.lyricsdock 1.0
import "qml"

AppletItem {
    id: root

    readonly property var viewModel: Applet.viewModel
    readonly property var panelRoot: Panel.rootObject
    readonly property int panelPosition: Panel.position === undefined ? Dock.Bottom : Panel.position
    readonly property int dockSize: panelRoot ? panelRoot.dockSize : LyricsTokens.dockVisualHeight
    readonly property bool useColumnLayout: panelPosition % 2
    readonly property int lyricExtent: Math.max(LyricsTokens.dockLyricWidthMin,
                                                Math.min(LyricsTokens.dockLyricWidthDefault,
                                                         LyricsTokens.dockLyricWidthMax))
    property int dockOrder: 24
    property bool shouldVisible: viewModel.serviceAvailable
                                 && viewModel.enabled
                                 && !viewModel.sessionHidden

    // Dock keeps an item's implicit dimensions in its layout even after QML
    // visibility changes, so release the slot explicitly when the service exits.
    implicitWidth: shouldVisible ? (useColumnLayout ? dockSize : lyricExtent) : 0
    implicitHeight: shouldVisible ? (useColumnLayout ? lyricExtent : dockSize) : 0
    enabled: shouldVisible
    visible: shouldVisible

    onShouldVisibleChanged: {
        if (!shouldVisible)
            detailsPopup.close()
    }

    function statusText() {
        if (!viewModel.serviceAvailable)
            return qsTr("Lyrics service is not running")
        switch (viewModel.status) {
        case "Disabled":
            return qsTr("Dock lyrics are disabled")
        case "WaitingForPlayer":
            return qsTr("Waiting for a music player")
        case "WaitingForTrack":
            return qsTr("Play a song to show lyrics")
        case "LookingUpLyrics":
            return qsTr("Matching lyrics")
        case "NoLyrics":
            return qsTr("No lyrics found")
        case "NeedsCandidateSelection":
            return qsTr("Select a lyric match in Settings")
        case "Error":
            return qsTr("Lyrics are temporarily unavailable")
        default:
            return qsTr("Waiting for lyrics")
        }
    }

    function openDetails() {
        var center = root.mapToItem(null, root.width / 2, root.height / 2)
        detailsPopup.DockPanelPositioner.bounding = Qt.rect(center.x, center.y,
                                                            detailsPopup.width,
                                                            detailsPopup.height)
        detailsPopup.open()
    }

    LyricBar {
        id: lyricBar

        width: root.lyricExtent
        height: LyricsTokens.dockVisualHeight
        anchors.centerIn: parent
        rotation: root.useColumnLayout ? (root.panelPosition === Dock.Right ? 90 : -90) : 0
        // 布局样式由设置项驱动（经典/卡拉 OK）。
        // Layout style is driven by the settings (classic/karaoke).
        karaokeLayout: viewModel.lyricLayout === "karaoke"
        currentText: viewModel.status === "LyricsReady" ? viewModel.currentText : root.statusText()
        secondaryText: viewModel.status === "LyricsReady"
                       ? (viewModel.translationText.length > 0
                          ? viewModel.translationText : viewModel.secondaryText) : ""
        lineProgress: viewModel.status === "LyricsReady" ? viewModel.lineProgress : 0
        progressVisible: viewModel.status === "LyricsReady"
                         && viewModel.timingCapability === "line"
        visualizerVisible: viewModel.audioVisualizerEnabled
                           || viewModel.status !== "LyricsReady"
        visualizerLevels: viewModel.visualizerLevels
        artUrl: viewModel.artUrl

        onActivated: root.openDetails()
        onHideRequested: viewModel.setSessionHidden(true)
    }

    LyricsPopup {
        id: detailsPopup

        popupX: root.panelRoot && DockPanelPositioner.x !== undefined
                ? DockPanelPositioner.x : 0
        popupY: root.panelRoot && DockPanelPositioner.y !== undefined
                ? DockPanelPositioner.y : 0
        // 与 Dock 歌词条一致的布局样式。
        // Same layout style as the Dock lyric bar.
        karaokeLayout: viewModel.lyricLayout === "karaoke"
        previousText: viewModel.previousText
        currentText: viewModel.status === "LyricsReady" ? viewModel.currentText : root.statusText()
        nextText: viewModel.status === "LyricsReady"
                  ? (viewModel.translationText.length > 0
                     ? viewModel.translationText : viewModel.secondaryText) : ""
        sourceText: viewModel.source === "lrclib" ? "LRCLIB" : ""
        timingText: viewModel.timingCapability === "line"
                    ? qsTr("Line-synchronised lyrics")
                    : qsTr("Plain lyrics")
        lyricsAvailable: viewModel.status === "LyricsReady"
                         && viewModel.currentText.length > 0
        visualizerVisible: viewModel.audioVisualizerEnabled || !lyricsAvailable
        visualizerLevels: viewModel.visualizerLevels
        positionMs: viewModel.positionMs
        durationMs: viewModel.durationMs
        artUrl: viewModel.artUrl

        onOpenSettingsRequested: {
            viewModel.openSettings()
            detailsPopup.close()
        }
    }
}
