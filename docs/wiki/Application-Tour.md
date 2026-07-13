# Application Tour

WimForge presents one Material desktop shell around twelve workflow pages. The pages edit a declarative project, manage reviewed VM operations, or host an explicit in-app shell; image-changing commands appear only after the project is converted into a reviewable servicing plan.

For a safe tour that does not require production media, start the populated demo project:

```powershell
.\WimForge.exe --demo --language bilingual --page overview
```

The accepted language values are `en`, `zh-HK`, and `bilingual`. Accepted page IDs are `overview`, `source`, `customize`, `gpo`, `unattended`, `packages`, `winforge`, `vmlab`, `plan`, `history`, `settings`, and `terminal`. `vm-lab` is accepted as an alias for `vmlab` at startup.

## Global shell

The navigation rail remains available throughout the desktop workflow. At narrower desktop widths it becomes a compact icon rail; each control retains a name and tooltip. Below roughly 1100 logical pixels, the header also gives search the available width and moves project, theme, and language actions into the accessible **More application actions** menu instead of clipping them. The header provides:

- a global search field that opens a ranked in-app palette for pages, commands, settings, features, packages, policies, and project data;
- a job indicator that opens **Review & run**;
- a notification bell with unread count and the recoverable notification drawer;
- the current project control, which opens the project/import sheet; and
- an interrupted-run banner when recovery state is detected.

The project summary at the bottom of the rail shows the active project, Git status, job progress, and serialized background work. Creating/opening/importing/exporting projects, saving project and tab state, committing notification lifecycle events, loading history/catalogs, building plans, scanning payloads, and inspecting ISO/catalog data run away from the UI thread. If a save fails, the queue pauses and the rail exposes **Retry save** so later changes do not silently overtake it. Informational sheets, snackbars, the notification drawer, Update Catalog results, and contextual history are in-app non-modal surfaces rather than native blocking dialogs.

窄過大約 1100 logical pixels 時，header 會將工程、theme 同語言操作收去有無障礙名稱嘅 **更多應用程式操作** menu，唔會硬擠到裁切。工程 rail 會顯示 Git、job 同有次序嘅後台工作狀態；工程／分頁儲存、通知、history、catalog、plan、payload 同 ISO 工作都唔會塞住 UI 主執行緒。儲存失敗會暫停隊列並出 **再試儲存**，之後嘅變更唔會偷偷爬過去。

Path selection is complete and non-modal across the main studios. Source has separate ISO/image, extracted-media, working-image, mount-folder, and output pickers; Settings has automatic-export save selection; Package Studio has distinct profile open/save pickers; GPO Studio has a documentation-export picker; VM Lab has ISO, existing VM configuration, virtual-disk, attached-ISO, and evidence pickers; WinForge Bridge has runtime-folder, recipe open/save, and staging-folder pickers. Each browse action has a purpose-specific accessible name instead of several indistinguishable **Browse** controls.

主要 Studio 嘅路徑選擇已經齊全兼非 modal。Source 分開 ISO／映像、已解壓 media、工作映像、mount 資料夾同 output picker；Settings 有自動匯出 save picker；Package Studio 分開 profile open／save；GPO Studio 有文件匯出；VM Lab 有 ISO、現有 VM 設定、虛擬磁碟、掛載 ISO 同 evidence；WinForge Bridge 有 runtime folder、recipe open／save 同 staging folder。每個 browse 動作都有講清楚用途嘅無障礙名稱，唔會成排掣都只叫「瀏覽」。

The supported minimum window is 900×640. At that size the compact toolbar, page-level scrolling, bounded inner lists, and compact one-column layouts keep lower actions reachable on Source, Customize, Package Studio, Review & Run, History, Settings, VM Lab, and WinForge Bridge rather than clipping them below the viewport.

支援嘅最細視窗係 900×640。去到呢個尺寸，compact toolbar、頁面捲動、有界內層清單同一欄 layout 仍然會令 Source、Customize、Package Studio、Review & Run、History、Settings、VM Lab 同 WinForge Bridge 下面啲操作搵得到，唔會跌咗落 viewport 外面就永遠撳唔到。

The tab strip below the header works like a browser: navigation opens or activates a project-local page tab, and each tab can be moved, closed, renamed, or given its own font family, size, color, bold, italic, and strikeout styling. Tabs expose the PageTab role, selected/focus state, and a visible keyboard focus ring; Left/Right moves between them, while Enter or Space activates one. The tab menu imports or exports portable definitions and complete Git-backed tab repositories. See [Workspace Tabs](Workspace-Tabs).

Header 下面嘅分頁列好似 browser。每個分頁會向輔助技術報告 PageTab、已選同 focus 狀態，鍵盤 focus 亦有清楚外框；左／右方向鍵切換，Enter 或 Space 啟用。分頁儲存同 Git commit 會排入後台隊列。

## The twelve pages

