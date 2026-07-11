# Screenshots / 截圖

The canonical gallery covers Project Start plus all twelve desktop routes. Every image comes from the
same build, a route-specific non-production demo or intentionally idle safe state, bilingual English/Hong Kong Cantonese UI mode, and a 1,440×900
PNG client area with one logical pixel mapped to one output pixel. Physical DPI metadata is not part of the capture contract. A route-specific public fixture keeps paths,
Git history, settings, and notification state deterministic without exposing a
real Windows image, private project, account name, or secret.

標準畫廊包括工程起始頁同全部十二個桌面功能頁。每幅圖都由同一個 build、按頁面安排嘅無害 demo 或刻意 idle 安全狀態、English / 香港粵語雙語模式，同已正規化嘅 1,440×900 PNG client area 產生；一個 logical pixel 對一個輸出 pixel，檔案入面嘅實體 DPI metadata 唔屬於截圖合約。公開 fixture 只用中性路徑同虛構資料，唔會露出真實映像、帳戶、秘密或私人工程。

## Complete application gallery

### Project Start / 工程起始頁

![WimForge Project Start showing bilingual create, open, import, and recent-project actions](https://raw.githubusercontent.com/codingmachineedge/WimForge/main/docs/screenshots/project-start.png)

呢個係 app 開啟後嘅第一個畫面：可以建立新工程、開啟現有資料夾、匯入 `.json` / `.wimforge`，或由最近清單繼續。

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

### Virtual Machine Lab

![Virtual Machine Lab in its safe empty state, showing unavailable provider discovery, inventory filters, workflow tabs, and no selected machine](https://raw.githubusercontent.com/codingmachineedge/WimForge/main/docs/screenshots/virtual-machine-lab.png)

### Review and run

![Review and run showing the reviewed operation list, deterministic verification steps, concurrency control, checkpoint setting, and explicit run button](https://raw.githubusercontent.com/codingmachineedge/WimForge/main/docs/screenshots/review-run.png)

### History and recovery

![History Time Machine showing append-only actions, branch and bookmark controls, guarded undo and restore actions, and the A/B comparison pane](https://raw.githubusercontent.com/codingmachineedge/WimForge/main/docs/screenshots/history.png)

### Settings

![Settings showing language, Material theme, interface density, motion, concurrency, scratch-space reserve, and failsafe controls](https://raw.githubusercontent.com/codingmachineedge/WimForge/main/docs/screenshots/settings.png)

### Embedded terminal

![Embedded Terminal ready state showing a trusted administrator shell selector, neutral working directory, empty bounded ConPTY viewport, command input, and inactive stop controls](https://raw.githubusercontent.com/codingmachineedge/WimForge/main/docs/screenshots/embedded-terminal.png)

## Reproduce the gallery

Build the restricted documentation harness, then run the committed capture
script from the repository root. The harness uses an `asInvoker` manifest so a
13-route automation run does not display 13 UAC prompts, and it refuses to run
unless `--screenshot` is present with exactly one of `--demo` or
`--project-start`. Normal and release builds still embed
`requireAdministrator`.

```powershell
cmake -S . -B build-capture -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64 `
  -DWIMFORGE_DOCUMENTATION_CAPTURE=ON -DBUILD_TESTING=OFF
cmake --build build-capture --config Debug --target WimForge --parallel
./scripts/capture-documentation-screenshots.ps1 -Language bilingual -Theme dark
```

The script launches Project Start with `--project-start` and every workspace
route with `--demo --language bilingual --page <id>`, gives every route a clean
public fixture and notification ledger, waits for the window to settle, and uses
WimForge's `--screenshot` option to save the normalized Qt Quick client area. It
fails if a route exits unsuccessfully, omits its image, or produces anything
other than the complete thirteen-file, true-PNG, 1,440×900 set.

| Page | Page ID | Image |
| --- | --- | --- |
| Project Start / 工程起始頁 | startup capture | `project-start.png` |
| Overview | `overview` | `overview.png` |
| Source and editions | `source` | `source.png` |
| Customize | `customize` | `customize.png` |
| Group Policy Studio | `gpo` | `group-policy.png` |
| Unattended Studio | `unattended` | `unattended.png` |
| Package Studio | `packages` | `package-studio.png` |
| WinForge Bridge | `winforge` | `winforge-bridge.png` |
| Virtual Machine Lab | `vmlab` | `virtual-machine-lab.png` |
| Review and run | `plan` | `review-run.png` |
| History and recovery | `history` | `history.png` |
| Settings | `settings` | `settings.png` |
| Embedded terminal | `terminal` | `embedded-terminal.png` |

## Documentation-site viewport captures / 文件網站 viewport 截圖

Refresh the application gallery first because the documentation-site hero embeds `overview.png`. Build the current documentation with `python -m mkdocs build --strict`, serve it locally with `python -m mkdocs serve --dev-addr 127.0.0.1:8000`, then capture `http://127.0.0.1:8000/WimForge/` as viewport-only, explicitly PNG-encoded images at device scale factor 1:

| View | Browser metrics | Image |
| --- | --- | --- |
| Desktop | 1,280×720, non-mobile | `site-home-desktop.png` |
| Mobile | 390×844, mobile emulation | `site-home-mobile.png` |

![WimForge documentation home at the desktop viewport](https://raw.githubusercontent.com/codingmachineedge/WimForge/main/docs/screenshots/site-home-desktop.png)

![WimForge documentation home at the mobile viewport](https://raw.githubusercontent.com/codingmachineedge/WimForge/main/docs/screenshots/site-home-mobile.png)

先重拍 app 畫廊，因為文件網站 hero 會直接用 `overview.png`。之後用本機嚴格 MkDocs build 開 `http://127.0.0.1:8000/WimForge/`，desktop 用 1,280×720／非 mobile，mobile 用 390×844／mobile emulation，device scale factor 固定做 1，而且一定要明確輸出真正 PNG，唔可以用 JPEG 資料扮 `.png`。

After both capture stages, enforce the complete tracked-file contract:

```powershell
./scripts/verify-documentation-screenshots.ps1
```

呢個 verifier 會拒絕缺漏、多餘檔案、假 PNG，或者任何一張尺寸唔啱嘅截圖；通過後仍然要逐張做視覺核對。

## Capture contract

- Use one application commit, theme, bilingual language mode, viewport, and output dimensions for the
  primary set.
- Keep source, project, output, profile, notification, and application-data
  paths under the public screenshot fixture.
- Never include credentials, private keys, customer names, private hostnames,
  organization data, or proprietary payload names.
- Capture the client area directly; do not include desktop clutter, another
  window, cursor effects, capture gutters, or post-capture rescaling.
- Before every completed-task handoff, regenerate and visually verify the complete
  thirteen-route gallery and both documentation-site viewports as one commit-consistent
  set; never land only a partial screenshot refresh.
- Use descriptive alt text that names the route and the meaningful visible
  state.

Secondary dark-theme, single-language English/Cantonese, density, minimum-viewport, and
overlay matrices are useful visual-regression work, but they should remain
clearly named QA sets rather than replacing this stable primary tour.

截圖合約重點：主畫廊一定要用 `--language bilingual`，全套保持同 commit、theme 同 viewport。路徑、通知、工程名同 payload 都要用公開 fixture；唔好放客戶名、私人 hostname、credential、product key 或專有 payload 入鏡。每個 task 完成之前都要重拍兼逐張核對工程起始頁、全部十二個功能頁，同 desktop/mobile 兩張文件網站圖，唔可以只更新部分頁面。

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
