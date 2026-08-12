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
    property bool shouldVisible: !viewModel.sessionHidden

    implicitWidth: useColumnLayout ? dockSize : lyricExtent
    implicitHeight: useColumnLayout ? lyricExtent : dockSize
    enabled: shouldVisible
    visible: shouldVisible

    function statusText() {
        if (!viewModel.serviceAvailable)
            return qsTr("Lyrics service is starting")
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
        currentText: viewModel.status === "LyricsReady" ? viewModel.currentText : root.statusText()
        secondaryText: viewModel.status === "LyricsReady" ? viewModel.secondaryText : ""
        lineProgress: viewModel.status === "LyricsReady" ? viewModel.lineProgress : 0
        progressVisible: viewModel.status === "LyricsReady"
                         && viewModel.timingCapability === "line"
        visualizerVisible: (viewModel.status === "LookingUpLyrics" || viewModel.status === "NoLyrics")
                           && viewModel.visualizerAvailable
        visualizerLevels: viewModel.visualizerLevels

        onActivated: root.openDetails()
        onHideRequested: viewModel.setSessionHidden(true)
    }

    LyricsPopup {
        id: detailsPopup

        popupX: root.panelRoot && DockPanelPositioner.x !== undefined
                ? DockPanelPositioner.x : 0
        popupY: root.panelRoot && DockPanelPositioner.y !== undefined
                ? DockPanelPositioner.y : 0
        previousText: viewModel.previousText
        currentText: viewModel.status === "LyricsReady" ? viewModel.currentText : root.statusText()
        nextText: viewModel.status === "LyricsReady" ? viewModel.secondaryText : ""
        sourceText: viewModel.source === "lrclib" ? "LRCLIB" : ""
        timingText: viewModel.timingCapability === "line"
                    ? qsTr("Line-synchronised lyrics")
                    : qsTr("Plain lyrics")

        onOpenSettingsRequested: {
            viewModel.openSettings()
            detailsPopup.close()
        }
    }
}
