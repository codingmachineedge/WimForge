import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root
    required property var app
    property var tr: function(en, zh) { return en }

    parent: Overlay.overlay
    x: Math.max(16, Math.round((parent.width - width) / 2))
    y: Math.max(16, Math.min(90, Math.round((parent.height - height) / 3)))
    width: Math.min(820, parent.width - 32)
    height: Math.min(660, parent.height - 32)
    modal: true
    focus: true
    padding: 18
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function openForQuery(query) {
        queryField.text = query
        open()
        queryField.forceActiveFocus()
        queryField.selectAll()
    }

    onClosed: app.clearSearch()

    background: Rectangle {
        radius: 24
        color: Material.theme === Material.Dark ? "#211F26" : "#FFFBFE"
        border.width: 1
        border.color: Material.theme === Material.Dark ? "#79747E" : "#CAC4D0"
    }

    Timer {
        id: searchDelay
        interval: 160
        repeat: false
        onTriggered: app.search(queryField.text)
    }

    contentItem: ColumnLayout {
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Label { text: "⌕"; font.pixelSize: 26; color: Material.accent; Accessible.ignored: true }
            TextField {
                id: queryField
                Layout.fillWidth: true
                placeholderText: root.tr("Search pages, commands, settings, features, packages, policies and project data",
                                         "搜尋頁面、指令、設定、功能、套件、政策同工程資料")
                Accessible.name: placeholderText
                onTextEdited: searchDelay.restart()
                onAccepted: {
                    if (app.searchResults.length > 0) {
                        app.activateSearchResult(app.searchResults[0])
                        root.close()
                    } else {
                        app.search(text)
                    }
                }
                Keys.onDownPressed: {
                    if (resultList.count > 0) {
                        resultList.currentIndex = 0
                        resultList.forceActiveFocus()
                    }
                }
            }
            ToolButton {
                text: "×"
                Accessible.name: root.tr("Close search", "關閉搜尋")
                ToolTip.visible: hovered
                ToolTip.text: Accessible.name
                onClicked: root.close()
            }
        }

        Label {
            Layout.fillWidth: true
            text: app.searchResults.length === 0
                  ? root.tr("No matching local result", "搵唔到相符本機結果")
                  : root.tr("%1 ranked result(s)", "%1 項排序結果").arg(app.searchResults.length)
            color: Material.theme === Material.Dark ? "#CAC4D0" : "#625B71"
            wrapMode: Text.Wrap
        }

        ListView {
            id: resultList
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: app.searchResults
            clip: true
            spacing: 6
            keyNavigationEnabled: true
            highlightMoveDuration: app.motionEnabled ? 100 : 0
            highlight: Rectangle {
                radius: 14
                color: Material.theme === Material.Dark ? "#4A4458" : "#E8DEF8"
            }

            delegate: ItemDelegate {
                id: resultDelegate
                required property var modelData
                required property int index
                width: resultList.width
                implicitHeight: Math.max(66, resultRow.implicitHeight + 16)
                highlighted: ListView.isCurrentItem
                Accessible.name: modelData.kindLabel + ": " + modelData.title + ". " + modelData.subtitle
                onClicked: {
                    app.activateSearchResult(modelData)
                    root.close()
                }
                Keys.onReturnPressed: clicked()
                Keys.onEnterPressed: clicked()
                Keys.onEscapePressed: root.close()
                contentItem: RowLayout {
                    id: resultRow
                    spacing: 12
                    Label {
                        text: resultDelegate.modelData.icon
                        font.pixelSize: 22
                        color: Material.accent
                        Accessible.ignored: true
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label {
                            Layout.fillWidth: true
                            text: resultDelegate.modelData.title
                            font.weight: Font.DemiBold
                            wrapMode: Text.Wrap
                        }
                        Label {
                            Layout.fillWidth: true
                            text: resultDelegate.modelData.subtitle
                            color: Material.theme === Material.Dark ? "#CAC4D0" : "#625B71"
                            wrapMode: Text.WrapAnywhere
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                    }
                    Label {
                        text: resultDelegate.modelData.kindLabel
                        font.pixelSize: 10
                        color: Material.accent
                        Accessible.ignored: true
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: root.tr("↑/↓ move · Enter open · Esc close", "↑/↓ 移動 · Enter 開啟 · Esc 關閉")
            horizontalAlignment: Text.AlignRight
            font.pixelSize: 10
            color: Material.theme === Material.Dark ? "#CAC4D0" : "#625B71"
        }
    }
}
