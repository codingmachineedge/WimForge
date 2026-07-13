# Projects and Sources

A WimForge project is a normal directory containing declarative configuration and local Git history. Selecting a source or customization changes project intent; it does not mount or service an image until a plan has been reviewed and confirmed.

## Create, open, and import

Use **New project** to choose a project name and directory. WimForge creates `project.json`, initializes the project repository, and creates the action-history journal under `.wimforge`.

The project sheet accepts three forms:

- an existing project directory containing `project.json`;
- a portable JSON configuration, imported into a destination directory; or
- a complete `.wimforge` bundle, validated and restored into a destination directory.

JSON is configuration interchange only. A [Project Bundle](Project-Bundles) carries the complete project tree—including hardened nested workspace-tab history—and the notification repository, including Git objects and undo history.

Creating, opening, importing, and exporting a project run away from the UI thread. The project rail shows background status/progress while bundle and Git work completes, and conflicting project actions are disabled until that transition finishes.

建立、開啟、匯入同匯出工程會離開 UI 主執行緒做。Bundle 或 Git 仲處理緊時，工程 rail 會顯示後台狀態／進度；未完成之前，會暫時停用互相衝突嘅工程操作。

Successful output-affecting changes use the canonical project save path: write `project.json`, create a project commit, then append the corresponding contextual action. These mutations are serialized in the background so later edits cannot overtake an earlier commit. If the project/tab save fails, later saves pause and **Retry save** appears; if only the secondary action-history append fails, the already-safe project commit is retained and WimForge raises a persistent warning.

會影響輸出嘅變更會先寫 `project.json`、建立工程 commit，再追加對應 contextual action。呢啲 mutation 會喺後台順序處理，之後嘅修改唔可以爬過前一個 commit。工程／分頁儲存失敗時，後續儲存會暫停並顯示 **再試儲存**；如果只係第二步 action-history 追加失敗，已安全完成嘅工程 commit 仍然會保留，WimForge 亦會出持續警告。

## Supported source forms

The **Source & editions** page accepts:

- an ISO file;
- an extracted Windows media directory;
- a WIM or ESD image; or
- the first part of a split SWM set.

Use **Choose and inspect ISO / image…** or the media-folder picker instead of typing paths. Selecting, dropping, or finishing a changed source path immediately inventories it; there is no separate inspect step to remember. The source-file and extracted-media dialogs are non-modal, so choosing a path does not freeze unrelated work. For a raw ISO, WimForge mounts the file read-only, discovers `sources\install.wim`, `install.esd`, or `install.swm`, runs detailed DISM inventory, and confirms dismount. The project stores only the stable internal relative path—not the temporary drive letter—so the servicing plan can extract the ISO into its project-owned media tree later.

請用 **揀 ISO／映像並自動檢查……** 或 media-folder picker，唔好靠手打路徑。揀、拖放，或者完成修改來源路徑之後就會即時做 inventory，唔使再記住撳多次 Inspect。原始 ISO 會唯讀掛載、找出 `sources\install.*`、做詳細 DISM inventory，再確認 dismount；工程只會記穩定相對路徑，唔會記臨時光碟機字母。

Inspection records edition names, target architecture, the full image version, and build. WimForge turns those fields into an automatic source profile such as `Windows 11 26100 x64`, then starts a Microsoft Update Catalog search for matching updates. Customize's Drivers section reuses the profile with a driver-specific query. You may refine the query in the non-modal results panel, but the normal workflow does not require hand-written search text. A search result is still not proof of edition/build applicability.

檢查會記低 edition 名、目標架構、完整映像版本同 build。WimForge 會將佢哋組成例如 `Windows 11 26100 x64` 嘅自動來源設定檔，再搜尋 Microsoft Update Catalog 更新；Customize 嘅 Drivers 會用同一份設定檔加 driver 條件。非 modal 結果 panel 仍然可以畀你收窄搜尋，但一般流程唔使自己寫搜尋字句。搜尋中咗亦唔代表已證明啱嗰個 edition／build。

