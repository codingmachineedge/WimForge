# WimForge full product audit — 2026-07-10

This audit is the implementation contract for the UI, feature-completeness,
documentation, screenshot, GitHub Wiki, and virtual-machine work that follows.
An item is not complete merely because a nearby core type exists: the desktop
control, command plan, persistence path, safety behavior, test, and user
documentation must agree.

## Audit method and baseline

- Inspected every QML route, shared overlay, public `AppController` property and
  invokable, servicing-plan generator, job-engine transition, CLI surface,
  checked-in screenshot, README claim, and local/live wiki page.
- Rendered the Debug desktop build against the populated demo project. The
  shipped 1,483×951 layout already clips bilingual navigation, and Source &
  editions visibly overlaps long labels/fields before the minimum-size test.
- Built with Visual Studio/Qt 6.8 and ran CTest. The existing 11 core/CLI tests
  pass, but there is no JobEngine, AppController, QML contract, accessibility,
  interaction, or responsive-render test target.
- Verified `codingmachineedge/WimForge` is the active GitHub repository and the
  live Wiki exists. Its 14 pages matched `docs/wiki`, but it has no sidebar,
  footer, complete application tour, or automatic sync.
- Audited the current `main` state after `b62c20e`. The repository deliberately
  keeps automatic OpenCode setup at startup; the implementation still needs a
  bounded, testable process lifecycle and documentation of the host change.

Status values used below:

- **Open** — verified missing, broken, misleading, or untested behavior.
- **Partial** — a core primitive exists, but the promised end-to-end workflow
  is incomplete.
- **External gate** — WimForge can implement the integration, but completion of
  a final artifact also needs an external asset, credential, vendor contract,
  or test environment.
- **Closed** — current evidence and a named test prove the requirement.

## UI and accessibility requirements

| ID | Status | Requirement | Authoritative evidence / completion gate |
| --- | --- | --- | --- |
| UI-001 | Open | Replace the fixed, overfull navigation rail with an adaptive and vertically scrollable rail. Show full bilingual labels at desktop width and a named/tooltipped compact state at narrow width. | `qml/Main.qml:107-187`; shipped screenshots visibly elide Group Policy, Unattended, and WinForge. Render at 1,080×700 and density 1.25 with every route reachable. |
| UI-002 | Open | Remove literal ampersand mnemonic corruption in buttons, tabs, and navigation (`Source_editions`, `Review_run`, and similar). | `qml/Main.qml:29,35-36`, `qml/pages/HistoryPage.qml:53`; screenshot/text assertion must show literal `&`. |
| UI-003 | Open | Make all ten pages responsive. Dense horizontal toolbars/splits must stack or scroll; long labels and paths must wrap or elide without overlapping controls. | Page-specific findings below; render matrix must report no out-of-viewport controls or Qt layout warnings. |
| UI-004 | Open | Make WinForge Bridge's entire runtime/recipe/staging pane reachable at the minimum height. | `qml/pages/WinForgeBridgePage.qml:217-335`; verify by keyboard and scroll at 1,080×700. |
| UI-005 | Open | Introduce theme-aware semantic colors with AA contrast for normal text and non-text state indicators. | Fixed colors in `Main.qml`, Plan, GPO, Package, Unattended, History, and WinForge are about 2.52:1 on `#211F26`. Automated contrast gate plus dark-theme screenshots. |
| UI-006 | Open | Make interface density observable across shared fonts, paddings, spacing, and dimensions without transform clipping. | `SettingsPage.qml:49` currently only persists. Render all pages at 0.8, 1.0, and 1.25. |
| UI-007 | Open | Honor reduced motion in every transition/behavior. | Unconditional animations in `NotificationCenter.qml:26`, `ContextHistoryPanel.qml:47-58`, `Snackbar.qml:79`; motion-off transition time must be zero. |
| UI-008 | Open | Preserve native text undo and context menus. Global history shortcuts/right-click must only run when an editor did not handle the input. | `qml/Main.qml:78-100`; interaction tests for TextField Ctrl+Z, selection, copy/paste menu, and background contextual history. |
| UI-009 | Open | Add accessible names to icon-only controls and text equivalents for color-only severity/failure states. | Main job/bell, remove, overflow, A/B/copy, notification, snackbar, and context-close controls; UI Automation enumeration must expose a meaningful name. |
| UI-010 | Open | Make destructive confirmation/recovery sheets modal, focus-contained, height-bounded, and scrollable. | `qml/Main.qml:399-485`; keyboard and minimum-size overlay tests. |
| UI-011 | Open | Localize notification, snackbar, context-history, validation, and consequential controller feedback in English, zh-HK, and bilingual modes. | Shared components currently use English-only `qsTr` without translation catalogs; screenshot/string inventory in all modes. |
| UI-012 | Open | Add a real aggregate search palette for pages, settings, commands, features, packages, GPOs, and relevant project data. | `Main.qml:73,208-215` routes every query to GPO despite the broader claim in `AppController.cpp:1617-1623`. Search-routing tests for each result type. |

