# Settings

The **Settings** page combines application preferences with project-scoped automation. Preference changes are persisted through Qt's platform settings store; automatic import/export values live in the open project's `project.json` and therefore create project history.

## Language

Choose one of three presentation modes:

- English;
- Hong Kong Cantonese; or
- English and Cantonese side by side.

The shell and pages use the selected mode immediately. External-tool output, source metadata, paths, and some technical identifiers are not translated.

## Appearance

| Setting | Current behavior |
| --- | --- |
| **Theme** | Follow the operating-system color scheme, force Material light, or force Material dark |
| **Interface density** | Persists a value from 0.8 to 1.25 and scales both Project Start and the loaded project shell as one logical surface.<br>會保存 0.8 至 1.25 嘅數值，並將 Project Start 同已載入工程嘅 shell 各自當成一個完整介面縮放。 |
| **Motion** | Persists the motion preference and is passed to shared overlay components; it does not change external process behavior |

Theme-aware colors belong to the UI only. They do not change output or servicing configuration.

In the wide layout, Settings categories expose PageTab/selected semantics and a visible keyboard-focus ring. Material color-scheme swatches behave as named radio buttons with checked and focus states. At narrower sizes—including 900×640—the category selector becomes a compact combo and each settings panel scrolls vertically, so the last controls remain reachable.

闊 layout 入面，Settings 類別會向輔助技術報告 PageTab／已選狀態，亦有清楚鍵盤 focus 外框。Material 配色 swatch 係有名稱、checked 同 focus 狀態嘅 radio button。窄到包括 900×640 時，類別會變 compact combo，每個 settings panel 都可以直向捲動，最下面啲設定仍然撳得到。

## Jobs and concurrency

| Setting | Range | Current behavior |
| --- | --- | --- |
| **Maximum parallel jobs** | 1–16 | Passed to the job engine; independent eligible work may overlap while mounted-image writes remain serialized.<br>會傳畀 job engine；符合條件嘅獨立工序可以重疊執行，但寫入已掛載 image 仍然會逐個處理。 |
| **CPU thread ceiling** | 1–logical CPU count | Persisted and exposed in Settings for future scheduling; the current job-engine start path does not pass it to external tools.<br>會保存並顯示喺 Settings，留畀之後嘅排程支援使用；而家 job engine 啟動外部工具時未會傳入呢個偏好。 |
| **Scratch-space reserve** | 5–500 GB | Persisted and exposed in Settings as a future planning target; the current planner does not pause jobs or enforce this value as a free-space gate.<br>會保存並顯示喺 Settings，做之後嘅規劃目標；而家嘅 planner 未會按呢個數值暫停工序或限制可用空間。 |

Set concurrency conservatively when source, mount, scratch, and output share a slow disk. A larger value does not bypass dependencies or allow concurrent writes to the same mounted image.

## Failsafes

- **Crash journal / 死機日誌** persists a future journal-policy preference. The current job engine maintains its project journal independently as part of normal execution; do not treat this setting as permission to remove recovery state.<br>呢個掣會儲存留畀之後 journal policy 用嘅偏好；而家 job engine 會獨立維護工程 journal，唔好將呢個設定當成可以刪走復原狀態嘅許可。
- **Hash source before apply / 套用前 hash 來源** updates the open project's payload-verification option. The plan computes SHA-256 before writes; it rejects a mismatch when an expected hash is configured, and still fails if an input cannot be read.<br>Plan 會喺寫入前計算 SHA-256；有設定預期 hash 時，唔一致就會拒絕繼續，而輸入讀唔到亦會失敗。
- **Future checkpoint policy / 日後檢查點 policy** persists checkpoint intent and is mirrored on **Review & run**, but the current run path does not create a Git checkpoint from this preference. Create and verify the required backup or checkpoint yourself before executing destructive work.<br>呢個偏好會儲存檢查點意向，亦會同步去 **Review & run**；不過而家執行流程唔會因為呢個偏好自動建立 Git 檢查點。做破壞性工作之前，請自行建立兼核實需要嘅 backup 或檢查點。
- **Recoverable tombstones** is displayed as always enabled: notification deletion remains a recoverable event rather than immediate history erasure.

