import QtQuick
import QtQuick.Effects

// Renders a monochrome SVG glyph in any theme color. The shipped icon set is
// stroked in a single light color, so colorization must carry the theme.
Item {
    id: root

    property url source
    property color color: "#F1DFD9"
    property int iconSize: 18

    implicitWidth: iconSize
    implicitHeight: iconSize

    Image {
        id: base
        anchors.fill: parent
        source: root.source
        sourceSize.width: root.iconSize * 2
        sourceSize.height: root.iconSize * 2
        fillMode: Image.PreserveAspectFit
        visible: false
        Accessible.ignored: true
    }

    MultiEffect {
        anchors.fill: base
        source: base
        colorization: 1
        colorizationColor: root.color
    }
}
