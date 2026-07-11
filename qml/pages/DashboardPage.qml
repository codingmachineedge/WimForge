import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

ScrollView {
    id: root
    property var app
    property var tr: function(en, zh) { return en }
    property var openPage: function(index) {}
    clip: true

    ColumnLayout {
        width: root.availableWidth
        spacing: 18

        GridLayout {
            Layout.fillWidth: true
            columns: root.availableWidth >= 620 ? 2 : 1
            columnSpacing: 12
            rowSpacing: 10
            Label {
                Layout.fillWidth: true
                text: root.tr("Good afternoon, image wrangler", "晏晝好，映像馴獸師")
                font.pixelSize: 30
                font.weight: Font.Bold
                wrapMode: Text.Wrap
            }
            Button {
                Layout.alignment: root.availableWidth >= 620 ? Qt.AlignRight | Qt.AlignVCenter : Qt.AlignLeft
                Layout.fillWidth: root.availableWidth < 620
                icon.name: "document-new"
                text: root.tr("New project", "開新工程")
                highlighted: true
                onClicked: app.requestNewProject()
            }
        }

        Label {
            Layout.fillWidth: true
            text: app.projectLoaded
                  ? root.tr("Your image recipe is saved after every change. Git is standing behind you with a very large safety net.",
                            "每次改動都自動儲存。Git 喺後面擔住你，個安全網大過維港。")
                  : root.tr("Create or import a project to start. Nothing touches an image until you review and run the plan.",
                            "開個新工程或者匯入設定先。未睇清楚同撳執行之前，WimForge 唔會掂你個映像。")
            color: Material.theme === Material.Dark ? "#CAC4D0" : "#625B71"
            wrapMode: Text.Wrap
        }

        GridLayout {
            Layout.fillWidth: true
            columns: width > 1050 ? 4 : width > 680 ? 2 : 1
            columnSpacing: 14
            rowSpacing: 14

            MetricCard {
                Layout.fillWidth: true
                eyebrow: root.tr("PROJECT", "工程")
                value: app.projectLoaded ? app.projectName : root.tr("None", "未開")
                detail: app.projectLoaded ? app.projectRoot : root.tr("Ready when you are", "等你開工")
                glyph: "▣"
                accent: Material.theme === Material.Dark ? "#D0BCFF" : "#6750A4"
            }
            MetricCard {
                Layout.fillWidth: true
                eyebrow: root.tr("OPERATIONS", "工序")
                value: String(app.operationCount)
                detail: root.tr("in the reviewed plan", "已排入檢查過嘅計劃")
                glyph: "⚙"
                accent: Material.theme === Material.Dark ? "#9CDADA" : "#006A6A"
            }
            MetricCard {
                Layout.fillWidth: true
                eyebrow: root.tr("HISTORY", "歷史")
                value: String(app.projectHistoryCount)
                detail: root.tr("recoverable Git commits", "可還原 Git commit")
                glyph: "↶"
                accent: Material.theme === Material.Dark ? "#FFD18B" : "#8B5000"
            }
            MetricCard {
                Layout.fillWidth: true
                eyebrow: root.tr("RUNNING", "執行緊")
                value: String(app.runningJobCount)
                detail: root.tr("of %1 parallel slots", "共 %1 個平行位")
                        .arg(app.maxParallelJobs)
                glyph: "▶"
                accent: Material.theme === Material.Dark ? "#A8D5A2" : "#386A20"
            }
        }

        Pane {
            Layout.fillWidth: true
            padding: 20
            background: Rectangle {
                radius: 20
                color: Material.theme === Material.Dark ? "#211F26" : "#FFFBFE"
                border.color: Material.theme === Material.Dark ? "#49454F" : "#E7E0EC"
            }

            ColumnLayout {
                width: parent.width
                spacing: 14
                Label {
                    text: root.tr("Build flow", "整碟流程")
                    font.pixelSize: 20
                    font.weight: Font.Bold
                }
                Repeater {
                    model: [
                        { icon: "①", en: "Choose Windows ISO / WIM / ESD / SWM", zh: "揀 Windows ISO / WIM / ESD / SWM", page: 1 },
                        { icon: "②", en: "Select editions and customize the recipe", zh: "揀版本，再調校你份配方", page: 2 },
                        { icon: "③", en: "Review exact commands and safety checks", zh: "逐條睇清楚指令同安全檢查", page: 8 },
                        { icon: "④", en: "Run concurrently with crash-safe checkpoints", zh: "平行開工，仲有防死機檢查點", page: 8 },
                        { icon: "⑤", en: "Load the ISO in VMware or VirtualBox and record validation", zh: "將 ISO 載入 VMware 或 VirtualBox，再記錄驗證", page: 7 }
                    ]
                    delegate: ItemDelegate {
                        required property var modelData
                        Layout.fillWidth: true
                        icon.name: "go-next"
                        text: modelData.icon + "  " + root.tr(modelData.en, modelData.zh)
                        enabled: app.projectLoaded || modelData.page === 1
                        onClicked: root.openPage(modelData.page)
                    }
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: width > 800 ? 2 : 1
            columnSpacing: 14

            GroupBox {
                Layout.fillWidth: true
                title: root.tr("Safety rails", "安全欄杆")
                ColumnLayout {
                    width: parent.width
                    Label { Layout.fillWidth: true; text: "✓ " + root.tr("Source images are never overwritten by default", "預設永遠唔會覆蓋原裝映像"); wrapMode: Text.Wrap }
                    Label { Layout.fillWidth: true; text: "✓ " + root.tr("Every config edit is committed automatically", "每次改設定都自動 commit"); wrapMode: Text.Wrap }
                    Label { Layout.fillWidth: true; text: "✓ " + root.tr("Jobs checkpoint before destructive steps", "危險工序之前一定落檢查點"); wrapMode: Text.Wrap }
                    Label { Layout.fillWidth: true; text: "✓ " + root.tr("Interrupted mounts are detected on restart", "重開會搵返中斷咗嘅掛載"); wrapMode: Text.Wrap }
                }
            }
            GroupBox {
                Layout.fillWidth: true
                title: root.tr("Current job", "而家做緊")
                ColumnLayout {
                    width: parent.width
                    Label {
                        Layout.fillWidth: true
                        text: app.statusText
                        wrapMode: Text.Wrap
                    }
                    ProgressBar { Layout.fillWidth: true; value: app.progress; indeterminate: app.busy && app.progress <= 0 }
                    RowLayout {
                        BusyIndicator {
                            running: app.busy
                            implicitWidth: 28
                            implicitHeight: 28
                            Accessible.name: app.busy ? root.tr("Servicing job running", "維護工序執行中") : root.tr("No servicing job running", "冇維護工序執行中")
                        }
                        Label {
                            Layout.fillWidth: true
                            text: app.busy ? root.tr("You can keep editing another project while this runs.", "佢做緊嘢嗰陣，你照樣可以改第二個工程。")
                                           : root.tr("No active servicing jobs", "而家冇工序行緊")
                            wrapMode: Text.Wrap
                            color: Material.theme === Material.Dark ? "#CAC4D0" : "#625B71"
                        }
                    }
                }
            }
        }
        Item { Layout.preferredHeight: 20 }
    }
}