Disabling a configurable safety preference transfers risk to the operator; it does not expand what external tools can safely do. See [Safety and Recovery](Safety-and-Recovery).

## Automatic import and export

These controls require an open project:

- **Watch project config for external changes** watches `project.json` and reloads reviewed external changes through the controller's project path.
- **Export after every commit** writes a complete `.wimforge` project bundle to the chosen destination. Choose a `.wimforge` path before enabling the toggle; typed paths with another extension are rejected instead of silently skipping export. Use the non-modal save-file picker to choose the path; the field keeps an accessibility name if a saved path replaces the visible label.

**每次 commit 後匯出** 會將完整 `.wimforge` 工程 bundle 寫去指定位置。啟用個掣之前要先揀 `.wimforge` 路徑；手打其他副檔名會直接被拒絕，唔會靜雞雞跳過匯出。請用旁邊嘅非 modal 儲存檔案 picker 揀路徑；就算已儲路徑取代咗畫面 label，欄位仍然會保留無障礙名稱。

Automatic export uses the same atomic [Project Bundle](Project-Bundles) format as **Complete save**. It carries the project repository, nested workspace-tab repository, and notification repository; it does not embed external source media, mount/output trees, or other payloads merely referenced by path.

自動匯出同 **完整儲存** 一樣，會用原子 [Project Bundle](Project-Bundles) 格式，帶埋工程 repository、內嵌 workspace-tab repository 同通知 repository；只係用路徑引用嘅外部來源媒體、mount／output tree 或其他 payload 就唔會塞入 bundle。

Avoid pointing automatic export inside a source, mount, or output tree. Keep the destination writable and under backup. If an external editor and WimForge change the same configuration path, inspect Git/action history rather than assuming last-writer-wins is safe.

## Notification center

Settings displays the active notification repository path and can create a test notification. The store contains notification state, an append-only event file, and its own local Git repository. Reading, marking unread, dismissing, tombstone-deleting, restoring, and undoing are committed independently of the current project.

See [Notification Center](Notification-Center) for lifecycle details and [Architecture and Data Layout](Architecture-and-Data-Layout) for storage boundaries.

## Diagnostics and logging

WimForge attempts to start structured JSON Lines diagnostics in the application-local `logs/wimforge.jsonl` file. Logging is best effort: if initialization fails, WimForge reports the error to standard error and continues without claiming an active log. When logging starts, Settings shows the path and can ask Windows to open either the current file or its folder. Records include a UTC timestamp, sequence, session, severity, category, event, process/thread identity, source location, message, and structured data.

Logging covers GUI and CLI startup/shutdown, Qt messages, controller mutations and outcomes, notifications, source inspection, host-driver export, servicing activity, scheduler state, every Job Engine child-process launch, redacted arguments and output, errors, completion, cancellation, and dependency blocks. The active file rotates at 5 MiB and retains five archives.

Secret-like JSON keys and textual password, bearer-token, API-key, credential, cookie, product-key, URL-password, GitHub-token, and OpenAI-key patterns are replaced with `[REDACTED]`, including separate sensitive command-line option values. Redaction is defense in depth, not permission to put secrets in project names, paths, scripts, or third-party output. Review diagnostic files before sharing them.

## 香港粵語重點

Settings 可以揀 English、香港粵語或雙語，同時控制 theme、density、motion、concurrency 同 safety 選項。類別同配色可以用鍵盤操作，有清楚 focus／已選狀態；900×640 時 panel 可以捲到底。自動匯出位置有非 modal save-file picker，唔使手打完整路徑。Diagnostics 會盡力啟動 JSONL logger；如果初始化失敗，WimForge 會喺 standard error 報錯再繼續，唔會扮有 active log。成功啟動後會顯示 `wimforge.jsonl` 路徑，亦可以叫 Windows 開 log 或所在資料夾；紀錄包括 GUI/CLI 開關、project mutation、source inspection、驅動匯出、scheduler 同每個子程式嘅生命週期/輸出。Logger 會遮蔽常見 secret pattern，但呢個只係防線；分享前仍然要自己逐行檢查，唔好將密碼放入路徑、工程名或第三方輸出。

---

[← Project Bundles](Project-Bundles) · [Troubleshooting →](Troubleshooting)
