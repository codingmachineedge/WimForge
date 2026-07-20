# Review and Run

**Review & run** is the boundary between declarative project intent and external image operations. It shows the plan WimForge built; nothing on this page makes a plan safe merely by displaying it. The operator must still verify applicability, paths, tools, payloads, and consequences.

## Build the plan

Choose **Rebuild plan** after changing the source or configuration. The planner validates the project and produces operations containing:

- a stable operation ID and kind;
- an executable and structured argument list;
- human-readable title and description;
- dependency IDs;
- working directory and preview command;
- administrator, destructive, reboot, and checkpoint flags; and
- whether the operation writes the mounted image/media workspace or may prepare in parallel.

An empty plan usually means no project is open or the project has no actionable source/configuration. A status message reports the first planning error when validation fails. A successfully generated plan is **ready for review**; it is not reviewed, approved, or safe to run merely because planning completed or the page was opened. Overview therefore keeps **Review** and **Run** as separate workflow steps.

空 plan 通常代表未開工程，或者來源／設定未有可執行內容；驗證失敗會顯示第一個 planning error。Plan 成功產生只係 **準備好俾你檢查**，唔代表已審閱、已批准或者已經安全；單單開咗呢頁亦唔算 review。Overview 因此會將 **審閱** 同 **執行** 分開做兩步。

## Read each operation card

| Marker or field | Meaning |
| --- | --- |
| **Admin** | The external operation requires elevation |
| **Destructive** | The operation can remove or commit state and deserves explicit review |
| **Reboot** | The resulting workflow may require a restart |
| Command preview | The executable/arguments that the structured operation represents |
| Status | Queued, running, succeeded, failed, skipped, or cancelled |

The command preview is for review and copying. Core operations retain executable and argument fields rather than becoming a shell-evaluated command string.

## Reorder and skip

The operation menu can move a card earlier or later. The chosen visual order is saved in project configuration. Execution still waits for declared dependencies, so moving a dependent card above its prerequisite does not make the dependency disappear.

Only these operation kinds can be toggled as optional from the desktop: driver, package, feature, capability, Appx, component, registry, unattended, post-setup, and cleanup. Safety and image-structure operations cannot be skipped. Skip/restore state is committed to the project and survives a plan rebuild.

Skipping an operation can produce an unusable or internally inconsistent result even when the UI allows the operation kind to be optional. Review downstream dependencies and test the result.

## Concurrency and ordering

The desktop accepts 1–16 parallel jobs. The job engine starts only operations whose dependencies are terminal. Independent preparation/verification may overlap; an operation that is not marked parallel waits for all active work, and writes to the mounted image are serialized against one another.

A failed dependency causes a queued dependent operation to be skipped. The journal records operation state, dependencies, run identity, paths, and progress so an interrupted run can be reviewed after restart.

## Checkpoints and confirmation

Operation cards expose checkpoint-required metadata for review. **Save future checkpoint-policy preference (not enforced yet)** persists intent only; the current run path does not create a Git checkpoint from it. Create and verify an appropriate backup or checkpoint yourself before destructive work. After completing the review, use the separate **Run reviewed plan** action. Its confirmation sheet reports the operation count, concurrency limit, and number of destructive operations before the job engine starts; automatic validation never presses or substitutes for that action.

Operation card 會顯示需要檢查點嘅審閱標記。**儲存日後檢查點 policy 偏好（而家未執行）** 只會保存意向，而家執行流程唔會因此自動建立 Git 檢查點；做破壞性工作之前，請自行建立兼核實合適嘅 backup 或檢查點。逐項檢查完先用獨立 **執行已檢查計劃** 動作；確認 sheet 會先列操作數量、並行上限同破壞性操作數量，之後先啟動 Job Engine。自動 validation 唔會代你撳呢個掣。

A project must be open, the plan must be nonempty, and no other run may be active. Destructive CLI apply also requires `--yes`; see [Command-Line Interface](CLI).

## Export a review script

**Export script** writes a PowerShell representation of the current project/plan for peer review or controlled automation. Treat it as executable code:

- inspect it after every project change;
- keep source/payload paths and hashes with the review record;
- do not add secrets; and
- do not assume the exported script extends WimForge's transaction boundary.

The application itself launches structured executables and arguments. A separately edited exported script is the operator's responsibility.

The Review & Run page has an outer vertical scroll surface. At 900×640, the plan summary, operation list, export controls, and final run action remain reachable instead of being clipped below the viewport.

Review & Run 成頁可以直向捲動；900×640 時 plan 摘要、operation 清單、匯出控制同最後 run 動作仍然搵得到，唔會被 viewport 截走。

## Cancellation and recovery

**Cancel safely** requests cancellation of active child processes and prevents new work from being scheduled. Cancellation is not proof that an external tool made no change. Review the operation output, mount state, job journal, and final artifact before retrying.

After an interruption, WimForge can rebuild the plan, undo the latest configuration change, or—only under guarded elevated conditions—discard the exact mount recorded in the journal. Read [Safety and Recovery](Safety-and-Recovery) before using recovery actions.

## Pre-run checklist

- [ ] Source and payload hashes are recorded.
- [ ] Source, mount, work, and output paths do not overlap.
- [ ] Every executable and argument is expected.
- [ ] All admin, destructive, and reboot markers are understood.
- [ ] Driver/update/package architecture and target-build applicability are verified.
- [ ] Answer XML was validated against the exact image/catalog.
- [ ] A complete [Project Bundle](Project-Bundles) exists.
- [ ] A disposable-VM installation test is planned.

---

[← Image Servicing](Image-Servicing) · [History Time Machine →](History-Time-Machine)
