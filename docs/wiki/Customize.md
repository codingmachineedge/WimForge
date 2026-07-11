# Customize

The **Customize** page records image intent in eight sections. Each successful edit is saved and committed before it can become a servicing operation. Use [Review and Run](Review-and-Run) to inspect the resulting executable, arguments, dependencies, and risk flags.

## Sections

| Section | Current desktop surface |
| --- | --- |
| **Updates** | Queue CAB/MSU paths, language packs, Features on Demand, and enablement packages. The servicing plan determines dependency order. |
| **Drivers** | Add one INF, a driver directory, or request import of the current host's third-party drivers. |
| **Features** | Toggle known Windows feature identities including .NET Framework 3.5, WSL, Virtual Machine Platform, Hyper-V, Containers, Telnet Client, SMB 1.0, and Print to PDF. |
| **Apps** | Queue provisioned Appx/MSIX package-name removals. Signed app provisioning is represented in the project/core model; provide required bundles and dependencies yourself. |
| **Components** | Queue low-level component identities or scheduled-task paths for removal. |
| **Settings** | Toggle the built-in registry/policy recipes listed below. |
| **Unattended** | Add an existing answer-file path or open Unattended Studio to build one. |
| **Post-setup** | Queue reviewed files, installers/scripts, REG files, or `$OEM$` content for later staging. |

Text-list sections support explicit add and remove actions. Passive navigation does not change the project.

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

WimForge does not acquire general driver, update, or application payloads for this page. The operator is responsible for source, architecture, applicability, licensing, redistribution, integrity, and signer review. Package Studio adds provider-aware first-logon profiles and trust metadata, but it also does not bypass vendor authentication, subscriptions, hardware requirements, or terms. See [Package Studio](Package-Studio).

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

---

[← Projects and Sources](Projects-and-Sources) · [Image Servicing →](Image-Servicing)