## Source, image, mount, and output paths

The first-use view keeps the selected source and edition visible. Open **Show advanced paths** only when you need to change image, mount, output, format, or ISO-label details; working-image, mount-directory, and output-file values each have their own non-modal picker and purpose-specific accessibility name. The complete page scrolls vertically at 900×640, so the advanced output controls remain reachable. These fields have different responsibilities:

第一次使用時只會先突出已揀來源同 edition。要改 image、mount、output、格式或者 ISO label 先開 **顯示進階路徑**；工作映像、mount 資料夾同 output 檔各自有非 modal picker 同講清楚用途嘅無障礙名稱。900×640 時成頁可以直向捲動，下面嘅進階輸出操作仍然搵得到。各欄位用途如下：

| Field | Meaning |
| --- | --- |
| **Source path** | The original ISO, media directory, or image supplied by the operator |
| **Image path** | The WIM/ESD/SWM that DISM addresses; raw ISO sources retain a stable internal `sources/install.*` mapping instead |
| **Mount path** | An empty directory used for an offline mount |
| **Output path** | The final WIM, ESD, SWM, or ISO destination |

Keep **Clone source before editing** enabled. Offline planning then creates project-owned image/media workspaces rather than using the original as the default write target. ISO and media sources are cloned even when low-level in-place behavior is requested.

Validation rejects paths that overlap source, image, mount, working media, or output boundaries. Do not work around that check with junctions or reparse points; trust-boundary staging rejects them where traversal could escape the expected root.

## Select an edition

After inspection, choose the target edition from the discovered names or enter its one-based image index. WimForge stores the discovered edition names and bilingual inventory summary in the project options, so reopening the project restores the same inventory without inventing a placeholder edition. Inspection clamps an older selection to the returned edition count; loading imported or restored history applies the same in-memory safety clamp before planning. Edition names and available indexes vary by source, so do not assume an index copied from another ISO still identifies the same edition.

檢查完成之後，可以由已發現版本名揀目標，亦可以輸入由 1 開始嘅映像索引。WimForge 會將版本名同雙語 inventory 摘要儲存喺工程 options，重開工程就會還原同一份清單，唔會作一個假版本出嚟。如果舊選擇超出今次版本數量，檢查時會夾返入有效範圍；匯入或者還原歷史之後，載入時亦會先做同一個記憶體安全夾限，先至建立計劃。唔同來源嘅版本名同索引可以完全唔同，唔好假設另一隻 ISO 嘅同一個索引仍然係同一版本。

## Choose an output

The desktop offers WIM, ESD, SWM, and ISO output. ISO creation also needs the Windows ADK Deployment Tools program `oscdimg`. The volume-label field is limited to 32 characters in the desktop.

WimForge builds output into project-owned work paths and promotes validated final files rather than presenting a partial file as complete. Output planning, split-image behavior, media staging, and ISO ordering are described in [Image Servicing](Image-Servicing).

## Before customization

Before adding payloads:

1. Record the source origin and SHA-256.
2. Inspect the exact image and architecture.
3. Keep source, mount, scratch, and output on storage with sufficient free space.
4. Preserve a pristine source outside the project work tree.
5. Decide whether the final artifact is an image or complete bootable media.

Then continue to [Customize](Customize). Before execution, review [Safety and Recovery](Safety-and-Recovery).

## 香港粵語快速版

工程起始頁有四條清晰路：建新工程、開現有 `project.json` 資料夾、匯入 `.json` / `.wimforge`，或開最近工程；呢啲工作會喺後台做，rail 會報進度。Source 頁揀完來源就自動檢查，記低架構／版本／build，再自動搜尋 Catalog。Image、Mount 同 Output 收喺進階路徑入面；輸出／掛載同來源重疊仍然會被拒絕。版本清單同雙語摘要會跟工程保存，超出範圍嘅舊索引會先夾返有效值。預設保持 clone source，正式輸出前跟住 Customize、Review、Run 同 Validate。

---

[← Application Tour](Application-Tour) · [Customize →](Customize)
