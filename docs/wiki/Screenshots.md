# Screenshots

The canonical gallery covers all ten desktop routes. Every image comes from the
same build, populated non-production demo, English UI mode, 1,480×920 Qt Quick
client area, and 96-DPI PNG output. A route-specific public fixture keeps paths,
Git history, settings, and notification state deterministic without exposing a
real Windows image, private project, account name, or secret.

## Complete application gallery

### Overview

![WimForge Overview showing project metrics, the four-step build flow, safety rails, and current-job status](https://raw.githubusercontent.com/codingmachineedge/WimForge/main/docs/screenshots/overview.png)

### Source and editions

![Source and editions showing neutral ISO and working-image paths, clone-before-editing, edition selection, mount path, and output format](https://raw.githubusercontent.com/codingmachineedge/WimForge/main/docs/screenshots/source.png)

### Customize

![Customize showing the update and language-package queue, navigation across all configuration categories, and a neutral demo payload](https://raw.githubusercontent.com/codingmachineedge/WimForge/main/docs/screenshots/customize.png)

### Group Policy Studio

![Group Policy Studio showing the installed ADMX catalog, a selected Delivery Optimization policy, three-state draft controls, a schema-generated numeric editor, registry target, and Git-backed commit action](https://raw.githubusercontent.com/codingmachineedge/WimForge/main/docs/screenshots/group-policy.png)

### Unattended Studio

![Unattended Studio showing computer-name behavior, Microsoft-published installation keys, the generic answer-file editor, and setup-pass values](https://raw.githubusercontent.com/codingmachineedge/WimForge/main/docs/screenshots/unattended.png)

### Package Studio

![Package Studio showing the Full AI Development profile, provider-backed software cards, selection count, and staging controls](https://raw.githubusercontent.com/codingmachineedge/WimForge/main/docs/screenshots/package-studio.png)

### WinForge Bridge

![WinForge Bridge showing typed recipe actions, runtime contract detection, portable recipe controls, and verified ISO staging](https://raw.githubusercontent.com/codingmachineedge/WimForge/main/docs/screenshots/winforge-bridge.png)

### Review and run

![Review and run showing the reviewed operation list, deterministic verification steps, concurrency control, checkpoint setting, and explicit run button](https://raw.githubusercontent.com/codingmachineedge/WimForge/main/docs/screenshots/review-run.png)

### History and recovery

![History Time Machine showing append-only actions, branch and bookmark controls, guarded undo and restore actions, and the A/B comparison pane](https://raw.githubusercontent.com/codingmachineedge/WimForge/main/docs/screenshots/history.png)

### Settings

![Settings showing language, Material theme, interface density, motion, concurrency, scratch-space reserve, and failsafe controls](https://raw.githubusercontent.com/codingmachineedge/WimForge/main/docs/screenshots/settings.png)

## Reproduce the gallery

Build the Debug application, then run the committed capture script from the
repository root:

```powershell
cmake --build build --config Debug --target WimForge --parallel
./scripts/capture-documentation-screenshots.ps1
```

The script launches each route with `--demo --language en --page <id>`, gives
every route a clean public fixture and notification ledger, waits for the window
to settle, and uses WimForge's `--screenshot` option to save the normalized Qt
Quick client area. It fails if a route exits unsuccessfully, omits its image, or
produces dimensions inconsistent with the rest of the set.

| Page | Page ID | Image |
| --- | --- | --- |
| Overview | `overview` | `overview.png` |
| Source and editions | `source` | `source.png` |
| Customize | `customize` | `customize.png` |
| Group Policy Studio | `gpo` | `group-policy.png` |
| Unattended Studio | `unattended` | `unattended.png` |
| Package Studio | `packages` | `package-studio.png` |
| WinForge Bridge | `winforge` | `winforge-bridge.png` |
| Review and run | `plan` | `review-run.png` |
| History and recovery | `history` | `history.png` |
| Settings | `settings` | `settings.png` |

## Capture contract

- Use one application commit, theme, language mode, viewport, and DPI for the
  primary set.
- Keep source, project, output, profile, notification, and application-data
  paths under the public screenshot fixture.
- Never include credentials, private keys, customer names, private hostnames,
  organization data, or proprietary payload names.
- Capture the client area directly; do not include desktop clutter, another
  window, cursor effects, capture gutters, or post-capture rescaling.
- Regenerate every route when the shared shell, primary data fixture, capture
  normalization, or screenshot contract changes.
- Use descriptive alt text that names the route and the meaningful visible
  state.

Secondary dark-theme, Cantonese, bilingual, density, minimum-viewport, and
overlay matrices are useful visual-regression work, but they should remain
clearly named QA sets rather than replacing this stable primary tour.

## What a screenshot does not prove

A screenshot demonstrates layout and populated state at one instant. It does
not prove that a control is keyboard or screen-reader accessible, that every
theme/scale/language is responsive, that a servicing plan succeeded, that the
resulting image boots, or that an answer file passed Windows SIM validation.
Pair images with tests, logs, hashes, and disposable-VM evidence appropriate to
the claim.

See [Safety and Recovery](Safety-and-Recovery), [Troubleshooting](Troubleshooting),
and [Contributing](Contributing).

---

[← Application Tour](Application-Tour) · [Troubleshooting →](Troubleshooting)