### Page-specific responsive defects

| Page | Verified defect to close |
| --- | --- |
| Overview | Header and safety/current-job rows do not have a narrow fallback; several bilingual status labels do not wrap. |
| Source & editions | The media card's GroupBox/title, source controls, and Inspect action overlap in the rendered default layout; the source row and long clone label need a stacked state. |
| Customize | Action buttons can crush the entry field; remove buttons overlay delegate text; feature grids switch to two columns too early for bilingual text. |
| Group Policy Studio | Search/action rows are overfull, fixed catalog/detail panes never stack, and the narrow detail pane cannot reliably fit all state actions. |
| Unattended Studio | Header actions and generic editor rows need a narrow stack; the settings table hardcodes 115/260/210 pixel columns. |
| Package Studio | Search/preset/stage toolbar is crowded; card name/provider/badge/version allocation is not bounded. |
| WinForge Bridge | The long right-hand staging pane is not scrollable and becomes unreachable at the supported minimum size. |
| Review & run | Header/footer do not adapt; failed and queued state are distinguished primarily by color. |
| History & recovery | Fixed 335-pixel comparison pane and event action row overflow at narrow width; recovery/notifications view is fixed two-column; checked-in capture contains black/cropped strips. |
| Settings | Long repository paths do not wrap; several long switches and the optional-AI row need responsive layout. |

## Broken or incomplete repo-owned feature contracts

