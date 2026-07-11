pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    required property var app
    required property var tr
    property bool active: false

    property var selectedPolicy: null
    property var elementValues: ({})
    readonly property bool compact: width < 820
    property int draftState: 1
    property bool documentationExpanded: false

    readonly property bool hasSelection: selectedPolicy !== null && selectedPolicy !== undefined
    readonly property color surfaceColor: Material.theme === Material.Dark ? "#211F26" : "#FFFBFE"
    readonly property color surfaceVariantColor: Material.theme === Material.Dark ? "#2B292F" : "#F7F2FA"
    readonly property color outlineColor: Material.theme === Material.Dark ? "#49454F" : "#E7E0EC"
    readonly property color secondaryTextColor: Material.theme === Material.Dark ? "#CAC4D0" : "#625B71"

    function ensureCatalog() {
        if (!active)
            return
        if (!app.gpoLoaded)
            app.loadGpoCatalog()
        Qt.callLater(root.syncSelection)
    }

    Component.onCompleted: root.ensureCatalog()
    onActiveChanged: root.ensureCatalog()

    Connections {
        target: root.app
        function onStudioChanged() {
            Qt.callLater(root.syncSelection)
        }
    }

    function syncSelection() {
        var results = app.gpoResults
        if (!results || results.length === 0) {
            selectedPolicy = null
            policyList.currentIndex = -1
            return
        }

        if (selectedPolicy) {
            for (var i = 0; i < results.length; ++i) {
                if (results[i].id === selectedPolicy.id) {
                    selectedPolicy = results[i]
                    policyList.currentIndex = i
                    return
                }
            }
        }

        // A policy with schema fields makes the first visit immediately useful,
        // while still falling back to the first installed policy.
        var initialIndex = 0
        for (var candidate = 0; candidate < results.length; ++candidate) {
            if (results[candidate].elements && results[candidate].elements.length > 0) {
                initialIndex = candidate
                break
            }
        }
        policyList.currentIndex = initialIndex
        policyList.positionViewAtIndex(initialIndex, ListView.Contain)
        selectPolicy(results[initialIndex])
    }

    function selectPolicy(policy) {
        selectedPolicy = policy
        documentationExpanded = false
        draftState = 1
        stateTabs.currentIndex = 1
        if (policyScope)
            policyScope.currentIndex = 0

        var defaults = ({})
        var elements = policy && policy.elements ? policy.elements : []
        for (var i = 0; i < elements.length; ++i) {
            var element = elements[i]
            if (element.control === "Switch") {
                defaults[element.id] = element.defaultChecked
            } else if (element.control === "ComboBox") {
                var selected = element.defaultValue
                if ((!selected || selected.length === 0) && element.options.length > 0)
                    selected = element.options[0].value
                defaults[element.id] = selected
            } else {
                defaults[element.id] = element.defaultValue
            }
        }
        elementValues = defaults
    }

    function policyState(action) {
        var scope = selectedPolicy && (selectedPolicy.policyClass === "User"
                    || (selectedPolicy.policyClass === "Both" && policyScope.currentIndex === 1))
                    ? "user" : "machine"
        return scope + action
    }

    function stateName() {
        if (draftState === 0)
            return root.tr("Not configured", "未設定")
        if (draftState === 2)
            return root.tr("Disabled", "停用")
        return root.tr("Enabled", "啟用")
    }

    function stateAction() {
        if (draftState === 0)
            return "NotConfigured"
        if (draftState === 2)
            return "Disabled"
        return "Enabled"
    }

    function scopeName() {
        if (!selectedPolicy)
            return ""
        if (selectedPolicy.policyClass === "Both")
            return policyScope.currentIndex === 1 ? root.tr("User", "使用者") : root.tr("Computer", "電腦")
        return selectedPolicy.policyClass === "User" ? root.tr("User", "使用者") : root.tr("Computer", "電腦")
    }

    function registryTarget() {
        if (!selectedPolicy)
            return ""
        var userScope = selectedPolicy.policyClass === "User"
                        || (selectedPolicy.policyClass === "Both" && policyScope.currentIndex === 1)
        var hive = userScope ? "HKCU" : "HKLM"
        return hive + (selectedPolicy.registryKey ? "\\" + selectedPolicy.registryKey : "")
    }

    function commitDraft() {
        if (!selectedPolicy)
            return
        var values = draftState === 1 ? elementValues : ({})
        app.applyGpoPolicy(selectedPolicy.id, policyState(stateAction()), values)
    }

    Timer {
        id: searchDebounce
        interval: 350
        repeat: false
        onTriggered: root.app.searchGpo(policySearch.text, regexMode.checked)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        GridLayout {
            Layout.fillWidth: true
            columns: root.width >= 760 ? 3 : 1
            columnSpacing: 10
            rowSpacing: 8
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Label {
                    Layout.fillWidth: true
                    text: root.tr("Group Policy Studio", "群組原則工房")
                    font.pixelSize: 30
                    font.weight: Font.Bold
                    wrapMode: Text.Wrap
                }
                Label {
                    Layout.fillWidth: true
                    text: root.tr("Find an installed ADMX policy, choose its desired state, and commit a reviewable image-build change.",
                                  "搵出已安裝 ADMX 政策、揀目標狀態，再 commit 一項可審核嘅映像建置變更。")
                    wrapMode: Text.Wrap
                    color: root.secondaryTextColor
                }
            }

            Rectangle {
                Layout.fillWidth: root.width < 760
                Layout.preferredWidth: policyCountLabel.implicitWidth + 28
                Layout.preferredHeight: 38
                radius: 19
                color: Material.theme === Material.Dark ? "#4A4458" : "#E8DEF8"
                Label {
                    id: policyCountLabel
                    anchors.centerIn: parent
                    text: root.app.gpoPolicyCount.toLocaleString() + "  " + root.tr("installed", "已安裝")
                    color: Material.theme === Material.Dark ? "#E8DEF8" : "#4F378B"
                    font.weight: Font.DemiBold
                }
            }

            Button {
                Layout.fillWidth: root.width < 760
                text: "⇩  " + root.tr("Export reference", "匯出參考")
                enabled: root.app.gpoLoaded
                onClicked: docsExport.open()
            }
        }

        Pane {
            Layout.fillWidth: true
            padding: 12
            background: Rectangle {
                radius: 18
                color: root.surfaceColor
                border.color: root.outlineColor
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.width >= 760 ? 4 : 1
                    columnSpacing: 8
                    rowSpacing: 8
                    TextField {
                        id: policySearch
                        Layout.fillWidth: true
                        placeholderText: root.tr("Search names, explanations, categories, or registry paths", "搜尋名稱、解說、分類或登錄路徑")
                        leftPadding: 38
                        selectByMouse: true
                        Label {
                            anchors.left: parent.left
                            anchors.leftMargin: 13
                            anchors.verticalCenter: parent.verticalCenter
                            text: "⌕"
                            font.pixelSize: 19
                            color: root.secondaryTextColor
                        }
                        onTextEdited: searchDebounce.restart()
                        onAccepted: root.app.searchGpo(text, regexMode.checked)
                    }

                    Switch {
                        id: regexMode
                        Layout.fillWidth: root.width < 760
                        text: root.tr("Regex", "正規表示式")
                        onToggled: {
                            if (policySearch.text.trim().length > 0)
                                searchDebounce.restart()
                        }
                    }

                    Button {
                        Layout.fillWidth: root.width < 760
                        text: "🧩  " + root.tr("Build regex", "建立 Regex")
                        onClicked: regexWizard.open()
                    }

                    Button {
                        Layout.fillWidth: root.width < 760
                        text: root.tr("Search", "搜尋")
                        highlighted: true
                        onClicked: root.app.searchGpo(policySearch.text, regexMode.checked)
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: root.width >= 660 ? 3 : 1
                    columnSpacing: 8
                    rowSpacing: 8

                    TextField {
                        id: intent
                        Layout.fillWidth: true
                        placeholderText: root.tr("Describe what you want Windows to do; OpenCode will suggest catalog keywords", "描述你想 Windows 做咩；OpenCode 會建議目錄關鍵字")
                        selectByMouse: true
                        onAccepted: {
                            if (text.trim().length > 0 && !root.app.openCodeBusy)
                                root.app.askOpenCodeForGpo(text)
                        }
                    }

                    Button {
                        Layout.fillWidth: root.width < 660
                        text: "✦  " + root.tr("Suggest search", "建議搜尋")
                        enabled: intent.text.trim().length > 0 && !root.app.openCodeBusy
                        onClicked: root.app.askOpenCodeForGpo(intent.text)
                    }

                    BusyIndicator {
                        running: root.app.openCodeBusy
                        visible: running
                        implicitWidth: 28
                        implicitHeight: 28
                        Accessible.name: root.tr("OpenCode policy search in progress", "OpenCode 政策搜尋進行中")
                    }
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: root.compact ? 1 : 2
            columnSpacing: 12
            rowSpacing: 12

            Pane {
                Layout.fillWidth: true
                Layout.preferredWidth: root.compact ? -1 : 360
                Layout.fillHeight: true
                Layout.minimumWidth: 0
                Layout.minimumHeight: 120
                padding: 8
                background: Rectangle {
                    radius: 18
                    color: root.surfaceColor
                    border.color: root.outlineColor
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 8
                        Layout.rightMargin: 8
                        Layout.topMargin: 5
                        Label {
                            text: root.tr("Policy catalog", "政策目錄")
                            font.pixelSize: 17
                            font.weight: Font.DemiBold
                        }
                        Item { Layout.fillWidth: true }
                        Label {
                            text: root.app.gpoResults.length + " " + root.tr("shown", "顯示")
                            color: Material.accent
                            font.weight: Font.DemiBold
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: 8
                        Layout.rightMargin: 8
                        text: root.app.gpoStatus
                        elide: Text.ElideMiddle
                        color: root.secondaryTextColor
                        font.pixelSize: 10
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: root.outlineColor
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        ListView {
                            id: policyList
                            anchors.fill: parent
                            clip: true
                            spacing: 3
                            model: root.app.gpoResults
                            boundsBehavior: Flickable.StopAtBounds

                            delegate: ItemDelegate {
                                id: policyDelegate
                                required property var modelData
                                required property int index
                                width: policyList.width
                                highlighted: root.hasSelection && root.selectedPolicy.id === modelData.id
                                leftPadding: 10
                                rightPadding: 10
                                topPadding: 8
                                bottomPadding: 8

                                onClicked: {
                                    policyList.currentIndex = index
                                    root.selectPolicy(modelData)
                                }

                                contentItem: RowLayout {
                                    spacing: 10

                                    Rectangle {
                                        Layout.preferredWidth: 34
                                        Layout.preferredHeight: 34
                                        radius: 10
                                        color: policyDelegate.highlighted
                                               ? (Material.theme === Material.Dark ? "#6750A4" : "#6750A4")
                                               : (Material.theme === Material.Dark ? "#36343B" : "#F3EDF7")
                                        Label {
                                            anchors.centerIn: parent
                                            text: policyDelegate.modelData.policyClass === "Machine" ? "C"
                                                  : policyDelegate.modelData.policyClass === "User" ? "U" : "B"
                                            color: policyDelegate.highlighted ? "white" : Material.accent
                                            font.weight: Font.Bold
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Label {
                                            Layout.fillWidth: true
                                            text: policyDelegate.modelData.name
                                            font.weight: Font.DemiBold
                                            wrapMode: Text.Wrap
                                            maximumLineCount: 2
                                            elide: Text.ElideRight
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            text: policyDelegate.modelData.category || root.tr("Uncategorized policy", "未分類政策")
                                            font.pixelSize: 10
                                            elide: Text.ElideMiddle
                                            color: root.secondaryTextColor
                                        }
                                    }

                                    Label {
                                        visible: policyDelegate.modelData.elements && policyDelegate.modelData.elements.length > 0
                                        text: policyDelegate.modelData.elements.length
                                        color: policyDelegate.highlighted ? "white" : root.secondaryTextColor
                                        font.pixelSize: 10
                                        padding: 4
                                        background: Rectangle {
                                            radius: 8
                                            color: policyDelegate.highlighted
                                                   ? (Material.theme === Material.Dark ? "#7D6CB2" : "#7D6CB2")
                                                   : (Material.theme === Material.Dark ? "#49454F" : "#E7E0EC")
                                        }
                                    }
                                }

                                background: Rectangle {
                                    radius: 14
                                    color: policyDelegate.highlighted
                                           ? (Material.theme === Material.Dark ? "#4A4458" : "#E8DEF8")
                                           : policyDelegate.hovered
                                             ? (Material.theme === Material.Dark ? "#2B292F" : "#F7F2FA")
                                             : "transparent"
                                }
                            }
                        }

                        Column {
                            anchors.centerIn: parent
                            width: Math.min(parent.width - 40, 280)
                            spacing: 10
                            visible: root.app.gpoResults.length === 0
                            BusyIndicator {
                                anchors.horizontalCenter: parent.horizontalCenter
                                running: !root.app.gpoLoaded
                                visible: running
                            }
                            Label {
                                width: parent.width
                                text: root.app.gpoLoaded
                                      ? root.tr("No matching policies", "冇相符政策")
                                      : root.tr("Loading installed policies…", "正在載入已安裝政策…")
                                horizontalAlignment: Text.AlignHCenter
                                font.pixelSize: 17
                                font.weight: Font.DemiBold
                            }
                            Label {
                                width: parent.width
                                text: root.app.gpoLoaded
                                      ? root.tr("Try fewer terms or turn off Regex.", "試下減少字詞或關閉 Regex。")
                                      : root.tr("Reading ADMX and language resources from Windows.", "正在讀取 Windows 嘅 ADMX 同語言資源。")
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.Wrap
                                color: root.secondaryTextColor
                            }
                        }
                    }
                }
            }

            Pane {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 0
                Layout.minimumHeight: 120
                padding: 0
                background: Rectangle {
                    radius: 18
                    color: root.surfaceColor
                    border.color: root.outlineColor
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        Column {
                            anchors.centerIn: parent
                            width: Math.min(440, parent.width - 48)
                            spacing: 14
                            visible: !root.hasSelection

                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: 64
                                height: 64
                                radius: 22
                                color: Material.theme === Material.Dark ? "#4A4458" : "#E8DEF8"
                                Label {
                                    anchors.centerIn: parent
                                    text: "☷"
                                    font.pixelSize: 28
                                    color: Material.accent
                                }
                            }
                            Label {
                                width: parent.width
                                text: root.tr("Select a policy to start a reviewable change", "揀一項政策開始可審核變更")
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.Wrap
                                font.pixelSize: 21
                                font.weight: Font.DemiBold
                            }
                            Label {
                                width: parent.width
                                text: root.tr("You will see the official explanation, scope, registry target, and a schema-matched value editor before anything is committed.",
                                              "Commit 之前，你會見到官方解說、範圍、登錄目標同 schema 配對值編輯器。")
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.Wrap
                                color: root.secondaryTextColor
                            }
                        }

                        ScrollView {
                            id: detailsScroll
                            anchors.fill: parent
                            visible: root.hasSelection
                            clip: true
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                            ColumnLayout {
                                width: Math.max(0, detailsScroll.availableWidth - 30)
                                x: 15
                                spacing: 12

                                Item { Layout.preferredHeight: 3 }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    Rectangle {
                                        Layout.preferredWidth: scopeChip.implicitWidth + 20
                                        Layout.preferredHeight: 28
                                        radius: 14
                                        color: Material.theme === Material.Dark ? "#4A4458" : "#E8DEF8"
                                        Label {
                                            id: scopeChip
                                            anchors.centerIn: parent
                                            text: root.scopeName().toUpperCase()
                                            color: Material.theme === Material.Dark ? "#E8DEF8" : "#4F378B"
                                            font.pixelSize: 10
                                            font.weight: Font.Bold
                                        }
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: root.selectedPolicy ? root.selectedPolicy.category : ""
                                        color: root.secondaryTextColor
                                        elide: Text.ElideMiddle
                                    }
                                    Label {
                                        text: root.tr("NEW DRAFT", "新草稿")
                                        color: Material.accent
                                        font.pixelSize: 10
                                        font.weight: Font.Bold
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: root.selectedPolicy ? root.selectedPolicy.name : ""
                                    font.pixelSize: 25
                                    font.weight: Font.Bold
                                    wrapMode: Text.Wrap
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    visible: root.selectedPolicy && root.selectedPolicy.policyClass === "Both"
                                    Label {
                                        text: root.tr("Apply for", "套用到")
                                        color: root.secondaryTextColor
                                    }
                                    ComboBox {
                                        id: policyScope
                                        model: [root.tr("Computer", "電腦"), root.tr("User", "使用者")]
                                    }
                                    Item { Layout.fillWidth: true }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: root.selectedPolicy ? root.selectedPolicy.documentation : ""
                                    wrapMode: Text.Wrap
                                    textFormat: Text.PlainText
                                    color: root.secondaryTextColor
                                    maximumLineCount: root.documentationExpanded ? 100 : 3
                                    elide: Text.ElideRight
                                }

                                Label {
                                    Layout.fillWidth: true
                                    visible: root.selectedPolicy && root.selectedPolicy.supportedOn.length > 0
                                    text: root.tr("Supported on: %1", "支援：%1")
                                              .arg(root.selectedPolicy ? root.selectedPolicy.supportedOn : "")
                                    color: Material.accent
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }

                                Button {
                                    visible: root.selectedPolicy && root.selectedPolicy.documentation.length > 240
                                    text: root.documentationExpanded ? root.tr("Show less", "顯示較少") : root.tr("Read full explanation", "閱讀完整解說")
                                    flat: true
                                    onClicked: root.documentationExpanded = !root.documentationExpanded
                                }

                                Pane {
                                    Layout.fillWidth: true
                                    padding: 14
                                    background: Rectangle {
                                        radius: 16
                                        color: root.surfaceVariantColor
                                        border.color: root.outlineColor
                                    }
                                    ColumnLayout {
                                        anchors.fill: parent
                                        spacing: 8
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Label {
                                                text: root.tr("Desired policy state", "目標政策狀態")
                                                font.pixelSize: 17
                                                font.weight: Font.DemiBold
                                            }
                                            Item { Layout.fillWidth: true }
                                            Label {
                                                text: root.tr("Draft only", "只係草稿")
                                                color: Material.accent
                                                font.pixelSize: 10
                                                font.weight: Font.Bold
                                            }
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            text: root.tr("Choose the state that the built image should receive.", "揀建置完成嘅映像應該收到邊個狀態。")
                                            color: root.secondaryTextColor
                                            wrapMode: Text.Wrap
                                        }
                                        TabBar {
                                            id: stateTabs
                                            Layout.fillWidth: true
                                            currentIndex: 1
                                            onCurrentIndexChanged: root.draftState = currentIndex
                                            TabButton { text: root.tr("Not configured", "未設定") }
                                            TabButton { text: root.tr("Enabled", "啟用") }
                                            TabButton { text: root.tr("Disabled", "停用") }
                                        }
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    visible: root.selectedPolicy && root.selectedPolicy.elements.length > 0
                                    enabled: root.draftState === 1
                                    opacity: enabled ? 1.0 : 0.55

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label {
                                            text: root.tr("Enabled values", "啟用值")
                                            font.pixelSize: 17
                                            font.weight: Font.DemiBold
                                        }
                                        Item { Layout.fillWidth: true }
                                        Label {
                                            text: root.selectedPolicy ? root.selectedPolicy.elements.length + " " + root.tr("field(s)", "個欄位") : ""
                                            color: root.secondaryTextColor
                                        }
                                    }

                                    Repeater {
                                        model: root.selectedPolicy ? root.selectedPolicy.elements : []
                                        delegate: Pane {
                                            id: elementCard
                                            required property var modelData
                                            Layout.fillWidth: true
                                            padding: 12
                                            background: Rectangle {
                                                radius: 14
                                                color: root.surfaceVariantColor
                                                border.color: root.outlineColor
                                            }
                                            ColumnLayout {
                                                anchors.fill: parent
                                                spacing: 7
                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    Label {
                                                        Layout.fillWidth: true
                                                        text: elementCard.modelData.label + (elementCard.modelData.required ? "  *" : "")
                                                        font.weight: Font.DemiBold
                                                        wrapMode: Text.Wrap
                                                    }
                                                    Label {
                                                        text: elementCard.modelData.control
                                                        font.pixelSize: 9
                                                        font.weight: Font.Bold
                                                        color: Material.accent
                                                    }
                                                }
                                                Loader {
                                                    Layout.fillWidth: true
                                                    Layout.preferredHeight: elementModel.control === "TextArea"
                                                                            || elementModel.control === "ListEditor" ? 96 : 48
                                                    property var elementModel: elementCard.modelData
                                                    onLoaded: {
                                                        if (item)
                                                            item["elementModel"] = elementModel
                                                    }
                                                    onElementModelChanged: {
                                                        if (item)
                                                            item["elementModel"] = elementModel
                                                    }
                                                    sourceComponent: elementModel.control === "Switch" ? switchEditor
                                                                   : elementModel.control === "ComboBox" ? comboEditor
                                                                   : elementModel.control === "SpinBox" && elementModel.numericTextEditor ? numericTextEditor
                                                                   : elementModel.control === "SpinBox" ? spinEditor
                                                                   : elementModel.control === "TextArea" || elementModel.control === "ListEditor" ? areaEditor
                                                                   : textEditor
                                                }
                                            }
                                        }
                                    }
                                }

                                Pane {
                                    Layout.fillWidth: true
                                    padding: 12
                                    background: Rectangle {
                                        radius: 14
                                        color: Material.theme === Material.Dark ? "#1D2020" : "#F2F7EE"
                                        border.color: Material.theme === Material.Dark ? "#53634B" : "#C4D8B8"
                                    }
                                    ColumnLayout {
                                        anchors.fill: parent
                                        spacing: 5
                                        RowLayout {
                                            Layout.fillWidth: true
                                            Label {
                                                text: root.tr("Policy target", "政策目標")
                                                font.weight: Font.DemiBold
                                                color: Material.theme === Material.Dark ? "#C7E0B8" : "#386A20"
                                            }
                                            Item { Layout.fillWidth: true }
                                            Label {
                                                text: root.selectedPolicy && root.selectedPolicy.registryValue
                                                      ? root.tr("Value", "值") + ": " + root.selectedPolicy.registryValue
                                                      : root.tr("Policy-defined values", "政策定義值")
                                                color: root.secondaryTextColor
                                                font.pixelSize: 10
                                            }
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            text: root.registryTarget()
                                            font.family: "Cascadia Mono"
                                            font.pixelSize: 11
                                            wrapMode: Text.WrapAnywhere
                                        }
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    visible: root.selectedPolicy && root.selectedPolicy.elements.length === 0
                                    text: root.tr("This policy has no additional values. Its state fully defines the change.",
                                                  "呢項政策冇額外值；狀態已完整定義變更。")
                                    wrapMode: Text.Wrap
                                    color: root.secondaryTextColor
                                    horizontalAlignment: Text.AlignHCenter
                                    padding: 12
                                }

                                Item { Layout.preferredHeight: 4 }
                            }
                        }
                    }

                    Rectangle {
                        visible: root.hasSelection
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: root.outlineColor
                    }

                    Pane {
                        visible: root.hasSelection
                        Layout.fillWidth: true
                        padding: 14
                        background: Rectangle {
                            color: root.surfaceVariantColor
                            radius: 18
                        }
                        RowLayout {
                            anchors.fill: parent
                            spacing: 12
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1
                                Label {
                                    text: root.tr("Git-backed recipe change", "Git 支援嘅配方變更")
                                    font.weight: Font.DemiBold
                                    color: Material.theme === Material.Dark ? "#C7E0B8" : "#386A20"
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: root.app.projectLoaded
                                          ? root.tr("Updates the build project; this PC's live policy stays untouched.", "只更新建置工程；呢部電腦嘅即時政策唔會改。")
                                          : root.tr("Open a project before committing this draft.", "先開啟工程，先可以 commit 呢份草稿。")
                                    color: root.secondaryTextColor
                                    font.pixelSize: 10
                                    wrapMode: Text.Wrap
                                }
                            }
                            Button {
                                text: root.tr("Commit %1", "Commit %1").arg(root.stateName())
                                highlighted: true
                                enabled: root.app.projectLoaded
                                onClicked: root.commitDraft()
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: switchEditor
        Switch {
            property var elementModel: ({})
            text: checked ? root.tr("On", "開") : root.tr("Off", "關")
            Accessible.name: elementModel.label
            checked: elementModel.defaultChecked
            onToggled: root.elementValues[elementModel.id] = checked
        }
    }

    Component {
        id: comboEditor
        ComboBox {
            property var elementModel: ({})
            width: parent ? parent.width : implicitWidth
            model: elementModel.options
            textRole: "label"
            Accessible.name: elementModel.label
            Component.onCompleted: {
                for (var i = 0; i < model.length; ++i) {
                    if (model[i].value === elementModel.defaultValue) {
                        currentIndex = i
                        break
                    }
                }
            }
            onActivated: root.elementValues[elementModel.id] = model[index].value
        }
    }

    Component {
        id: spinEditor
        SpinBox {
            property var elementModel: ({})
            from: Math.max(-2147483648, Number(elementModel.minimum))
            to: Math.min(2147483647, Number(elementModel.maximum))
            value: Number(elementModel.defaultValue || 0)
            stepSize: Math.max(1, Number(elementModel.spinStep))
            editable: true
            Accessible.name: elementModel.label
            onValueModified: root.elementValues[elementModel.id] = value
        }
    }

    Component {
        id: numericTextEditor
        TextField {
            property var elementModel: ({})
            width: parent ? parent.width : implicitWidth
            text: elementModel.defaultValue
            maximumLength: 20
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            Accessible.name: elementModel.label
            placeholderText: root.tr("Integer from %1 to %2", "整數範圍 %1 至 %2")
                                 .arg(elementModel.minimum).arg(elementModel.maximum)
            validator: RegularExpressionValidator { regularExpression: /^-?[0-9]+$/ }
            onTextEdited: root.elementValues[elementModel.id] = text
        }
    }

    Component {
        id: textEditor
        TextField {
            property var elementModel: ({})
            width: parent ? parent.width : implicitWidth
            text: elementModel.defaultValue
            maximumLength: elementModel.maximumLength >= 0 ? elementModel.maximumLength : 32767
            placeholderText: root.tr("Value", "值")
            Accessible.name: elementModel.label
            onTextEdited: root.elementValues[elementModel.id] = text
        }
    }

    Component {
        id: areaEditor
        TextArea {
            property var elementModel: ({})
            width: parent ? parent.width : implicitWidth
            implicitHeight: 96
            text: elementModel.defaultValue
            placeholderText: elementModel.control === "ListEditor"
                             ? root.tr("One entry per line", "每行一項") : root.tr("Text", "文字")
            wrapMode: TextEdit.Wrap
            Accessible.name: elementModel.label
            onTextChanged: root.elementValues[elementModel.id] = text
        }
    }

    Popup {
        id: regexWizard
        anchors.centerIn: Overlay.overlay
        width: Math.min(620, root.width - 50)
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: 22
        background: Rectangle {
            radius: 24
            color: root.surfaceColor
            border.color: Material.accent
        }
        ColumnLayout {
            anchors.fill: parent
            Label {
                text: "🧩  " + root.tr("Regex builder wizard", "Regex 建構精靈")
                font.pixelSize: 22
                font.weight: Font.Bold
            }
            Label {
                Layout.fillWidth: true
                text: root.tr("Build a safe expression without memorizing punctuation. Invalid expressions stay in this panel and never interrupt other work.",
                              "唔使背符號都砌到安全 expression；錯咗只會留喺呢塊 panel，唔會阻住其他工序。")
                wrapMode: Text.Wrap
            }
            TextField {
                id: containsWords
                Layout.fillWidth: true
                placeholderText: root.tr("Contains words (space separated)", "包含字詞（空格分隔）")
            }
            TextField {
                id: startsWith
                Layout.fillWidth: true
                placeholderText: root.tr("Starts with (optional)", "開頭係（可選）")
            }
            TextField {
                id: endsWith
                Layout.fillWidth: true
                placeholderText: root.tr("Ends with (optional)", "結尾係（可選）")
            }
            CheckBox { id: wholeWord; text: root.tr("Whole words", "完整字詞") }
            Label {
                id: regexPreview
                Layout.fillWidth: true
                text: {
                    var parts = containsWords.text.trim().split(/\s+/).filter(function(x) { return x.length > 0 })
                    var body = parts.length ? parts.map(function(x) { return "(?=.*" + x.replace(/[.*+?^${}()|[\]\\]/g, "\\$&") + ")" }).join("") + ".*" : ".*"
                    if (wholeWord.checked && parts.length === 1)
                        body = "\\b" + parts[0].replace(/[.*+?^${}()|[\]\\]/g, "\\$&") + "\\b"
                    return "^" + startsWith.text.replace(/[.*+?^${}()|[\]\\]/g, "\\$&") + body
                           + endsWith.text.replace(/[.*+?^${}()|[\]\\]/g, "\\$&") + "$"
                }
                font.family: "Cascadia Mono"
                color: Material.accent
                wrapMode: Text.Wrap
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: root.tr("Cancel", "取消")
                    flat: true
                    onClicked: regexWizard.close()
                }
                Button {
                    text: root.tr("Use && search", "使用同搜尋")
                    highlighted: true
                    onClicked: {
                        policySearch.text = regexPreview.text
                        regexMode.checked = true
                        regexWizard.close()
                        root.app.searchGpo(policySearch.text, true)
                    }
                }
            }
        }
    }

    Popup {
        id: docsExport
        anchors.centerIn: Overlay.overlay
        width: Math.min(560, root.width - 50)
        modal: false
        focus: true
        padding: 20
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            radius: 22
            color: root.surfaceColor
            border.color: Material.accent
        }
        ColumnLayout {
            anchors.fill: parent
            Label {
                text: root.tr("Export complete bilingual GPO reference", "匯出完整雙語 GPO 參考")
                font.pixelSize: 20
                font.weight: Font.Bold
            }
            TextField {
                id: docsPath
                Layout.fillWidth: true
                placeholderText: "D:\\docs\\all-installed-policies.md"
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    text: root.tr("Cancel", "取消")
                    flat: true
                    onClicked: docsExport.close()
                }
                Button {
                    text: root.tr("Export", "匯出")
                    highlighted: true
                    enabled: docsPath.text.trim().length > 0
                    onClicked: {
                        if (root.app.exportGpoDocumentation(docsPath.text))
                            docsExport.close()
                    }
                }
            }
        }
    }
}
