pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window
import "components"
import "pages"

ApplicationWindow {
    id: root
    width: 1480
    height: 920
    minimumWidth: 1080
    minimumHeight: 700
    visible: true
    onClosing: Qt.quit()
    title: app.projectLoaded ? "WimForge — " + app.projectName : "WimForge"
    color: Material.theme === Material.Dark ? "#141218" : "#F7F2FA"

    Material.theme: app.themeMode === 1 ? Material.Light
                  : app.themeMode === 2 ? Material.Dark
                  : (Application.styleHints.colorScheme === Qt.ColorScheme.Dark ? Material.Dark : Material.Light)
    Material.accent: "#6750A4"
    Material.primary: "#6750A4"

    property int currentPage: startupPage
    property bool notificationsOpen: false
    readonly property var controller: app
    readonly property bool compactNavigation: width < 1280
    readonly property bool darkTheme: Material.theme === Material.Dark
    readonly property color secondaryTextColor: darkTheme ? "#CAC4D0" : "#625B71"
    readonly property color errorColor: darkTheme ? "#FFB4AB" : "#BA1A1A"
    readonly property color errorBadgeColor: darkTheme ? "#8C1D18" : "#BA1A1A"
    readonly property color successColor: darkTheme ? "#8BD7A6" : "#386A20"
    readonly property color warningColor: darkTheme ? "#FFB95C" : "#744B00"
    readonly property color warningContainerColor: darkTheme ? "#4A2800" : "#FFF3E0"
    property var navigationItems: [
        { icon: "⌂", en: "Overview", zh: "總覽", context: "project" },
        { icon: "◫", en: "Source & editions", zh: "來源同版本", context: "source" },
        { icon: "⚙", en: "Customize", zh: "調校", context: "config" },
        { icon: "▤", en: "Group Policy Studio", zh: "群組原則工房", context: "gpo" },
        { icon: "✦", en: "Unattended Studio", zh: "無人值守工房", context: "unattended" },
        { icon: "▦", en: "Package Studio", zh: "套件工房", context: "packages" },
        { icon: "⌁", en: "WinForge Bridge", zh: "WinForge 橋接", context: "winforge" },
        { icon: "▣", en: "Virtual Machine Lab", zh: "虛擬機實驗室", context: "vm-lab" },
        { icon: "▶", en: "Review & run", zh: "檢查同開工", context: "plan" },
        { icon: "↶", en: "History & recovery", zh: "歷史同復原", context: "history" },
        { icon: "☷", en: "Settings", zh: "設定", context: "settings" },
        { icon: ">_", en: "Embedded terminal", zh: "內嵌終端機", context: "terminal" }
    ]

    function openContextHistory(x, y) {
        var item = navigationItems[Math.max(0, Math.min(currentPage, navigationItems.length - 1))]
        var recent = app.contextualHistory(item.context, "")
        if (!recent || recent.length === 0)
            recent = app.contextualHistory("", "")
        contextHistory.contextKey = item.context
        contextHistory.elementId = ""
        contextHistory.openAt(x, y, tr2(item.en, item.zh), recent)
    }

    function tr2(en, zh) {
        var dependency = app.languageMode
        if (dependency === 1) return zh
        if (dependency === 2) return en + "  ·  " + zh
        return en
    }

    // AbstractButton interprets a single ampersand as a mnemonic marker.
    function buttonText(value) {
        return String(value).replace(/&/g, "&&")
    }

    function isTextEditor(item) {
        var candidate = item
        while (candidate) {
            if (candidate instanceof TextInput || candidate instanceof TextEdit)
                return true
            candidate = candidate.parent
        }
        return false
    }

    Connections {
        target: app
        function onSnackbarRequested(message, tone) { snackbar.show(message, tone, "") }
        function onNewProjectRequested() { newProjectSheet.open() }
        function onOpenProjectRequested() { openProjectSheet.open() }
        function onRunConfirmationRequested(summary, destructiveCount) {
            runSummary.text = summary
            sourceBackupAck.checked = false
            adminAck.checked = false
            destructiveBadge.visible = destructiveCount > 0
            destructiveBadge.text = root.tr2("⚠ %1 destructive", "⚠ %1 個破壞性步驟").arg(destructiveCount)
                                         + (app.languageMode === 1 ? " 項有破壞性" : "")
            runSheet.open()
        }
        function onExportProjectRequested() { exportProjectSheet.open() }
        function onExportScriptRequested() { exportScriptSheet.open() }
        function onUnattendedStudioRequested() { root.currentPage = 4 }
        function onRecoveryReviewRequested() { root.currentPage = 8 }
        function onSearchRequested(query) { searchPalette.openForQuery(query) }
        function onSearchNavigationRequested(page, focusId, query) {
            root.currentPage = Math.max(0, Math.min(page, root.navigationItems.length - 1))
            globalSearch.clear()
            searchPalette.close()
        }
    }

    Shortcut { sequence: "Ctrl+N"; onActivated: newProjectSheet.open() }
    Shortcut { sequence: "Ctrl+O"; onActivated: openProjectSheet.open() }
    Shortcut {
        sequence: "Ctrl+Z"
        context: Qt.ApplicationShortcut
        enabled: !root.isTextEditor(root.activeFocusItem)
        onActivated: app.undoContext(root.navigationItems[root.currentPage].context, "")
    }
    Shortcut {
        sequence: "Ctrl+Shift+Z"
        context: Qt.ApplicationShortcut
        enabled: !root.isTextEditor(root.activeFocusItem)
        onActivated: root.openContextHistory(root.width / 2 - 210, 90)
    }
    Shortcut { sequence: "Ctrl+Enter"; onActivated: app.requestRunPlan() }

    MouseArea {
        parent: root.contentItem
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        hoverEnabled: false
        onClicked: function(mouse) {
            root.openContextHistory(mouse.x, mouse.y)
            mouse.accepted = true
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Pane {
            id: navigation
            Layout.fillHeight: true
            Layout.preferredWidth: root.compactNavigation ? 76 : 320
            Layout.minimumWidth: Layout.preferredWidth
            padding: root.compactNavigation ? 8 : 12
            background: Rectangle {
                color: Material.theme === Material.Dark ? "#1D1B20" : "#F3EDF7"
                border.color: Material.theme === Material.Dark ? "#343139" : "#E7E0EC"
                border.width: 0
                Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: parent.border.color }
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 52

                    Image {
                        id: appLogo
                        anchors.left: root.compactNavigation ? undefined : parent.left
                        anchors.leftMargin: root.compactNavigation ? 0 : 8
                        anchors.horizontalCenter: root.compactNavigation ? parent.horizontalCenter : undefined
                        anchors.verticalCenter: parent.verticalCenter
                        source: "qrc:/qt/qml/WimForge/assets/app-icon.svg"
                        sourceSize.width: 42
                        sourceSize.height: 42
                        width: 42
                        height: 42
                        Accessible.name: root.tr2("WimForge application", "WimForge 應用程式")
                    }
                    Column {
                        visible: !root.compactNavigation
                        anchors.left: appLogo.right
                        anchors.leftMargin: 10
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 0
                        Label { width: parent.width; text: "WimForge"; font.pixelSize: 21; font.weight: Font.Bold; elide: Text.ElideRight }
                        Label { width: parent.width; text: root.tr2("Windows Image Studio", "Windows 映像工房"); font.pixelSize: 10; color: root.secondaryTextColor; elide: Text.ElideRight }
                    }
                }

                Button {
                    Layout.fillWidth: true
                    Layout.topMargin: root.compactNavigation ? 4 : 12
                    icon.name: "document-new"
                    text: root.compactNavigation ? "+" : root.buttonText(root.tr2("New project", "開新工程"))
                    highlighted: true
                    Accessible.name: root.tr2("New project", "開新工程")
                    onClicked: newProjectSheet.open()
                    ToolTip.visible: root.compactNavigation && hovered
                    ToolTip.text: Accessible.name
                }

                ScrollView {
                    id: navigationScroll
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    Column {
                        width: navigationScroll.availableWidth
                        spacing: 4

                        Repeater {
                            model: root.navigationItems
                            delegate: ItemDelegate {
                                id: navigationDelegate
                                required property var modelData
                                required property int index
                                width: parent.width
                                implicitHeight: 48
                                text: root.compactNavigation
                                      ? modelData.icon
                                      : modelData.icon + "   " + root.buttonText(root.tr2(modelData.en, modelData.zh))
                                highlighted: root.currentPage === index
                                font.weight: highlighted ? Font.DemiBold : Font.Normal
                                Accessible.name: (highlighted ? root.tr2("Current page: ", "目前頁面：") : "")
                                                 + root.tr2(modelData.en, modelData.zh)
                                onClicked: root.currentPage = index
                                ToolTip.visible: root.compactNavigation && hovered
                                ToolTip.text: root.tr2(modelData.en, modelData.zh)
                                background: Rectangle {
                                    radius: 22
                                    color: navigationDelegate.highlighted
                                           ? (Material.theme === Material.Dark ? "#4A4458" : "#E8DEF8")
                                           : navigationDelegate.hovered ? (Material.theme === Material.Dark ? "#2B292F" : "#EDE7F1") : "transparent"
                                }
                            }
                        }
                    }
                }

                Pane {
                    Layout.fillWidth: true
                    Layout.preferredHeight: projectSummary.implicitHeight + topPadding + bottomPadding
                    visible: app.projectLoaded
                    padding: root.compactNavigation ? 8 : 12
                    Accessible.name: root.tr2("Project status: ", "工程狀態：") + app.projectName + ". " + app.gitStatusText
                    background: Rectangle { radius: 16; color: Material.theme === Material.Dark ? "#211F26" : "#FFFBFE"; border.color: Material.theme === Material.Dark ? "#49454F" : "#E7E0EC" }
                    ColumnLayout {
                        id: projectSummary
                        width: parent.width
                        Label { visible: root.compactNavigation; text: app.busy ? "◉" : "▣"; font.pixelSize: 20; Layout.alignment: Qt.AlignHCenter; Accessible.ignored: true }
                        Label { visible: !root.compactNavigation; text: "▣  " + app.projectName; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight }
                        Label { visible: !root.compactNavigation; text: app.gitStatusText; font.pixelSize: 11; color: root.successColor; Layout.fillWidth: true; elide: Text.ElideRight }
                        ProgressBar { visible: app.busy; Layout.fillWidth: true; value: app.progress; indeterminate: app.progress <= 0; Accessible.name: root.tr2("Project job progress", "工程工作進度") }
                    }
                    HoverHandler { id: projectSummaryHover }
                    ToolTip.visible: root.compactNavigation && projectSummaryHover.hovered
                    ToolTip.text: app.projectName + " · " + app.gitStatusText
                }

                Label {
                    Layout.fillWidth: true
                    text: root.compactNavigation ? "v" + app.version : "v" + app.version + "  ·  MIT"
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: 10
                    color: Material.theme === Material.Dark ? "#938F99" : "#79747E"
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Pane {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 68
                    leftPadding: 24; rightPadding: 18; topPadding: 10; bottomPadding: 10
                    background: Rectangle {
                        color: Material.theme === Material.Dark ? "#1D1B20" : "#FFFBFE"
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Material.theme === Material.Dark ? "#343139" : "#E7E0EC" }
                    }
                    RowLayout {
                        anchors.fill: parent
                        TextField {
                            id: globalSearch
                            Layout.preferredWidth: Math.min(440, parent.width * 0.42)
                            placeholderText: root.tr2("Search features, commands and settings", "搜尋功能、指令同設定")
                            Accessible.name: placeholderText
                            leftPadding: 42
                            Label { anchors.left: parent.left; anchors.leftMargin: 15; anchors.verticalCenter: parent.verticalCenter; text: "⌕"; font.pixelSize: 20; color: Material.accent; Accessible.ignored: true }
                            onAccepted: app.search(text)
                        }
                        Item { Layout.fillWidth: true }
                        ToolButton {
                            text: app.busy ? "◉" : "○"
                            Accessible.name: app.busy ? root.tr2("Jobs are running; open job queue", "有工序行緊；開啟工序隊列") : root.tr2("Open job queue", "開啟工序隊列")
                            onClicked: root.currentPage = 8
                            ToolTip.visible: hovered
                            ToolTip.text: Accessible.name
                        }
                        ToolButton {
                            id: bell
                            text: "🔔"
                            Accessible.name: app.notificationUnreadCount > 0
                                             ? root.tr2("Notification center, %1 unread", "通知中心，%1 個未讀").arg(app.notificationUnreadCount)
                                             : root.tr2("Notification center, no unread notifications", "通知中心，冇未讀通知")
                            onClicked: root.notificationsOpen = !root.notificationsOpen
                            ToolTip.visible: hovered
                            ToolTip.text: Accessible.name
                            Rectangle {
                                visible: app.notificationUnreadCount > 0
                                anchors.right: parent.right; anchors.top: parent.top
                                anchors.rightMargin: 2; anchors.topMargin: 1
                                width: Math.max(18, unread.implicitWidth + 8); height: 18; radius: 9
                                color: root.errorBadgeColor
                                Label { id: unread; anchors.centerIn: parent; text: app.notificationUnreadCount; color: "white"; font.pixelSize: 10; font.bold: true; Accessible.ignored: true }
                            }
                        }
                        MenuSeparator { implicitHeight: 28 }
                        Button {
                            flat: true
                            icon.name: "document-open"
                            text: app.projectLoaded ? app.projectName : root.tr2("Open project", "開工程")
                            onClicked: openProjectSheet.open()
                        }
                    }
                }

                Pane {
                    Layout.fillWidth: true
                    Layout.preferredHeight: recoveryRow.implicitHeight + topPadding + bottomPadding
                    visible: app.pendingRecovery
                    padding: 10
                    background: Rectangle { color: root.warningContainerColor }
                    RowLayout {
                        id: recoveryRow
                        width: parent.width
                        Label { text: "🛟"; font.pixelSize: 22 }
                        Label {
                            Layout.fillWidth: true
                            text: root.tr2("Interrupted work found. The image is protected; choose how to recover when ready.",
                                           "搵到上次中斷嘅工序。映像仲安全；得閒先揀點樣復原，唔使即刻畀個彈窗追住。")
                            wrapMode: Text.Wrap
                        }
                        Button { text: root.tr2("Review recovery", "檢查復原"); onClicked: recoverySheet.open() }
                    }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: 22
                    currentIndex: root.currentPage

                    DashboardPage { app: root.controller; tr: root.tr2; openPage: index => root.currentPage = index }
                    SourcePage { app: root.controller; tr: root.tr2 }
                    CustomizePage { app: root.controller; tr: root.tr2 }
                    GpoStudioPage { app: root.controller; tr: root.tr2 }
                    UnattendedStudioPage { app: root.controller; tr: root.tr2 }
                    PackageStudioPage { app: root.controller; tr: root.tr2 }
                    WinForgeBridgePage { app: root.controller; tr: root.tr2 }
                    VmLabPage { app: root.controller; tr: root.tr2 }
                    PlanPage { app: root.controller; tr: root.tr2 }
                    HistoryPage { app: root.controller; tr: root.tr2 }
                    SettingsPage { app: root.controller; tr: root.tr2 }
                    TerminalPage {
                        app: root.controller
                        terminal: terminalSession
                        tr: root.tr2
                    }
                }
            }

            NotificationCenter {
                id: notificationCenter
                anchors.top: parent.top
                opened: root.notificationsOpen
                entries: app.notifications
                unreadCount: app.notificationUnreadCount
                motionEnabled: app.motionEnabled
                onCloseRequested: root.notificationsOpen = false
                onMarkReadRequested: id => app.markNotificationRead(id)
                onMarkUnreadRequested: id => app.markNotificationUnread(id)
                onDismissRequested: id => app.dismissNotification(id)
                onDeleteRequested: id => app.deleteNotification(id)
                onRestoreRequested: id => app.restoreNotification(id)
                onUndoRequested: app.undoLatestNotificationChange()
            }

            Snackbar {
                id: snackbar
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 22
                motionEnabled: app.motionEnabled
            }

            SearchPalette {
                id: searchPalette
                app: root.controller
                tr: root.tr2
            }

            ContextHistoryPanel {
                id: contextHistory
                parent: Overlay.overlay
                branchName: "main"
                motionEnabled: app.motionEnabled
                onUndoRequested: function(eventId) {
                    if (app.undoHistoryEvent(eventId)) events = app.contextualHistory(contextKey, elementId)
                }
                onRedoRequested: function(eventId) {
                    if (app.redoHistoryEvent(eventId)) events = app.contextualHistory(contextKey, elementId)
                }
                onRestoreRequested: function(eventId) {
                    if (app.restoreHistoryEvent(eventId)) events = app.contextualHistory(contextKey, elementId)
                }
                onBookmarkRequested: function(eventId, name) {
                    if (app.bookmarkHistoryEvent(eventId, name)) events = app.contextualHistory(contextKey, elementId)
                }
                onBranchRequested: function(eventId, name) {
                    if (app.branchHistoryEvent(eventId, name)) events = app.contextualHistory(contextKey, elementId)
                }
            }
        }
    }

    Popup {
        id: newProjectSheet
        readonly property string heading: root.tr2("Create a Git-backed project", "開個有 Git 保護嘅工程")
        anchors.centerIn: Overlay.overlay
        width: Math.min(600, root.width - 32)
        height: Math.min(newProjectContent.implicitHeight + topPadding + bottomPadding, root.height - 32)
        modal: true
        dim: true
        focus: true
        closePolicy: Popup.CloseOnEscape
        padding: 24
        onOpened: projectName.forceActiveFocus()
        background: Rectangle { radius: 24; color: Material.theme === Material.Dark ? "#211F26" : "#FFFBFE"; border.color: Material.accent; border.width: 1 }

        contentItem: ScrollView {
            id: newProjectScroll
            Accessible.name: newProjectSheet.heading
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                id: newProjectContent
                width: newProjectSheet.availableWidth
                spacing: 12
                Label { Layout.fillWidth: true; text: newProjectSheet.heading; font.pixelSize: 22; font.weight: Font.Bold; wrapMode: Text.Wrap }
                Label { Layout.fillWidth: true; text: root.tr2("The folder becomes its own local repository. Every edit is committed automatically.", "呢個資料夾會變成獨立本機 Git 倉，每次改動都自動 commit。唔怕手快快。"); wrapMode: Text.Wrap }
                TextField { id: projectName; Layout.fillWidth: true; Accessible.name: placeholderText; placeholderText: root.tr2("Project name", "工程名"); text: "Windows 11 Custom" }
                TextField { id: projectRoot; Layout.fillWidth: true; Accessible.name: placeholderText; placeholderText: root.tr2("Project folder", "工程資料夾"); text: app.defaultProjectPath }
                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    Button { text: root.tr2("Cancel", "取消"); flat: true; onClicked: newProjectSheet.close() }
                    Button {
                        text: root.tr2("Create project", "建立工程")
                        highlighted: true
                        enabled: projectName.text.trim().length > 0 && projectRoot.text.trim().length > 0
                        onClicked: {
                            if (app.createProject(projectRoot.text.trim(), projectName.text.trim())) {
                                newProjectSheet.close(); root.currentPage = 1
                            }
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: openProjectSheet
        readonly property string heading: root.tr2("Open or import project", "開啟或者匯入工程")
        anchors.centerIn: Overlay.overlay
        width: Math.min(600, root.width - 32)
        height: Math.min(openProjectContent.implicitHeight + topPadding + bottomPadding, root.height - 32)
        modal: true; dim: true; focus: true
        closePolicy: Popup.CloseOnEscape
        padding: 24
        onOpened: openPath.forceActiveFocus()
        background: Rectangle { radius: 24; color: Material.theme === Material.Dark ? "#211F26" : "#FFFBFE"; border.color: Material.accent; border.width: 1 }

        contentItem: ScrollView {
            id: openProjectScroll
            Accessible.name: openProjectSheet.heading
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                id: openProjectContent
                width: openProjectSheet.availableWidth
                spacing: 12
                Label { Layout.fillWidth: true; text: openProjectSheet.heading; font.pixelSize: 22; font.weight: Font.Bold; wrapMode: Text.Wrap }
                Label { Layout.fillWidth: true; text: root.tr2("Paste a project folder, complete .wimforge save bundle, or legacy .json config path. No blocking file dialog required.", "貼工程資料夾、完整 .wimforge 儲存 bundle，或者舊式 .json 設定路徑；唔使畀檔案對話框阻住。"); wrapMode: Text.Wrap }
                TextField { id: openPath; Layout.fillWidth: true; Accessible.name: placeholderText; placeholderText: root.tr2("Project folder or config file", "工程資料夾或者設定檔") }
                TextField { id: importDestination; Layout.fillWidth: true; Accessible.name: placeholderText; visible: openPath.text.toLowerCase().endsWith(".json") || openPath.text.toLowerCase().endsWith(".wimforge"); placeholderText: root.tr2("Destination folder for imported project", "匯入工程目的資料夾"); text: app.defaultProjectPath }
                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    Button { text: root.tr2("Cancel", "取消"); flat: true; onClicked: openProjectSheet.close() }
                    Button {
                        text: root.tr2("Open", "開啟")
                        highlighted: true
                        enabled: openPath.text.trim().length > 0
                        onClicked: {
                            var ok = openPath.text.toLowerCase().endsWith(".json") || openPath.text.toLowerCase().endsWith(".wimforge")
                                   ? app.importProject(openPath.text.trim(), importDestination.text.trim())
                                   : app.openProject(openPath.text.trim())
                            if (ok) { openProjectSheet.close(); root.currentPage = 0 }
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: runSheet
        readonly property string heading: root.tr2("Run the reviewed plan?", "執行已檢查計劃？")
        anchors.centerIn: Overlay.overlay
        width: Math.min(660, root.width - 32)
        height: Math.min(runContent.implicitHeight + topPadding + bottomPadding, root.height - 32)
        modal: true; dim: true; focus: true
        closePolicy: Popup.CloseOnEscape
        padding: 24
        background: Rectangle { radius: 24; color: Material.theme === Material.Dark ? "#211F26" : "#FFFBFE"; border.color: destructiveBadge.visible ? root.errorColor : Material.accent; border.width: 2 }

        contentItem: ScrollView {
            id: runScroll
            Accessible.name: runSheet.heading
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                id: runContent
                width: runSheet.availableWidth
                spacing: 10
                Label { Layout.fillWidth: true; text: runSheet.heading; font.pixelSize: 22; font.weight: Font.Bold; wrapMode: Text.Wrap }
                Label { id: runSummary; Layout.fillWidth: true; wrapMode: Text.Wrap }
                Label { id: destructiveBadge; Layout.fillWidth: true; visible: false; color: root.errorColor; font.weight: Font.Bold; wrapMode: Text.Wrap }
                CheckBox { id: sourceBackupAck; Layout.fillWidth: true; text: root.tr2("I verified the source and output paths", "我睇清楚來源同輸出路徑") }
                CheckBox { id: adminAck; Layout.fillWidth: true; text: root.tr2("I understand servicing needs Administrator rights", "我明白映像維護要管理員權限") }
                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    Button { text: root.tr2("Keep reviewing", "再睇吓先"); flat: true; onClicked: runSheet.close() }
                    Button {
                        text: root.tr2("Start jobs", "開始工序")
                        highlighted: true
                        enabled: sourceBackupAck.checked && adminAck.checked
                        onClicked: { runSheet.close(); app.runPlan() }
                    }
                }
            }
        }
    }

    component PathExportSheet: Popup {
        id: exportSheet
        property string heading
        property string placeholder
        property var acceptAction: function(path) { return false }
        anchors.centerIn: Overlay.overlay
        width: Math.min(600, root.width - 32)
        height: Math.min(exportContent.implicitHeight + topPadding + bottomPadding, root.height - 32)
        modal: true; dim: true; focus: true
        closePolicy: Popup.CloseOnEscape
        padding: 24
        onOpened: exportPathField.forceActiveFocus()
        background: Rectangle { radius: 24; color: Material.theme === Material.Dark ? "#211F26" : "#FFFBFE"; border.color: Material.accent; border.width: 1 }

        contentItem: ScrollView {
            id: exportScroll
            Accessible.name: exportSheet.heading
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                id: exportContent
                width: exportSheet.availableWidth
                spacing: 12
                Label { Layout.fillWidth: true; text: exportSheet.heading; font.pixelSize: 22; font.weight: Font.Bold; wrapMode: Text.Wrap }
                TextField { id: exportPathField; Layout.fillWidth: true; Accessible.name: exportSheet.placeholder; placeholderText: exportSheet.placeholder }
                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    Button { text: root.tr2("Cancel", "取消"); flat: true; onClicked: exportSheet.close() }
                    Button { text: root.tr2("Export", "匯出"); highlighted: true; enabled: exportPathField.text.trim().length > 0; onClicked: { if (exportSheet.acceptAction(exportPathField.text.trim())) exportSheet.close() } }
                }
            }
        }
    }

    PathExportSheet {
        id: exportProjectSheet
        heading: root.tr2("Export complete project + Git histories", "匯出完整工程同所有 Git 歷史")
        placeholder: "D:\\profiles\\windows-build.wimforge"
        acceptAction: path => app.exportProject(path)
    }
    PathExportSheet {
        id: exportScriptSheet
        heading: root.tr2("Export PowerShell build script", "匯出 PowerShell 建置 script")
        placeholder: "D:\\profiles\\build-image.ps1"
        acceptAction: path => app.exportScript(path)
    }

    Popup {
        id: recoverySheet
        readonly property string heading: root.tr2("Recovery workspace", "復原工作區")
        anchors.centerIn: Overlay.overlay
        width: Math.min(720, root.width - 32)
        height: Math.min(recoveryContent.implicitHeight + topPadding + bottomPadding, root.height - 32)
        modal: true; dim: true; focus: true
        closePolicy: Popup.CloseOnEscape
        padding: 24
        background: Rectangle { radius: 24; color: Material.theme === Material.Dark ? "#211F26" : "#FFFBFE"; border.color: root.warningColor; border.width: 2 }

        contentItem: ScrollView {
            id: recoveryScroll
            Accessible.name: recoverySheet.heading
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                id: recoveryContent
                width: recoverySheet.availableWidth
                spacing: 12
                Label { Layout.fillWidth: true; text: "🛟  " + recoverySheet.heading; font.pixelSize: 22; font.weight: Font.Bold; wrapMode: Text.Wrap }
                Label { Layout.fillWidth: true; text: app.recoverySummary; wrapMode: Text.Wrap }
                GridLayout {
                    Layout.fillWidth: true
                    columns: width >= 620 && app.languageMode !== 2 ? 4 : 1
                    rowSpacing: 6
                    columnSpacing: 6
                    Button { Layout.fillWidth: true; text: "▶  " + root.buttonText(root.tr2("Rebuild & review plan", "重排同檢查計劃")); onClicked: { recoverySheet.close(); app.resumeRecovery() } }
                    Button { Layout.fillWidth: true; text: "↶  " + root.tr2("Undo latest config", "Undo 最新設定"); onClicked: { recoverySheet.close(); app.rollbackRecovery() } }
                    Button { Layout.fillWidth: true; text: "⏏  " + root.tr2("Safe unmount", "安全卸載"); onClicked: { recoverySheet.close(); app.safeUnmountRecovery() } }
                    Button { Layout.fillWidth: true; text: root.tr2("Later", "遲啲先"); flat: true; onClicked: recoverySheet.close() }
                }
            }
        }
    }
}
