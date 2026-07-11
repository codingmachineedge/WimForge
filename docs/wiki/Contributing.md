# Contributing

Focused issues and pull requests are welcome. WimForge handles destructive image, registry, setup, and installer workflows, so a contribution must preserve its review and recovery contracts—not merely make the happy path appear to work.

## Before starting

1. Search the [issues](https://github.com/codingmachineedge/WimForge/issues) and current documentation.
2. Keep one change focused enough to review and test.
3. Identify the affected trust boundary: project state, external process, mount/media write, package payload, answer file, history, notification store, or bundle import/export.
4. For a user-visible feature, define its validation, failure, cancellation, history, CLI, accessibility, and documentation behavior before implementation.

Do not place Windows images, proprietary installers, credentials, product keys, private organization data, or secret-bearing bundles in the repository or an issue.

## Build and test

Development requires Visual Studio 2022 C++ tools, CMake 3.24+, Qt 6.8 for MSVC 2022 x64, Git, and PowerShell. Use the Visual Studio or Ninja commands in [Building and Releases](Building-and-Releases).

At minimum, configure with `BUILD_TESTING=ON`, build the affected configuration, and run:

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

For a single-config Ninja build, omit `-C Debug`. A green test run is evidence only for the behavior those tests cover. Add or extend tests for every new non-UI rule and regression.

The checked-in suite covers project/history, notifications, unattended profiles, GPO catalog/compiler, Package Studio, action history, project bundles, servicing plans, WinForge Bridge, and the CLI. Desktop QML interaction, responsive rendering, contrast, and full screenshot automation remain areas that require additional coverage; do not claim those gates exist until they are committed and running.

## Safety invariants

Contributions must retain these principles:

- original offline media is not the default write target;
- output, mount, source, and work paths cannot overlap unsafely;
- external commands use executable plus argument arrays, not evaluated shell strings;
- destructive actions and CLI apply require explicit confirmation;
- mounted-image/media writes remain dependency ordered;
- final files are not presented as complete before validation/promotion;
- project, action, and notification histories keep their distinct durability contracts;
- undo records compensation and never promises to reverse escaped side effects;
- archive/staging paths reject traversal, reserved names, collisions, links, and reparse escapes;
- required payload hashes/signers fail closed; and
- a failed safety check remains an error rather than becoming a warning for convenience.

If a design intentionally changes an invariant, document the new threat model and add direct tests before asking for review.

## C++ and core changes

- Keep business logic in `src/core` where it can be tested without QML.
- Prefer small typed structures and explicit validation results.
- Preserve `QT_NO_CAST_FROM_ASCII`, `QT_NO_CAST_TO_ASCII`, warning levels, and C++20 portability within the supported MSVC/Qt toolchain.
- Inject process, filesystem, clock, or environment dependencies when deterministic tests need control.
- Return and surface the exact actionable error; do not hide external-tool failures behind a generic success state.

## QML and accessibility changes

- Keep controls inside the Material shell and use the application's theme-aware semantic colors.
- Give every actionable/icon-only control an accessible name and tooltip.
- Keep keyboard access and text-editor shortcut exceptions working.
- Test the minimum 1080×700 window and wider desktop layouts in light/dark and English/Cantonese/bilingual modes.
- Ensure dense rows can wrap, stack, elide, or scroll without hiding actions.
- Do not advertise aggregate search, localization, density, or accessibility behavior broader than the implemented/tested surface.

## Documentation changes

`docs/wiki` is the checked-in Wiki source. Every completed task must keep the README and canonical Wiki synchronized with the delivered behavior, even when the primary change is code or process:

1. Update the relevant Wiki page and any detailed `docs/*.md` reference.
2. Update README claims, comparisons, and completion evidence.
3. Use live links for GitHub resources and Wiki-style page links inside `docs/wiki`.
4. Keep limitations and external validation requirements next to feature claims.
5. Run canonical Wiki validation and verify the applicable live synchronization workflow after the task reaches `main`.

The active **Synchronize GitHub Wiki** workflow publishes `docs/wiki` changes from `main`, validates the canonical page set, checks the published Wiki, and performs scheduled drift checks. A completed task verifies the applicable run instead of assuming that a successful Git push also proves Wiki equivalence.

`docs/wiki` 係已簽入版本嘅 Wiki 來源。每個完成嘅 task 都要同步 README 同所有需要改嘅 canonical Wiki 頁面，唔可以程式已經改咗但文件仲講舊行為。推上 `main` 之後，要睇埋相應嘅 **Synchronize GitHub Wiki** workflow，同核對 live Wiki 冇 drift。

## Screenshot changes

Before every task handoff, run the committed full capture process described in [Screenshots](Screenshots). Regenerate and visually inspect Project Start, all twelve application routes, and both documentation-site viewport captures as one commit-consistent set. Captures must use neutral project paths, the documented viewport/theme contract, descriptive alt text, true PNG encoding, and no secrets or usernames. Never land a partial refresh.

每個 task 交付之前，都要跟 [Screenshots](Screenshots) 重拍兼逐張核對工程起始頁、十二個 app 頁面，同 desktop/mobile 兩張文件網站圖。全套要用同一個 commit、指定 viewport/theme、真正 PNG 編碼同中性資料；唔可以只更新部分截圖就交付。

## Repository completion gate / Repo 完成關卡

The task is complete only after its README/Wiki claims and all fifteen tracked screenshots are current, relevant tests pass, its commit message is bilingual, the work is pushed and landed on `main`, and the exact resulting SHA has passed every applicable Release, Wiki, Pages, and container workflow.

Task 只可以喺 README/Wiki 同十五張已追蹤截圖全部更新、相關測試通過、commit message 用雙語、工作已 push 同落到 `main`，而且呢個 SHA 嘅 Release、Wiki、Pages 同 container workflow 全部適用關卡都過咗，先算完成。

## Pull-request checklist

- [ ] Scope and user-visible behavior are explained.
- [ ] Failure, cancellation, recovery, and threat boundaries are covered.
- [ ] Core/CLI tests pass and new rules have tests.
- [ ] Relevant desktop routes were exercised at minimum and normal sizes.
- [ ] `README.md` and every affected canonical `docs/wiki` page match the delivered behavior; Wiki validation passes.
- [ ] All thirteen application captures and both site viewport screenshots were refreshed and visually inspected; none is missing, stale, cropped, mislabeled, or private.
- [ ] The commit message is bilingual, the work was pushed and landed on `main`, and the resulting workflows were verified.
- [ ] No generated build output, payloads, credentials, or personal paths were added.
- [ ] Packaging/release behavior was tested when dependencies or deployment changed.

GitHub releases are produced from `main`; contributors should not create release tags for ordinary pull requests. Release artifacts are not currently code-signed.

---

[← Building and Releases](Building-and-Releases) · [Home →](Home)
