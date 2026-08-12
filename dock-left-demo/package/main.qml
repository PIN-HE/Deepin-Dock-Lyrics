import QtQuick
import QtQuick.Controls
import org.deepin.ds 1.0

AppletItem {
    id: root

    // The Dock's right area accepts items with 20 < dockOrder <= 30.
    property int dockOrder: 24
    property bool shouldVisible: true

    implicitWidth: 156
    implicitHeight: Panel.rootObject ? Panel.rootObject.dockSize : 36

    Rectangle {
        width: parent.width - 6
        height: 36
        anchors.centerIn: parent
        color: "#20ffffff"
        border.color: "#35ffffff"
        border.width: 1
        radius: 5
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        text: "Demo area"
        color: "#f5ffffff"
        font.pixelSize: 12
    }

    ToolButton {
        id: closeButton
        anchors.right: parent.right
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        width: 28
        height: 28
        icon.name: "window-close"
        icon.width: 16
        icon.height: 16
        hoverEnabled: true
        Accessible.name: "Close demo area"

        background: Rectangle {
            color: closeButton.down ? "#45ffffff" : (closeButton.hovered ? "#30ffffff" : "transparent")
            radius: 4
        }

        ToolTip.visible: closeButton.hovered
        ToolTip.text: "Close"
        ToolTip.delay: 500

        onClicked: root.shouldVisible = false
    }
}
