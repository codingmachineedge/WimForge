import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Pane {
    id: root
    property string eyebrow: ""
    property string value: ""
    property string detail: ""
    property color accent: Material.accent
    property string glyph: "●"
    readonly property color effectiveAccent: Material.theme === Material.Dark ? Qt.lighter(accent, 1.65) : accent

    implicitWidth: 230
    implicitHeight: 142
    padding: 18
    Accessible.name: eyebrow + ": " + value + ". " + detail

    background: Rectangle {
        radius: 18
        color: Material.theme === Material.Dark ? "#211F26" : "#FFFBFE"
        border.color: Material.theme === Material.Dark ? "#49454F" : "#E7E0EC"
        border.width: 1
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: root.eyebrow
                color: Material.theme === Material.Dark ? "#CAC4D0" : "#625B71"
                font.pixelSize: 12
                font.weight: Font.DemiBold
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Item { Layout.fillWidth: true }
            Rectangle {
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                radius: 10
                color: Qt.rgba(root.effectiveAccent.r, root.effectiveAccent.g, root.effectiveAccent.b, 0.16)
                Label { anchors.centerIn: parent; text: root.glyph; color: root.effectiveAccent; font.pixelSize: 14; Accessible.ignored: true }
            }
        }
        Label {
            text: root.value
            font.pixelSize: 26
            font.weight: Font.Bold
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        Label {
            text: root.detail
            color: Material.theme === Material.Dark ? "#CAC4D0" : "#625B71"
            font.pixelSize: 12
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
    }
}