| Route | What to do there | Detailed guide |
| --- | --- | --- |
| **Overview** | See the current project, operation/history counts, running jobs, build flow, safety reminders, and current job progress. | [Getting Started](Getting-Started) |
| **Source & editions** | Select and inspect media, choose the working image/index, keep cloning enabled, and select an output. | [Projects and Sources](Projects-and-Sources) |
| **Customize** | Queue update, driver, feature, Appx, component, setting, answer-file, and post-setup intent. | [Customize](Customize) |
| **Group Policy Studio** | Load installed ADMX/ADML definitions, search them, edit schema-backed values, and commit registry policy state. | [Group Policy Studio](Group-Policy-Studio) |
| **Unattended Studio** | Build or import a portable profile and export Windows answer-file XML. | [Unattended Studio](Unattended-Studio) |
| **Package Studio** | Select first-logon software, validate provider/trust metadata, and stage a resumable bundle. | [Package Studio](Package-Studio) |
| **WinForge Bridge** | Review typed actions, detect a runtime contract, and stage a verified OEM payload. | [WinForge Bridge](WinForge-Bridge) |
| **Virtual Machine Lab** | Discover installed VMware/VirtualBox providers, create or load VMs, review lifecycle and device operations, manage snapshots, and retain validation evidence. | [Virtual Machine Lab](Virtual-Machine-Lab) |
| **Review & run** | Inspect commands, dependencies, risk markers, elevation, checkpoints, and execution state. | [Review and Run](Review-and-Run) |
| **History & recovery** | Selectively undo/redo project intent, inspect commits/diffs, and review notification history. | [History Time Machine](History-Time-Machine) |
| **Settings** | Choose language/theme, job limits, failsafes, project automation, and inspect notification storage. | [Settings](Settings) |
| **Embedded terminal** | Run an explicitly started elevated PowerShell or Command Prompt session through ConPTY with bounded in-app output and no external console. | [Embedded Terminal](Embedded-Terminal) |

## Keyboard and pointer actions

| Gesture | Behavior |
| --- | --- |
| `Ctrl+Z` | Undo in the active page context, unless focus is in a text editor |
| `Ctrl+Shift+Z` | Open the active-page/global contextual history manager |
| Right-click outside a text editor | Open that contextual manager at the pointer |
| `Ctrl+Enter` | Request confirmation for the current reviewed plan |
| `Ctrl+W` | Close the active workspace tab |
| `Ctrl+Tab` | Activate the next workspace tab |
| `Ctrl+Shift+Tab` | Activate the previous workspace tab |
| Search field: `Enter` | Activate the first ranked result; after moving into the result list, activate the focused result |
| Search palette: `Down` | Move focus from the query to the result list |
| Search palette: `Escape` | Close search without activation |
| VM inventory: `Enter` or `Space` | Select the focused machine row |
| Notification drawer: `Escape` | Close the drawer and return focus to the notification bell |

Undo appends compensating history; it does not rewrite the past or reverse external bytes already committed by DISM or an installer. See [History Time Machine](History-Time-Machine) and [Safety and Recovery](Safety-and-Recovery).

## Suggested first tour

The Overview cards make the primary sequence explicit: **Choose source → Customize → Review → Run → Validate**. A generated plan is labeled **ready for review**; it is not described as reviewed merely because planning succeeded. Review and Run remain separate steps even though both actions live on the same page.

Overview 會將主流程分清楚：**揀來源 → 自訂 → 審閱 → 執行 → 驗證**。Plan 產生成功只會標做「準備好俾你檢查」，唔會扮成已審閱；Review 同 Run 仍然係兩步，就算兩個動作放喺同一頁都一樣。

1. Open the demo on **Overview** and inspect the five-step flow plus project/job metrics.
2. Visit **Source & editions**, choose a source, and watch automatic inspection create its architecture/version/build profile and Update Catalog search. Open **Show advanced paths** only if you need the separate image, mount, or output fields.
3. Add or remove one reversible item in **Customize**; on Updates/Drivers, review the automatic ISO matches rather than inventing a search query.
4. Open **Review & run** and inspect the resulting command and dependency change without running it.
5. Use `Ctrl+Z`, then inspect the new action in **History & recovery**.
6. Create a test notification from **Settings** and exercise read, dismiss, restore, delete, and undo in the bell drawer while navigating elsewhere.
7. Open **Virtual Machine Lab** to inspect provider discovery and exact-preview safety; no hypervisor is installed by WimForge.
8. Open **Embedded terminal** and read its elevation/bounds notice. Start a session only if you intend to run an administrator command.

Overview 會將主路線寫清楚：**揀來源 → 自訂 → 審閱 → 執行 → 驗證**。先揀來源，等自動檢查同 Catalog 配對完成；要改 image／mount／output 先展開進階路徑。Updates／Drivers 先睇自動 ISO 配對，再去 Review & Run 對指令。一路可以用鍵盤分頁同其他頁面，後台儲存、通知同 history 工作唔會凍住成個 shell。

The current checked-in image gallery is on [Screenshots](Screenshots).

## 香港粵語導覽

開 app 先會見到工程起始頁，可以建立、開啟、匯入或繼續最近工程；呢啲 Git／bundle 工作會喺後台做。入到工程後，左邊 rail 有十二個功能頁；上面分頁列好似 browser 咁，可以開啟、排位、關閉、改名同改字體樣式。`Ctrl+W` 關而家呢個分頁，`Ctrl+Tab` / `Ctrl+Shift+Tab` 前後切換，Tab 後亦可以用左右鍵、Enter 同 Space；工程內 `.wimforge/tabs/.git` 會記錄每次變更。所有確認、通知、Catalog、復原同 history 都係 app 內非封鎖畫面，唔會將 UI 或 job queue 卡死。

---

[← Getting Started](Getting-Started) · [Projects and Sources →](Projects-and-Sources)
