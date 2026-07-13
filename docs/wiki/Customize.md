# Customize

The **Customize** page records image intent in eight sections. Each successful edit is saved and committed before it can become a servicing operation. Use [Review and Run](Review-and-Run) to inspect the resulting executable, arguments, dependencies, and risk flags.

## Sections

| Section | Current desktop surface |
| --- | --- |
| **Updates** | Queue CAB/MSU paths, language packs, Features on Demand, and enablement packages. The servicing plan applies them in the stored, reviewed order; it does not infer SSU/LCU applicability or dependencies. |
| **Drivers** | Add one INF, a driver directory, or request import of the current host's third-party drivers. |
| **Features** | Set known Windows feature identities to Enable, Disable, or Unchanged, and queue exact capability/FOD identities for Add, Remove, or Unchanged. |
| **Apps** | Queue provisioned Appx/MSIX package-name removals separately from signed `.appx`, `.appxbundle`, `.msix`, and `.msixbundle` provisioning files. |
| **Components** | Queue low-level component-package removals or typed offline scheduled-task Enable, Disable, and guarded Delete actions. |
| **Settings** | Toggle the built-in registry/policy recipes listed below. |
| **Unattended** | Add an existing answer-file path or open Unattended Studio to build one. |
| **Post-setup** | Queue reviewed files, installers/scripts, REG files, or `$OEM$` content for later staging. |

Text-list sections support explicit add and remove actions. Passive navigation does not change the project.

The page scrolls as one responsive surface: payload actions use five, two, or one columns as space allows, so a narrow window does not clip the lower controls. Section tabs expose selected state to assistive technology and support Left/Right arrow navigation.

成個頁面係響應式捲動畫面：payload 操作會按空間排五欄、兩欄或者一欄，窄視窗唔會截走下面啲掣。Section 分頁會向輔助技術報告已選狀態，亦可以用左／右方向鍵切換。

## Typed feature, capability, app, and task changes

Optional features are tri-state. **Unchanged** removes the project override; it is not another spelling of Disable. Capability/FOD Add and Remove lists are also mutually exclusive, and clearing an identity restores Unchanged. Every successful mutation is saved through `ProjectConfig` and committed to the project's local Git repository.

The Apps surface keeps package-name removal separate from file-based provisioning. The file picker accepts existing `.appx`, `.appxbundle`, `.msix`, and `.msixbundle` files. WimForge validates the path and extension when queueing; DISM performs the package-signature and image-applicability checks during servicing. Queue signed framework dependencies before the main bundle, then verify the exact order in **Review & run**. WimForge does not provide a Store browser or dependency resolver.

Scheduled tasks use paths relative to `Windows\System32\Tasks`. Enable and Disable atomically edit the offline task XML. Delete removes the task definition, so the desktop and controller both require an explicit compatibility override; the servicing plan also requires a checkpoint. The editor does not claim to inventory every task or provide build-specific compatibility advice.

## 類型化功能、能力、App 同排程工作變更

選用功能係三態：**啟用**、**停用**同**不變**。揀「不變」係清除工程覆寫，唔係另一種停用。Capability/FOD 嘅加入同移除亦唔可以同時存在；清除 identity 就會回復不變。每次成功修改都會經 `ProjectConfig` 儲存，再 commit 入工程自己嘅本機 Git repository。

Apps 畫面會分開「按套件名移除」同「按檔案預載」。File picker 只收現有 `.appx`、`.appxbundle`、`.msix` 同 `.msixbundle`；WimForge 排隊時會驗路徑同副檔名，而套件簽署同映像適用性就由 DISM 喺維護期間驗。已簽署 framework 依賴要排喺主 bundle 前面，之後去 **Review & run** 對清楚次序。WimForge 暫時冇 Store browser，亦冇依賴 resolver。

排程工作路徑係相對於 `Windows\System32\Tasks`。啟用同停用會原子修改離線工作 XML；刪除會移走工作定義，所以畫面同 controller 都一定要你明確確認相容性解鎖，servicing plan 亦會要求檢查點。呢個 editor 唔會扮識晒每個 Windows build 嘅工作清單同相容性。

## Built-in settings

The current desktop exposes these named setting recipes:

- reduce diagnostics and advertising telemetry;
- allow a local account during OOBE;
- show known file extensions;
- use the classic context menu;
- disable consumer application suggestions;
- enable Win32 long paths;
- prefer performance-oriented visual effects; and
- disable Recall by policy.

