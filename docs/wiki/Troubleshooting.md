# Troubleshooting

Start with the exact in-app status, persistent notification, operation output, and project path. WimForge orchestrates Git, DISM, `oscdimg`, WinGet, npm, installers, and other external programs; an application-level validation success does not guarantee that those tools or a particular Windows image will accept the request.

## Startup and project problems

| Symptom | Checks |
| --- | --- |
| Application does not start from a portable folder | Extract the complete archive; keep the executable beside deployed Qt/MSVC files and the `platforms`/QML directories. Do not run only a copied `WimForge.exe`. |
| Project will not create | Use a valid writable destination, ensure Git is on `PATH`, and review any existing destination contents. |
| Project will not open | Select a directory containing `project.json`, not the JSON file itself. Use the import destination field for `.json` or `.wimforge` input. |
| A project mutation reports a Git error | Confirm Git is available and the project `.git` directory is writable. Do not delete lock files until no WimForge/Git process owns them. |
| Complete-save import is refused | The importer rejects unsafe paths, links/reparse points, collisions, incomplete Git declarations, unsupported format flags, and an existing destination unless overwrite was explicitly requested. |

Use `WimForgeCli.exe --project <folder> project validate --execution --json` when deterministic validation output is easier to archive. See [CLI](CLI).

## Source inspection

### A raw ISO shows no edition list

Direct inspection needs a readable WIM/ESD/SWM or extracted media directory. Mount or extract the ISO, then point **Image path** at `sources\install.wim`, `install.esd`, or the first `install.swm`. The reviewed servicing plan may still use the original ISO as its source.

### DISM inspection or servicing fails

Check:

- that the image path exists and the selected one-based index is valid;
- target architecture/build applicability;
- administrator state for operations that require it;
- free space for source clone, mount, scratch, staging, and output;
- antivirus or another process locking the image/mount; and
- the complete DISM output, not only the last line.

Do not retry against the pristine source by disabling clone protection. Repair the work path or rebuild it from the preserved source.

### Output or mount validation fails

Source, working image/media, mount, scratch, and output must not overlap. Use separate canonical paths and remove junction/reparse indirection. A mount directory must meet DISM's expectations and should not be reused while Windows still records it as mounted.

## Plan and job problems

### The plan is empty

Open a project, supply a valid source/image, add at least one actionable configuration item, then select **Rebuild plan**. The status line reports the first planning error.

### An operation was skipped

It may have been explicitly marked optional/skipped, or one of its dependencies failed. Restore it from the operation menu if appropriate, then fix the prerequisite and rebuild the plan.

### Cancellation appears incomplete

Cancellation stops scheduling and requests termination of active child processes. An external program may already have written state. Inspect operation logs, `dism /Get-MountedWimInfo`, the project journal, and output hashes before rerunning.

### WimForge starts with a recovery banner

An unfinished `.wimforge/job-journal.json` was detected. Review the recorded run ID, mount path, operations, and last states. **Safe unmount** is available only when no job is active and WimForge is elevated; it discards the exact journal-recorded mount and leaves the original journal intact if DISM fails. See [Safety and Recovery](Safety-and-Recovery).

## Studio-specific problems

### Group Policy Studio is empty

The catalog is built from an installed or selected PolicyDefinitions store. Confirm ADMX files exist and matching ADML language resources are readable. Search can use text or validated regular expressions. Missing translation text is kept visibly empty rather than invented. See [Group Policy Studio](Group-Policy-Studio).

### OpenCode setup fails

WimForge first looks for an existing `opencode` and live-verifies `opencode --version`. Otherwise it needs npm, or WinGet to install exact package ID `OpenJS.NodeJS.LTS`, before running `npm install -g opencode-ai@latest`. Review the notification for the exact failed stage. Managed environments may intentionally block global npm or WinGet changes; the rest of WimForge remains usable.

### A package profile will not stage

Review profile validation, network mode, dependencies, required offline files, SHA-256 values, Authenticode publisher expectations, executable-only command fields, and structured argument arrays. Required missing/mismatched payloads fail closed. See [Package Studio](Package-Studio).

### Unattended XML is rejected

Fix editor validation first, then validate the exported XML in Windows SIM against the exact target image/catalog and architecture. Do not copy settings blindly between setup passes or architectures. See [Unattended Studio](Unattended-Studio).

### WinForge actions are marked unsupported

Select and detect a published runtime. A typed action is supported only when the runtime contract declares the matching capability and invocation. A legacy runtime may support only page deep-links. See [WinForge Bridge](WinForge-Bridge).

## Where state and logs live

- Project configuration: `<project>\project.json`
- Contextual action journal: `<project>\.wimforge\action-history.jsonl`
- Job journal: `<project>\.wimforge\job-journal.json`
- Per-run output: `<project>\.wimforge\logs\<run-id>\`
- Notification store: the path shown in **Settings**, normally under Qt's per-user application-local data location

Preserve journals/logs before cleanup when reporting a reproducible issue. Remove secrets, private keys, credentials, product keys, usernames, and proprietary payload details from reports and screenshots.

## Reporting an issue

Include:

1. WimForge version and whether installer/portable/source build was used.
2. Windows version, target image build/index/architecture, and relevant external-tool versions.
3. Reproduction steps from a non-secret test project.
4. Expected and actual behavior.
5. Sanitized error text, operation kind/ID, and relevant log excerpt.
6. Whether the issue reproduces with `--demo` or the CLI.

Do not attach Windows images, proprietary installers, credentials, or a secret-bearing `.wimforge` bundle to a public issue.

---

[← Settings](Settings) · [Architecture and Data Layout →](Architecture-and-Data-Layout)
