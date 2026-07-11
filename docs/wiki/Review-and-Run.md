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

An empty plan usually means no project is open or the project has no actionable source/configuration. A status message reports the first planning error when validation fails.

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

Keep **Create recovery checkpoint before destructive steps** enabled. The confirmation sheet reports the operation count, concurrency limit, and number of destructive operations before **Run reviewed plan** calls the job engine.

A project must be open, the plan must be nonempty, and no other run may be active. Destructive CLI apply also requires `--yes`; see [Command-Line Interface](CLI).

## Export a review script

**Export script** writes a PowerShell representation of the current project/plan for peer review or controlled automation. Treat it as executable code:

- inspect it after every project change;
- keep source/payload paths and hashes with the review record;
- do not add secrets; and
- do not assume the exported script extends WimForge's transaction boundary.

The application itself launches structured executables and arguments. A separately edited exported script is the operator's responsibility.

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