These switches are declarative recipe inputs, not a guarantee that every Windows edition/build implements the same registry or policy behavior. Inspect the generated plan and validate the installed result.

## Feature and component caution

Feature identities are Windows component names, not friendly compatibility advice. SMB 1.0 is explicitly labeled legacy/risky. Component removal is a low-level identifier workflow; WimForge does not ship a mature component-dependency or compatibility database comparable to long-established commercial tooling.

Before enabling, disabling, or removing an item:

1. Confirm the identity exists in the selected image/build.
2. Review dependencies and servicing output.
3. Keep the destructive-operation checkpoint enabled.
4. Boot-test the result and the recovery environment in a disposable VM.

## Payload responsibility

After source inspection records architecture, version, and build, WimForge automatically searches Microsoft Update Catalog for matching Updates. The Drivers section derives its own driver query from that same ISO profile. **View matches** opens a non-modal in-app results panel; its text field is an optional refinement, not a required “search for” step. A trusted Microsoft download is added straight to the reviewed update or driver queue—no external browser—and the search can be cancelled without closing the panel.

來源檢查記低架構、版本同 build 之後，WimForge 會自動搜尋 Microsoft Update Catalog 嘅 Updates。Drivers 亦會由同一份 ISO 設定檔自動組合 driver 搜尋。撳 **睇配對結果** 會開非 modal app 內結果 panel；入面個文字欄只係可選微調，唔係必做嘅「search for」步驟。由可信 Microsoft 主機下載嘅項目會直接加入更新／驅動審閱隊列，唔使開外部 browser；搜尋亦可以取消，唔使關 panel。

WimForge still does not acquire general application payloads for this page, resolve SSU/LCU applicability, or prove that a result fits the target image. The operator remains responsible for source, architecture, applicability, licensing, redistribution, integrity, and signer review. Package Studio adds provider-aware first-logon profiles and trust metadata, but it also does not bypass vendor authentication, subscriptions, hardware requirements, or terms. See [Package Studio](Package-Studio).

WimForge 仍然唔會喺呢頁下載一般應用程式 payload、解決 SSU／LCU 依賴，或者證明搜尋結果一定啱目標映像。操作員仍然要負責來源、架構、適用性、授權、再發佈權、完整性同簽署者審閱。Package Studio 有 provider-aware first-logon profile 同 trust metadata，但一樣唔會繞過供應商登入、訂閱、硬件要求或條款。

## Answer files and post-setup work

An answer-file path here becomes servicing input. Use [Unattended Studio](Unattended-Studio) for profile editing and XML export, then validate the XML in Windows SIM against the exact image/catalog.

Post-setup work crosses from offline image construction into code that runs during setup or first logon. Review every executable and argument, keep secrets out of project state, and test both connected and offline network conditions. For the structured, resumable software path, prefer [Package Studio](Package-Studio). For typed WinForge-family actions, use [WinForge Bridge](WinForge-Bridge).

## Undo and review

`Ctrl+Z` from this page targets the `config` history context. Selective undo is guarded: unrelated later edits can survive, while a later change to the same target path produces a conflict rather than being silently overwritten.

After customization:

1. Open **Review & run**.
2. Rebuild the plan.
3. Resolve all validation errors.
4. Review destructive/admin/reboot markers and exact commands.
5. Export a review script or complete `.wimforge` save before execution.

## 香港粵語重點

Updates 同 Drivers 唔再係空白清單：揀完 ISO 會按架構、版本同 build 自動搜尋 Catalog，亦可以用 picker 加 CAB/MSU、INF 或驅動資料夾；清單會顯示 KB、大小、provider、class 同 driver version。搜尋結果可以 app 內下載（只限可信 Microsoft 主機）再自動加入審閱隊列，唔使開 browser 或先手打 query；不過 WimForge 唔會估 SSU/LCU 依賴，亦唔會證明某個 update 啱目標 build。頁面窄時會自動改成一欄並可以捲動，section tabs 可用方向鍵。Features 係真正三態；Apps 分開套件名移除同已簽署 bundle file picker。所有項目都要去 Review & Run 對指令同 destructive marker 先執行。

---

[← Projects and Sources](Projects-and-Sources) · [Image Servicing →](Image-Servicing)
