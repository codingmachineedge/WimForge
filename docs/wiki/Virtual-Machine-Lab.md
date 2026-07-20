# Virtual Machine Lab

Virtual Machine Lab is WimForge's reviewed manager for loading a Windows ISO
into an installed VMware or VirtualBox environment, controlling the machine,
and retaining structured smoke-test evidence. It is the eighth desktop route
and uses page ID `vmlab` (`vm-lab` is accepted as a startup alias).

![Virtual Machine Lab in its safe empty state, showing unavailable provider discovery, inventory filters, workflow tabs, and no selected machine](https://raw.githubusercontent.com/Ding-Ding-Projects/WimForge/main/docs/screenshots/virtual-machine-lab.png)

!!! important "Bring your own hypervisor"
    WimForge detects and drives a supported hypervisor that is already
    installed. It does **not** download, install, license, update, or silently
    enable VMware Workstation, VMware Player, VirtualBox, Hyper-V, firmware
    virtualization, or host networking. If no supported provider is installed,
    discovery reports that state and VM actions remain unavailable.

## Providers and discovery

Select **Refresh providers** to probe the host and rebuild the live inventory.
Automatic discovery recognizes these stable provider IDs:

| Provider | Provider ID | Command surfaces used |
| --- | --- | --- |
| Oracle VM VirtualBox | `virtualbox` | `VBoxManage.exe` and, when present, `VirtualBox.exe` |
| VMware Workstation Pro | `vmware-workstation` | `vmrun.exe`, `vmware.exe`, and, for creation, `vmware-vdiskmanager.exe` |
| VMware Workstation Player | `vmware-player` | `vmrun.exe`, `vmplayer.exe`, and, for creation, `vmware-vdiskmanager.exe` |

Discovery does not trust `PATH`. It derives protected Program Files roots from
the machine registry, canonicalizes each candidate, rejects executables outside
those roots, and rejects symbolic-link or junction traversal. A bounded,
structured process launch then asks the provider for its version. The provider
is marked available only when that probe succeeds.

Capabilities are evidence, not assumptions. Inventory, create, registration,
console, lifecycle, configuration, media, snapshots, deletion, TPM, and Secure
Boot controls are enabled only when the detected provider/version proves the
corresponding capability. The provider cards show the evidence and warnings
that led to the decision.

## Inventory and scope

The **Inventory** tab combines current provider inventory with WimForge's
catalog. Search and filter by provider or power state, then select a machine to
see its identity, configuration path, hardware, attached devices, warnings,
ownership, and allowed actions.

The inventory is exposed as an accessible list. Each row reports its machine
name, power state, selected state, and keyboard focus; Tab reaches the rows,
Enter or Space selects the focused machine, and the visible focus outline stays
distinct from selection. On compact layouts, selecting a row opens its detail
pane without removing the ability to return to inventory.

Inventory 係有無障礙語意嘅 list；每一行會報虛擬機名、電源狀態、已選
狀態同鍵盤 focus。Tab 可以去到清單項目，Enter 或 Space 揀 focus 緊嗰部
機，focus 外框亦會同已選狀態分開。Compact layout 揀完會開 details，但
仍然可以返去 inventory。

Provider identity is always a pair: provider ID plus provider-native machine
ID. Display names are not treated as unique. A refresh records complete live
state and a revision before a mutation can be reviewed. If inventory is
partial, stale, inaccessible, or disagrees with the catalog, state-changing
operations fail closed.

VM state is isolated by scope. With a project open, WimForge uses a
project-derived application-data scope; without one it uses the global scope.
Large VM disks and catalog state are not placed in the source repository.

## Create from an ISO or load an existing VM

The **Create / load** tab has two distinct ownership paths.

### Create a managed machine

1. Choose an available provider and a safe machine name.
2. Select an existing absolute `.iso` path.
3. Set the guest type, firmware, CPU, memory, disk, and network mode.
4. Enable Secure Boot, TPM, unattended boot, or boot-after-create only when the
   selected provider proves support.
5. Review the generated effects and exact provider commands, then execute that
   preview.

Use the non-modal ISO picker beside the installation path; choosing a file only
fills the draft. Loading an external machine likewise uses the `.vmx`/`.vbox`
picker. Hardware uses a separate `.vdi`/`.vmdk` disk picker and attached-media
ISO picker, while Validation has its own evidence-file picker. Their accessible
names state the exact purpose, so several generic browse controls are not
announced identically.

安裝路徑旁邊有非 modal ISO picker，揀檔只會填入 draft。載入外部虛擬機有
獨立 `.vmx`／`.vbox` picker；Hardware 另外有 `.vdi`／`.vmdk` 磁碟 picker
同掛載媒體 ISO picker，Validation 亦有自己嘅 evidence-file picker。每個掣
嘅無障礙名稱都會講清楚用途，唔會全部只讀成同一個「瀏覽」。

A managed machine must be created as a direct child of WimForge's configured
managed root. The target is reserved without following reparse points before a
provider runs. On success WimForge writes a durable ownership marker containing
the provider identity and a unique token. The catalog entry, marker, and
on-disk directory together establish managed ownership.

### Load or register an external machine

Loading an existing `.vbox` or `.vmx` configuration records **external**
ownership. WimForge does not move those files into its managed root and never
promotes an import to managed ownership.

- VirtualBox can register a reviewed `.vbox` file through its documented
  provider command when registration capability is available.
- VMware `vmrun` does not expose a sufficiently safe registration operation.
  WimForge therefore imports the `.vmx` into its own catalog without claiming
  provider registration; opening or starting the VMX remains explicit.

This distinction controls removal: external files remain owned by their user
or provider, not WimForge.

## Lifecycle

For a selected machine, the inventory exposes only state-valid actions:

- open the provider's graphical console;
- start normally or headless;
- request guest shutdown;
- power off, pause, resume, reset, or save state; and
- cancel the currently running provider task when the command supports it.

**Open console** intentionally opens the provider's graphical VM window. It is
not a command shell or an external terminal popup. All provider command output,
errors, exit status, and cancellation state remain in WimForge.

Graceful shutdown asks the guest/provider to cooperate. Power off and reset are
disruptive and can lose guest data. The review labels that risk before any
command runs. Configuration and media edits require a powered-off VM; WimForge
does not guess that a machine is safe merely because it is absent from a stale
running list.

## Hardware, media, and networking

The **Hardware** tab can review changes to CPU count, memory, BIOS/EFI firmware,
Secure Boot, and TPM where the provider proves support. It also shows the
provider-reported storage and network inventory.

Media and storage operations use explicit topology:

- bus: IDE, SATA, SCSI, or NVMe;
- controller index and optional provider-visible controller name;
- port and device number; and
- optical/disk type plus an absolute source path for attachment.

Detaching media removes the attachment; it does not delete the ISO or disk.
VMware VMX edits are hash-guarded atomic writes, and VirtualBox changes use
structured `VBoxManage` arguments. Both require refreshed live evidence.

Network adapters use an explicit one-based slot and one of `nat`, `bridged`,
`host-only`, `internal`, or `disconnected`. Bridged/host-only selections can
name the host interface. Attaching, editing, or disconnecting an adapter
requires the machine to be powered off.

## Snapshots

The **Snapshots** tab lists provider snapshots, takes a named snapshot, and
reviews restore or deletion. Restore and delete are destructive because they
can discard guest state or recovery points. Before either action, WimForge
refreshes snapshot inventory, binds the preview to its revision, and verifies
that the selected snapshot still exists. A renamed, deleted, or replaced
snapshot invalidates the preview.

Snapshots are useful recovery points, but they are not substitutes for pristine
installation media, exported project state, or independent backups.

## Exact review and confirmation

Every mutation is review-first. The review dialog shows:

- the provider and exact target identity;
- read-only, reversible, disruptive, or destructive risk;
- effects and warnings;
- executable, individual arguments, working directory, timeout, and detached
  status for every provider command; and
- a preview ID, catalog/live-state revision, and expiry.

A preview is immutable, expires after five minutes, and can execute only once.
Execution refuses a different preview ID, an expired preview, a changed catalog
revision, changed file hashes, or changed provider inventory. The manager holds
the catalog transaction through the provider mutation and catalog commit.

Destructive actions also require the exact token displayed by the fresh
preview, normally `DELETE <machine or snapshot name>`. The token is
case-sensitive and target-specific. A checkbox or generic **Yes** does not
replace it.

At the supported 900×640 minimum, VM Lab reduces tall inventory/detail panels,
uses compact one-column flow where needed, and keeps dense hardware, snapshot,
validation, and exact-preview content in bounded scroll views. Lower actions
remain reachable instead of being clipped below the window.

去到支援嘅最細 900×640，VM Lab 會縮短 inventory／details panel，需要時改
做 compact 一欄流程，hardware、snapshot、validation 同 exact preview 等
密集內容會放入有界 scroll view。下面啲操作仍然捲得到，唔會被視窗截走。

## Removal and ownership boundaries

The removal verbs deliberately mean different things:

| Action | Catalog | Provider registration | VM files |
| --- | --- | --- | --- |
| **Forget** an external entry | Removed | Unchanged | Unchanged |
| **Unregister** an external VirtualBox VM | Removed after reviewed provider action | Removed | Unchanged |
| **Delete** a managed VM | Removed after all guards pass | Removed where supported | Managed directory deleted |

Managed entries cannot be forgotten or unregistered in a way that strands the
only ownership record. External entries cannot use WimForge's managed-file
deletion path.

For managed deletion, WimForge requires all of the following: the exact
ownership token and marker, a canonical direct-child directory beneath the
managed root, no reparse traversal, a unique catalog path, complete provider
inventory, a powered-off machine, a matching reviewed revision, and the exact
destructive token. It retains no-follow handles across provider unregister,
refreshes the complete provider inventory again, revalidates identity, and only
then removes the retained directory. If any proof changes, files remain.

## Validation evidence

The **Validation** tab is project-scoped. It records which exact ISO, image,
provider, VM, configuration snapshot, and hashes were tested. Choose a profile,
start a run, record milestones and evidence, then finalize it as passed, failed,
or cancelled.

| Profile | Required milestones |
| --- | --- |
| Installation | installation boot, disk layout, installation complete |
| First boot | first boot, drivers, networking, smoke test |
| Upgrade | installation boot, installation complete, first boot, drivers, smoke test |
| Customization | customizations, first boot, smoke test |
| Full smoke | all installation, first-boot, driver, network, customization, and smoke-test gates |

Evidence can be a screenshot, log, report, or other file. Project-contained
evidence is hashed and recorded with its label and capture time. Updates use a
run revision so two writers cannot silently overwrite one another. A run cannot
be marked passed until every required milestone is reached, no milestone has
failed, and at least one evidence file has a SHA-256 hash. Failed and cancelled
runs require reviewer notes; final results are locked.

VM validation is an evidence ledger, not a claim that WimForge automatically
understands every guest screen. A human or external test harness still decides
what the observed boot/install result means.

## Command-line automation

The same provider adapter, preview, confirmation, catalog, ownership, and
evidence rules are available from `WimForgeCli.exe`. Start with:

```powershell
.\WimForgeCli.exe vm help
.\WimForgeCli.exe vm detect
.\WimForgeCli.exe vm inventory
```

Mutating commands return a JSON-friendly preview by default. Execute only after
reviewing that output in the same invocation:

```powershell
# Review only
.\WimForgeCli.exe vm create --provider virtualbox --name "Windows 11 QA" `
  --iso "C:\Media\Windows11.iso" --cpu 4 --memory-mib 8192 --disk-mib 65536

# Execute the freshly generated non-destructive preview
.\WimForgeCli.exe vm create --provider virtualbox --name "Windows 11 QA" `
  --iso "C:\Media\Windows11.iso" --cpu 4 --memory-mib 8192 --disk-mib 65536 `
  --execute --yes
```

Lifecycle, `configure`, `device`, `snapshot`, `forget`, `unregister`, and
`delete` subcommands follow the forms printed by `vm help`. A destructive
execution additionally needs `--confirm "<exact token from the preview>"`.
Never script a guessed or hard-coded token.

Project validation automation is under `vm validation start`, `update`,
`finish`, `show`, and `list`. It uses the same optimistic run revisions and
hashed-evidence rules as the desktop surface.

## Operational checklist

1. Install and license the chosen hypervisor yourself; reboot if its installer
   or virtualization driver requires it.
2. Refresh providers and read capability warnings.
3. Work from a disposable copy of the ISO and use a new managed VM for smoke
   tests.
4. Keep NAT or disconnected networking unless the test genuinely requires
   broader access.
5. Review every exact command and target; type destructive tokens only after
   checking them.
6. Capture hashes, milestones, logs, screenshots, and reviewer notes.
7. Preserve pristine media and independent backups even when snapshots exist.

See [Embedded Terminal](Embedded-Terminal), [Review and Run](Review-and-Run),
[Safety and Recovery](Safety-and-Recovery), and [Command-Line Interface](CLI).

---

[← WinForge Bridge](WinForge-Bridge) · [Embedded Terminal →](Embedded-Terminal)
