# Application Tour

WimForge presents one Material desktop shell around ten workflow pages. The pages edit a declarative project; image-changing commands appear only after the project is converted into a reviewable servicing plan.

For a safe tour that does not require production media, start the populated demo project:

```powershell
.\WimForge.exe --demo --language bilingual --page overview
```

The accepted language values are `en`, `zh-HK`, and `bilingual`. Accepted page IDs are `overview`, `source`, `customize`, `gpo`, `unattended`, `packages`, `winforge`, `plan`, `history`, and `settings`.

## Global shell

The navigation rail remains available throughout the desktop workflow. At narrower desktop widths it becomes a compact icon rail; each control retains a name and tooltip. The header provides:

- a search field; in the current desktop implementation, submitting a query opens Group Policy Studio and searches its catalog;
- a job indicator that opens **Review & run**;
- a notification bell with unread count and the recoverable notification drawer;
- the current project control, which opens the project/import sheet; and
- an interrupted-run banner when recovery state is detected.

The project summary at the bottom of the rail shows the active project, Git status, and job progress. Informational sheets, snackbars, the notification drawer, and contextual history are in-app surfaces rather than native blocking dialogs.

## The ten pages

| Route | What to do there | Detailed guide |
| --- | --- | --- |
| **Overview** | See the current project, operation/history counts, running jobs, build flow, safety reminders, and current job progress. | [Getting Started](Getting-Started) |
| **Source & editions** | Select and inspect media, choose the working image/index, keep cloning enabled, and select an output. | [Projects and Sources](Projects-and-Sources) |
| **Customize** | Queue update, driver, feature, Appx, component, setting, answer-file, and post-setup intent. | [Customize](Customize) |
| **Group Policy Studio** | Load installed ADMX/ADML definitions, search them, edit schema-backed values, and commit registry policy state. | [Group Policy Studio](Group-Policy-Studio) |
| **Unattended Studio** | Build or import a portable profile and export Windows answer-file XML. | [Unattended Studio](Unattended-Studio) |
| **Package Studio** | Select first-logon software, validate provider/trust metadata, and stage a resumable bundle. | [Package Studio](Package-Studio) |
| **WinForge Bridge** | Review typed actions, detect a runtime contract, and stage a verified OEM payload. | [WinForge Bridge](WinForge-Bridge) |
| **Review & run** | Inspect commands, dependencies, risk markers, elevation, checkpoints, and execution state. | [Review and Run](Review-and-Run) |
| **History & recovery** | Selectively undo/redo project intent, inspect commits/diffs, and review notification history. | [History Time Machine](History-Time-Machine) |
| **Settings** | Choose language/theme, job limits, failsafes, project automation, and inspect notification storage. | [Settings](Settings) |

## Keyboard and pointer actions

| Gesture | Behavior |
| --- | --- |
| `Ctrl+Z` | Undo in the active page context, unless focus is in a text editor |
| `Ctrl+Shift+Z` | Open the active-page/global contextual history manager |
| Right-click outside a text editor | Open that contextual manager at the pointer |
| `Ctrl+Enter` | Request confirmation for the current reviewed plan |

Undo appends compensating history; it does not rewrite the past or reverse external bytes already committed by DISM or an installer. See [History Time Machine](History-Time-Machine) and [Safety and Recovery](Safety-and-Recovery).

## Suggested first tour

1. Open the demo on **Overview** and inspect the project/job metrics.
2. Visit **Source & editions** to see the separate source, image, mount, and output fields.
3. Add or remove one reversible item in **Customize**.
4. Open **Review & run** and inspect the resulting command and dependency change without running it.
5. Use `Ctrl+Z`, then inspect the new action in **History & recovery**.
6. Create a test notification from **Settings** and exercise read, dismiss, restore, delete, and undo in the bell drawer.

The current checked-in image gallery is on [Screenshots](Screenshots).

---

[← Getting Started](Getting-Started) · [Projects and Sources →](Projects-and-Sources)
