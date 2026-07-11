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

`docs/wiki` is the checked-in Wiki source. When behavior changes:

1. Update the relevant Wiki page and any detailed `docs/*.md` reference.
2. Update README claims and comparisons if their status changed.
3. Use live links for GitHub resources and Wiki-style page links inside `docs/wiki`.
4. Keep limitations and external validation requirements next to feature claims.
5. Verify the local Wiki and live Wiki remain content-equivalent.

Safe automatic Wiki synchronization is a desired repository gate, but it must not be described as active until the corresponding workflow and drift check are committed and verified. Until then, publishing the Wiki is a separate reviewed repository push.

## Screenshot changes

Use `WimForge.exe --demo --language <mode> --page <id>` and follow [Screenshots](Screenshots). Captures must use neutral project paths, one documented resolution/DPI/theme convention, descriptive alt text, and no secrets or usernames. Replace—not supplement—an obsolete screenshot when the page contract changes.

## Pull-request checklist

- [ ] Scope and user-visible behavior are explained.
- [ ] Failure, cancellation, recovery, and threat boundaries are covered.
- [ ] Core/CLI tests pass and new rules have tests.
- [ ] Relevant desktop routes were exercised at minimum and normal sizes.
- [ ] Documentation and current/planned status are accurate.
- [ ] No generated build output, payloads, credentials, or personal paths were added.
- [ ] Packaging/release behavior was tested when dependencies or deployment changed.

GitHub releases are produced from `main`; contributors should not create release tags for ordinary pull requests. Release artifacts are not currently code-signed.

---

[← Building and Releases](Building-and-Releases) · [Home →](Home)
