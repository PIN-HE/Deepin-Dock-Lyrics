import QtQuick
import org.deepin.ds 1.0

AppletItem {
    // The right Dock area accepts applets where 20 < dockOrder <= 30.
    property int dockOrder: 24

    // S06 supplies the visible lyric surface. The skeleton must not reserve Dock space.
    visible: false
    implicitWidth: 0
    implicitHeight: Panel.rootObject ? Panel.rootObject.dockSize : 36
}