| ID | Priority | Status | Requirement | Current contradiction / completion gate |
| --- | --- | --- | --- | --- |
| FEAT-001 | P0 | Open | Make automatic OpenCode setup deterministic and observable: use one queued state machine for startup/helper requests, add bounded verify/npm/WinGet timeouts, explicit absent/installing/verifying/ready/failed states, idempotent retry, and clear documentation of host Node/npm changes. | `installOpenCodeThen()` recursively polls every 500 ms while busy and its install subprocesses are not uniformly bounded. Process-injection tests must cover startup, concurrent helper clicks, hangs, failures, restart, and retry. |
| FEAT-002 | P0 | Open | Give skipped operations honest dependency semantics and an incomplete/failure result when downstream work cannot safely run. | `JobEngine.cpp:223-300` treats skipped dependencies like failures but ignores skipped states in final success. Add `job_engine_tests`. |
| FEAT-003 | P0 | Open | Export exactly the reviewed operation graph: omit/mark skips, topologically order dependencies, reject cycles/unsafe reorders, and preserve safety metadata. | `ServicingPlan::exportPowerShell()` emits every operation in list order. Add script-export graph tests. |
| FEAT-004 | P0 | Open | Make every Customize card map to the operation it names. Separate updates/packages, Appx remove/provision, component packages, scheduled tasks, applied answer files, literal post-setup commands, and staged payloads. | `CustomizePage.qml` currently funnels multiple unlike inputs into the wrong lists; add AppController-to-plan integration tests per card. |
| FEAT-005 | P0 | Open | Implement all eight Customize settings as typed, reversible registry/GPO/unattended operations with build-aware compatibility notes. | `setSetting()` only stores booleans; `ServicingPlan` never consumes them. Plans before/after every toggle must differ correctly. |
| FEAT-006 | P0 | Open | Expose dependencies, checkpoint requirements, write scope, parallel eligibility, and skip consequences in Review & run. | `operationPlan()` omits those fields although the page promises them. Model/QML test required. |
| FEAT-007 | P0 | Open | Wire runtime settings: density, motion, worker/thread ceiling, scratch reserve, journal policy, and destructive checkpoints. | Several Settings controls only persist. Each needs an observable runtime test with injected storage/process/journal adapters. |
| FEAT-008 | P1 | Open | Keep active servicing jobs isolated from whichever project the user edits next. | JobEngine signals overwrite the controller's single `m_plan`; project-A run/project-B edit integration test required. |
| FEAT-009 | P1 | Open | Make auto-export configurable from a fresh project and guarantee `.wimforge` paths always contain atomic complete bundles, never temporary plain JSON. | The path is disabled until enable, while validation rejects enable without a path; export ownership is split across ProjectConfig/AppController. |
| FEAT-010 | P1 | Open | Synchronize source-hash and parallelism settings with the active project's effective values and explicit global defaults. | UI globals and project options currently diverge. Reopen/switch-project tests required. |
| FEAT-011 | P1 | Partial | Turn Package Studio into the documented profile editor for every provider, dependencies, commands, verification, retry/network modes, payloads, hashes, signers, scope, architecture, and metadata. | Desktop is presently a search/toggle/import/export surface. Full author/edit/round-trip QML tests required. |
| FEAT-012 | P1 | Partial | Let GPO Studio choose a PolicyDefinitions store and discover/select all installed ADML languages; lazy-load only when requested. | Desktop hardcodes the host store and `en-US`/`zh-HK`, and eagerly loads at startup. Fixture-store switching tests required. |
| FEAT-013 | P1 | Open | Fix QML/controller symbol and transaction failures: `unattendFiles` typo, no-project editing, and in-memory mutation before failed persistence. | Add metaobject-backed QML contract tests and save-failure rollback tests. |
| FEAT-014 | P2 | Open | Use the active history branch in contextual history and calculate a real state A→B diff. | `Main.qml:312` hardcodes `main`; History concatenates inverse/forward patches instead of comparing reconstructed states. |
| FEAT-015 | P2 | Open | Clear/reload edition metadata on project/source changes and isolate notification repositories per project/bundle. | Controller-global metadata and retained notification store can bleed across projects. Switch/import/export tests required. |
| FEAT-016 | P2 | Partial | Complete desktop coverage for existing core fields: arbitrary features/capabilities, typed registry, Appx provisioning, compression/split size, cleanup/ResetBase, dry-run, unattended removal/placement, and WinForge registry/copy actions. | Core/CLI support exists but the desktop has no complete controls. |

## Previously disclosed gaps now promoted to implementation work

The user requested implementation of all unimplemented features, so the items
that were previously only disclosed in the comparison/limitations text are now
tracked work rather than permanent exclusions.

| ID | Status | Required deliverable |
| --- | --- | --- |
| EXP-001 | Open | Integrated Microsoft Update discovery, download, SHA-256 content-addressed cache, metadata, offline import, applicability evaluation, retry/resume, and reviewed servicing integration. |
| EXP-002 | Open | Windows services inventory/editor with start-mode/account/dependency safety, offline registry plan generation, compatibility warnings, reversible project state, UI, CLI, and tests. |
| EXP-003 | Open | Scheduled-task inventory/editor with safe disable/enable/delete/import behavior, file/registry consistency checks, reversible project state, UI, CLI, and tests. |
| EXP-004 | Open | Extensible build/edition-aware component and tweak compatibility database with dependency/conflict rules, provenance/versioning, local updates, explicit expert override, UI, CLI, and tests. |
| EXP-005 | Open | NTLite preset importer with schema detection, mapping report, warnings for unsupported/vendor-only constructs, round-trip-safe original attachment, CLI/UI, and fixture tests. |
| EXP-006 | Open | Remote-machine management/servicing using explicit authenticated Windows remoting, capability discovery, reviewed structured operations, logs, cancellation, and safety boundaries. |
| EXP-007 | Open | Host/image inventory refresh for Windows editions, features, capabilities, Appx, components, services, tasks, drivers, and updates. |
| EXP-008 | External gate | Authenticode-ready release signing and verification pipeline. Repository automation can be completed; publication of signed binaries requires a protected code-signing identity supplied by the project owner. |
| EXP-009 | Open | VM-based validation workflow with boot/install milestones, evidence capture, and result history. This is implemented through VM Lab below. |

