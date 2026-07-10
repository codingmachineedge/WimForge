import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

Item {
    id: root
    property string message: ""
    property string tone: "info"
    property string actionText: ""
    property bool motionEnabled: true
    readonly property bool darkTheme: Material.theme === Material.Dark
    readonly property string toneLabel: tone === "error" ? qsTr("Error")
                                                : tone === "warning" ? qsTr("Warning")
                                                : tone === "success" ? qsTr("Success")
                                                : qsTr("Information")
    readonly property color toneBackground: tone === "error" ? (darkTheme ? "#93000A" : "#FFDAD6")
                                                  : tone === "warning" ? (darkTheme ? "#5D3A00" : "#FFE1B3")
                                                  : tone === "success" ? (darkTheme ? "#285021" : "#D6E8CF")
                                                  : (darkTheme ? "#4A4458" : "#E8DEF8")
    readonly property color toneForeground: tone === "error" ? (darkTheme ? "#FFDAD6" : "#410002")
                                                  : tone === "warning" ? (darkTheme ? "#FFE1B3" : "#3E2700")
                                                  : tone === "success" ? (darkTheme ? "#D6E8CF" : "#16380D")
                                                  : (darkTheme ? "#F5EEFF" : "#1D192B")
    readonly property color toneBorder: tone === "error" ? (darkTheme ? "#FFB4AB" : "#BA1A1A")
                                              : tone === "warning" ? (darkTheme ? "#FFB95C" : "#744B00")
                                              : tone === "success" ? (darkTheme ? "#8BD7A6" : "#386A20")
                                              : (darkTheme ? "#D0BCFF" : "#6750A4")
    signal actionTriggered()

    function show(text, kind, action) {
        message = text
        tone = kind || "info"
        actionText = action || ""
        visible = true
        hideTimer.restart()
    }

    visible: false
    implicitWidth: Math.min(620, parent ? parent.width - 48 : 620)
    implicitHeight: 58
    z: 1000
    Accessible.name: toneLabel + ": " + message

    Rectangle {
        anchors.fill: parent
        radius: 15
        color: root.toneBackground
        border.color: root.toneBorder

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 10
            spacing: 12
            Label {
                text: root.toneLabel
                color: root.toneForeground
                font.pixelSize: 13
                font.weight: Font.Bold
            }
            Label {
                text: root.message
                color: root.toneForeground
                wrapMode: Text.Wrap
                Layout.fillWidth: true
                maximumLineCount: 2
                elide: Text.ElideRight
            }
            Button {
                visible: root.actionText.length > 0
                text: root.actionText
                flat: true
                Material.foreground: root.toneForeground
                Accessible.name: root.actionText
                onClicked: { root.actionTriggered(); root.visible = false }
            }
            ToolButton {
                text: "×"
                Material.foreground: root.toneForeground
                Accessible.name: qsTr("Dismiss notification")
                onClicked: root.visible = false
                ToolTip.visible: hovered
                ToolTip.text: Accessible.name
            }
        }
    }

    Timer {
        id: hideTimer
        interval: 5200
        onTriggered: root.visible = false
    }

    Behavior on opacity { NumberAnimation { duration: root.motionEnabled ? 180 : 0 } }
}
