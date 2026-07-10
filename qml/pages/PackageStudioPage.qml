pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    required property var app
    required property var tr

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        GridLayout {
            Layout.fillWidth: true
            columns: root.width >= 660 ? 2 : 1
            columnSpacing: 12
            rowSpacing: 10
            ColumnLayout {
                Layout.fillWidth: true
                Label { Layout.fillWidth: true; text: root.tr("Package Manager Studio", "套件管理工房"); font.pixelSize: 30; font.weight: Font.Bold; wrapMode: Text.Wrap }
                Label {
                    Layout.fillWidth: true
                    text: root.tr("Choose software for the finished ISO. Dependencies, offline payloads, signatures, retries and crash-resume state stay explicit.",
                                  "揀完成 ISO 要裝咩軟件；依賴、離線 payload、簽署、重試同斷電續跑狀態全部寫清楚。")
                    wrapMode: Text.Wrap; color: Material.theme === Material.Dark ? "#CAC4D0" : "#625B71"
                }
            }
            Pane {
                Layout.alignment: root.width >= 660 ? Qt.AlignRight | Qt.AlignVCenter : Qt.AlignLeft
                padding: 12
                background: Rectangle { radius: 18; color: Material.theme === Material.Dark ? "#4A4458" : "#E8DEF8" }
                ColumnLayout {
                    Label { text: app.selectedPackageCount; font.pixelSize: 26; font.bold: true; Layout.alignment: Qt.AlignHCenter }
                    Label { text: root.tr("selected", "已揀"); font.pixelSize: 10; Layout.alignment: Qt.AlignHCenter }
                }
            }
        }

        Pane {
            Layout.fillWidth: true
            padding: 12
            background: Rectangle { radius: 18; color: Material.theme === Material.Dark ? "#211F26" : "#FFFBFE"; border.color: Material.theme === Material.Dark ? "#49454F" : "#E7E0EC" }
            GridLayout {
                anchors.fill: parent
                columns: root.width >= 820 ? 3 : 1
                columnSpacing: 8
                rowSpacing: 8
                TextField { id: packageSearch; Layout.fillWidth: true; placeholderText: root.tr("Search software, provider, package ID…", "搜尋軟件、provider、套件 ID…") }
                Button { Layout.fillWidth: root.width < 820; text: "✦  " + root.tr("Full AI Development ISO", "完整 AI 開發 ISO"); highlighted: true; onClicked: app.loadAiDevelopmentPackageTemplate() }
                Button { Layout.fillWidth: root.width < 820; text: "▣  " + root.tr("Stage into ISO", "放入 ISO"); enabled: app.projectLoaded && app.selectedPackageCount > 0; onClicked: app.stagePackageProfile() }
            }
        }

        Pane {
            Layout.fillWidth: true
            padding: 10
            background: Rectangle { radius: 16; color: app.openCodeInstalled ? (Material.theme === Material.Dark ? "#17351F" : "#EAF8E6") : (Material.theme === Material.Dark ? "#3A2E00" : "#FFF4D6") }
            GridLayout {
                anchors.fill: parent
                columns: root.width >= 700 ? 4 : 1
                columnSpacing: 8
                rowSpacing: 6
                Label { text: app.openCodeInstalled ? "✓" : "⬇"; font.pixelSize: 20 }
                Label { Layout.fillWidth: true; text: app.openCodeStatus; wrapMode: Text.Wrap }
                BusyIndicator {
                    visible: app.openCodeBusy
                    running: visible
                    implicitWidth: 28
                    implicitHeight: 28
                    Accessible.name: root.tr("OpenCode installation in progress", "OpenCode 安裝進行中")
                }
                Button { visible: !app.openCodeInstalled; Layout.fillWidth: root.width < 700; enabled: !app.openCodeBusy; text: root.tr("Install now", "而家安裝"); onClicked: app.ensureOpenCode() }
            }
        }

        ListView {
            id: packageList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: app.packageCatalog
            delegate: Pane {
                id: packageCard
                required property var modelData
                readonly property bool matches: packageSearch.text.trim().length === 0
                    || (modelData.name + " " + modelData.identifier + " " + modelData.provider).toLowerCase().indexOf(packageSearch.text.toLowerCase()) >= 0
                width: packageList.width
                height: matches ? implicitHeight : 0
                visible: matches
                padding: 14
                background: Rectangle { radius: 17; color: Material.theme === Material.Dark ? "#211F26" : "#FFFBFE"; border.color: modelData.enabled ? Material.accent : (Material.theme === Material.Dark ? "#49454F" : "#E7E0EC"); border.width: modelData.enabled ? 2 : 1 }
                RowLayout {
                    anchors.fill: parent
                    Switch {
                        checked: packageCard.modelData.enabled
                        Accessible.name: root.tr("Include %1", "包括 %1").arg(packageCard.modelData.name)
                        onClicked: app.setPackageEnabled(packageCard.modelData.id, checked)
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        GridLayout {
                            Layout.fillWidth: true
                            columns: packageList.width >= 680 ? 2 : 1
                            Label { Layout.fillWidth: true; text: packageCard.modelData.name; font.pixelSize: 17; font.weight: Font.DemiBold; wrapMode: Text.Wrap }
                            Label { Layout.alignment: packageList.width >= 680 ? Qt.AlignRight : Qt.AlignLeft; text: packageCard.modelData.version; wrapMode: Text.Wrap }
                        }
                        Flow {
                            Layout.fillWidth: true
                            Layout.preferredHeight: childrenRect.height
                            spacing: 8
                            Label { text: packageCard.modelData.provider; color: Material.accent; font.pixelSize: 11 }
                            Label {
                                visible: packageCard.modelData.optional
                                text: root.tr("Optional payload", "可選 payload")
                                color: Material.theme === Material.Dark ? "#FFD18B" : "#8B5000"
                                font.pixelSize: 10
                            }
                        }
                        Label { Layout.fillWidth: true; text: packageCard.modelData.description; wrapMode: Text.Wrap; color: Material.theme === Material.Dark ? "#CAC4D0" : "#625B71" }
                        Label { Layout.fillWidth: true; text: packageCard.modelData.identifier; font.family: "Cascadia Mono"; font.pixelSize: 10; color: Material.accent; elide: Text.ElideMiddle }
                    }
                }
            }
        }

        Pane {
            Layout.fillWidth: true
            padding: 10
            background: Rectangle { radius: 14; color: Material.theme === Material.Dark ? "#211F26" : "#F7F2FA" }
            GridLayout {
                anchors.fill: parent
                columns: root.width >= 780 ? 4 : 1
                columnSpacing: 8
                rowSpacing: 8
                Label { text: "↕  " + root.tr("Profile file", "設定檔") }
                TextField { id: profilePath; Layout.fillWidth: true; placeholderText: "D:\\profiles\\software.json" }
                Button { Layout.fillWidth: root.width < 780; text: root.tr("Import", "匯入"); enabled: profilePath.text.trim().length > 0; onClicked: app.importPackageProfile(profilePath.text) }
                Button { Layout.fillWidth: root.width < 780; text: root.tr("Export", "匯出"); enabled: profilePath.text.trim().length > 0; onClicked: app.exportPackageProfile(profilePath.text) }
            }
        }
    }
}