Vendor-owned compatibility data, third-party licenses, credentials, Windows
activation, and hardware coverage may not be fabricated. WimForge must provide
the complete integration and an honest external-gate status when those inputs
are absent.

## VMware and VirtualBox VM Lab requirement

| ID | Status | Requirement |
| --- | --- | --- |
| VM-001 | Open | Detect installed VMware Workstation/Player and VirtualBox providers and report executable/capability/version evidence. |
| VM-002 | Open | Create/register a VM from the current output ISO with reviewed name, directory, guest type, firmware/secure-boot/TPM compatibility, CPU, memory, disk, network, and unattended-boot settings. |
| VM-003 | Open | Maintain provider-neutral inventory and live state; open the provider console without hiding provider errors. |
| VM-004 | Open | Implement explicit start, graceful shutdown, power off, pause, resume, reset, save-state, unregister, and delete flows with confirmation for destructive actions. |
| VM-005 | Open | Attach/detach ISO and storage/network devices, edit supported settings while powered off, and refresh configuration from the provider. |
| VM-006 | Open | Create, list, restore, and delete snapshots with provider capability checks and clear destructive consequences. |
| VM-007 | Open | Record validation runs, boot/install checkpoints, logs, screenshots/evidence references, pass/fail notes, and the exact image/hash used. |
| VM-008 | Open | Expose the complete manager in QML and CLI, persist only portable provider-neutral metadata, use structured arguments (never shell command strings), and add fake-provider unit/integration tests. |

## Documentation, screenshots, and Wiki requirements

| ID | Status | Requirement |
| --- | --- | --- |
| DOC-001 | Open | Replace the README's local wiki link with the live Wiki and make every repository-view link valid. |
| DOC-002 | Open | Capture a standardized, neutral-path screenshot set for all routes after fixes. README gets a concise representative tour; Wiki gets the full gallery. |
| DOC-003 | Open | Add live Wiki `_Sidebar.md` and `_Footer.md`, Application Tour, Projects & Sources, Customize, Review & run, Settings, Troubleshooting, Architecture/Data Layout, and Contributing pages. |
| DOC-004 | Open | Add safe main-repo-to-Wiki synchronization with drift verification. The Wiki keeps its own commit/push history. |
| DOC-005 | Open | Update every feature, comparison, safety, CLI, and known-limitations claim as each audit ID closes. Do not advertise planned work as implemented. |
| DOC-006 | Open | Remove username-bearing paths, inconsistent DPI, black capture strips, clipped text, and generic alt text from screenshots. |

## Required automated gates before this audit can close

1. Existing core/CLI tests remain green.
2. New JobEngine and reviewed-script graph tests cover skip, dependency,
   cancellation, checkpoint, and final-result semantics.
3. AppController workflow tests use isolated QSettings, project, notification,
   storage, process, and clock adapters.
4. QML contract tests reject unknown `app.*` properties/invokables/signals.
5. Qt Quick interaction tests exercise every visible control and prove failed
   persistence never leaves the UI in an uncommitted state.
6. Responsive render tests cover every route at default/minimum size, light and
   dark themes, three language modes, and density 0.8/1.0/1.25.
7. Accessibility enumeration finds names/state text for all actionable and
   severity controls; contrast checks meet AA targets.
8. VM/update/service/task/compatibility/remote integrations pass unit tests
   against fakes plus opt-in smoke tests when the real provider is installed.
9. README assets, local wiki source, and live Wiki are link-checked and in sync.
10. Release CI runs tests, QML lint/contract checks, screenshot smoke tests,
    packaging verification, and optional signing verification.

This document stays open until every row is either **Closed** with cited
evidence or explicitly marked **External gate** with the repository-side
integration complete.
