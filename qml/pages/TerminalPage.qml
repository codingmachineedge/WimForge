pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property var app
    required property var terminal
    required property var tr

    readonly property color cardColor: Material.theme === Material.Dark ? "#211F26" : "#FFFBFE"
    readonly property color outlineColor: Material.theme === Material.Dark ? "#49454F" : "#E7E0EC"
    readonly property color secondaryText: Material.theme === Material.Dark ? "#CAC4D0" : "#625B71"
    readonly property color successText: Material.theme === Material.Dark ? "#A8D5A2" : "#386A20"
    readonly property color errorText: Material.theme === Material.Dark ? "#FFB4AB" : "#BA1A1A"
    readonly property int terminalColumns: Math.max(40, Math.floor(Math.max(360, terminalOutput.width) / 8.2))
    readonly property int terminalRows: Math.max(12, Math.floor(Math.max(220, terminalOutput.height) / 18))

    property var commandHistory: []
    property int historyIndex: commandHistory.length

    function statusText() {
        if (root.terminal.running)
            return root.tr("Running inside WimForge", "正在 WimForge 內運行")
        if (root.terminal.errorString.length > 0)
            return root.terminal.errorString
        if (root.terminal.exitStatus !== 0) {
            var outcome = root.terminal.exitStatus === 3
                        ? root.tr("Terminated", "已終止")
                        : root.terminal.exitStatus === 2
                          ? root.tr("Crashed", "已崩潰")
                          : root.tr("Exited", "已結束")
            return root.tr("%1 with code %2", "%1，代碼 %2")
                       .arg(outcome).arg(root.terminal.exitCode)
        }
        return root.tr("Ready", "準備好")
    }

    function startSession() {
        var directory = workingDirectory.text.trim()
        if (directory.length === 0 && root.app.projectLoaded)
            directory = root.app.projectRoot
        if (root.terminal.startShell(shellChoice.currentValue, directory,
                                     root.terminalColumns, root.terminalRows)) {
            terminalInput.forceActiveFocus()
        }
    }

    function sendInput() {
        var command = terminalInput.text
        if (command.length === 0 || !root.terminal.running)
            return
        if (root.terminal.writeInput(command + "\r\n")) {
            var nextHistory = root.commandHistory.slice(Math.max(0, root.commandHistory.length - 99))
            if (nextHistory.length === 0 || nextHistory[nextHistory.length - 1] !== command)
                nextHistory.push(command)
            root.commandHistory = nextHistory
            root.historyIndex = root.commandHistory.length
            terminalInput.clear()
        }
    }

    function showPreviousCommand() {
        if (root.commandHistory.length === 0)
            return
        root.historyIndex = Math.max(0, root.historyIndex - 1)
        terminalInput.text = root.commandHistory[root.historyIndex]
        terminalInput.cursorPosition = terminalInput.length
    }

    function showNextCommand() {
        if (root.commandHistory.length === 0)
            return
        root.historyIndex = Math.min(root.commandHistory.length, root.historyIndex + 1)
        terminalInput.text = root.historyIndex < root.commandHistory.length
                           ? root.commandHistory[root.historyIndex] : ""
        terminalInput.cursorPosition = terminalInput.length
    }

    Component.onCompleted: {
        if (root.app.projectLoaded)
            workingDirectory.text = root.app.projectRoot
    }

    Connections {
        target: root.app
        function onStateChanged() {
            if (root.app.projectLoaded && workingDirectory.text.trim().length === 0)
                workingDirectory.text = root.app.projectRoot
        }
    }

    Connections {
        target: root.terminal
        function onDisplayTranscriptChanged() {
            Qt.callLater(function() {
                terminalOutput.cursorPosition = terminalOutput.length
                outputScroll.ScrollBar.vertical.position = 1.0
            })
        }
        function onProcessExited() {
            terminalInput.clear()
        }
    }

    Timer {
        id: resizeTimer
        interval: 180
        repeat: false
        onTriggered: {
            if (root.terminal.running)
                root.terminal.resize(root.terminalColumns, root.terminalRows)
        }
    }

    onTerminalColumnsChanged: resizeTimer.restart()
    onTerminalRowsChanged: resizeTimer.restart()

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        GridLayout {
            Layout.fillWidth: true
            columns: width >= 760 ? 2 : 1
            columnSpacing: 16
            rowSpacing: 4

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Label {
                    Layout.fillWidth: true
                    text: root.tr("Embedded terminal", "內嵌終端機")
                    font.pixelSize: 26
                    font.weight: Font.Bold
                    wrapMode: Text.Wrap
                }
                Label {
                    Layout.fillWidth: true
                    text: root.tr("An interactive Windows shell hosted by ConPTY. Output stays in the app; no external console window is created.",
                                  "由 ConPTY 承載嘅互動式 Windows shell。輸出會留喺應用程式內，唔會建立外部主控台視窗。")
                    color: root.secondaryText
                    wrapMode: Text.Wrap
                }
            }

            Pane {
                Layout.fillWidth: true
                padding: 10
                background: Rectangle {
                    radius: 12
                    color: root.cardColor
                    border.color: root.outlineColor
                }
                contentItem: ColumnLayout {
                    spacing: 3
                    Label {
                        Layout.fillWidth: true
                        text: (root.terminal.running ? "● " : "○ ") + root.statusText()
                        color: root.terminal.running ? root.successText
                              : root.terminal.errorString.length > 0 ? root.errorText
                              : root.secondaryText
                        font.weight: Font.DemiBold
                        wrapMode: Text.WrapAnywhere
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: root.terminal.transcriptTruncated || root.terminal.droppedOutputBytes > 0
                        text: root.tr("Bounded output protection is active. %1 byte(s) were discarded under backpressure.",
                                      "有界輸出保護已啟用。因背壓已捨棄 %1 bytes。")
                              .arg(root.terminal.droppedOutputBytes)
                        color: root.errorText
                        wrapMode: Text.Wrap
                    }
                }
            }
        }

        Pane {
            Layout.fillWidth: true
            padding: 10
            background: Rectangle {
                radius: 12
                color: root.cardColor
                border.color: root.outlineColor
            }
            contentItem: GridLayout {
                columns: width >= 920 ? 6 : width >= 620 ? 3 : 1
                columnSpacing: 8
                rowSpacing: 8

                ComboBox {
                    id: shellChoice
                    Layout.fillWidth: true
                    enabled: !root.terminal.running
                    textRole: "label"
                    valueRole: "value"
                    model: [
                        { label: root.tr("Default (PowerShell)", "預設（PowerShell）"), value: "default" },
                        { label: root.tr("Windows PowerShell", "Windows PowerShell"), value: "powershell" },
                        { label: root.tr("Command Prompt", "命令提示字元"), value: "cmd" }
                    ]
                    Accessible.name: root.tr("Embedded terminal shell", "內嵌終端機 shell")
                }
                TextField {
                    id: workingDirectory
                    Layout.fillWidth: true
                    Layout.columnSpan: width >= 920 ? 2 : 1
                    enabled: !root.terminal.running
                    placeholderText: root.tr("Working directory (project when empty)", "工作目錄（留空時使用專案）")
                    Accessible.name: root.tr("Terminal working directory", "終端機工作目錄")
                    selectByMouse: true
                }
                Button {
                    Layout.fillWidth: true
                    text: root.tr("Start", "啟動")
                    highlighted: true
                    enabled: !root.terminal.running
                    Accessible.name: root.tr("Start the embedded terminal", "啟動內嵌終端機")
                    onClicked: root.startSession()
                }
                Button {
                    Layout.fillWidth: true
                    text: root.tr("Stop", "停止")
                    enabled: root.terminal.running
                    Accessible.name: root.tr("Gracefully stop the embedded terminal", "正常停止內嵌終端機")
                    onClicked: root.terminal.stopGracefully(3000)
                }
                Button {
                    Layout.fillWidth: true
                    text: root.tr("Force stop", "強制停止")
                    enabled: root.terminal.running
                    Accessible.name: root.tr("Force stop the embedded terminal process tree", "強制停止內嵌終端機程序樹")
                    onClicked: forceStopDialog.open()
                }
            }
        }

        Pane {
            Layout.fillWidth: true
            Layout.fillHeight: true
            padding: 0
            background: Rectangle {
                radius: 12
                color: "#0D1117"
                border.color: root.terminal.running ? "#6750A4" : "#30363D"
            }
            contentItem: ColumnLayout {
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: 8
                    Label {
                        Layout.fillWidth: true
                        text: root.tr("Administrator shell · %1 columns × %2 rows",
                                      "系統管理員 shell · %1 欄 × %2 行")
                              .arg(root.terminalColumns).arg(root.terminalRows)
                        color: "#8B949E"
                        font.family: "Cascadia Mono"
                        font.pixelSize: 10
                    }
                    ToolButton {
                        text: root.tr("Copy", "複製")
                        enabled: terminalOutput.length > 0
                        Accessible.name: root.tr("Copy terminal output", "複製終端機輸出")
                        onClicked: {
                            terminalOutput.selectAll()
                            terminalOutput.copy()
                            terminalOutput.deselect()
                        }
                    }
                    ToolButton {
                        text: root.tr("Clear", "清除")
                        enabled: terminalOutput.length > 0
                        Accessible.name: root.tr("Clear retained terminal output", "清除保留嘅終端機輸出")
                        onClicked: root.terminal.clearTranscript()
                    }
                }

                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#30363D" }

                ScrollView {
                    id: outputScroll
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    TextArea {
                        id: terminalOutput
                        width: outputScroll.availableWidth
                        text: root.terminal.displayTranscript
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.WrapAnywhere
                        color: "#E6EDF3"
                        selectionColor: "#6750A4"
                        selectedTextColor: "#FFFFFF"
                        font.family: "Cascadia Mono"
                        font.pixelSize: 12
                        background: null
                        leftPadding: 12
                        rightPadding: 12
                        topPadding: 10
                        bottomPadding: 10
                        Accessible.name: root.tr("Embedded terminal output", "內嵌終端機輸出")
                    }
                }

                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#30363D" }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.margins: 8
                    spacing: 6

                    Label {
                        text: ">"
                        color: root.terminal.running ? "#FFB4AB" : "#8B949E"
                        font.family: "Cascadia Mono"
                        font.bold: true
                    }
                    TextField {
                        id: terminalInput
                        Layout.fillWidth: true
                        enabled: root.terminal.running
                        placeholderText: root.terminal.running
                                         ? root.tr("Type a command and press Enter", "輸入命令再按 Enter")
                                         : root.tr("Start a session to enter commands", "啟動工作階段後先可以輸入命令")
                        Accessible.name: root.tr("Embedded terminal command input", "內嵌終端機命令輸入")
                        font.family: "Cascadia Mono"
                        selectByMouse: true
                        onAccepted: root.sendInput()
                        Keys.onUpPressed: function(event) {
                            root.showPreviousCommand()
                            event.accepted = true
                        }
                        Keys.onDownPressed: function(event) {
                            root.showNextCommand()
                            event.accepted = true
                        }
                    }
                    ToolButton {
                        text: "Ctrl+C"
                        enabled: root.terminal.running
                        Accessible.name: root.tr("Send interrupt to terminal", "傳送中斷到終端機")
                        onClicked: root.terminal.writeInput("\u0003")
                    }
                    Button {
                        text: root.tr("Send", "傳送")
                        enabled: root.terminal.running && terminalInput.text.length > 0
                        Accessible.name: root.tr("Send command to embedded terminal", "傳送命令到內嵌終端機")
                        onClicked: root.sendInput()
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: "⚠ " + root.tr("WimForge is elevated by default, so terminal commands also run as administrator. Review every command before sending it. The backend uses the documented Windows ConPTY API—the same pseudoconsole infrastructure used by open-source Windows Terminal; it does not vendor or embed Windows Terminal source.",
                                      "WimForge 預設會提升權限，所以終端機命令亦會以系統管理員身份運行。傳送前請審閱每個命令。後端使用 Windows 文件記載嘅 ConPTY API，即開源 Windows Terminal 所用嘅同一偽主控台基礎設施；並冇包含或嵌入 Windows Terminal 原始碼。")
            color: root.secondaryText
            font.pixelSize: 10
            wrapMode: Text.Wrap
        }
    }

    Dialog {
        id: forceStopDialog
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(520, root.width - 40)
        modal: true
        focus: true
        title: root.tr("Force-stop terminal process tree?", "強制停止終端機程序樹？")
        standardButtons: Dialog.Cancel | Dialog.Ok

        Label {
            width: parent.width
            text: root.tr("This immediately terminates the shell and every process it started. Unsaved command work can be lost.",
                          "呢個操作會即時終止 shell 同由佢啟動嘅所有程序。未儲存嘅命令工作可能會遺失。")
            wrapMode: Text.Wrap
        }
        onAccepted: root.terminal.forceStop()
    }
}
