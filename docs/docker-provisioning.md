# Docker provisioning

WimForge includes a small Linux container for rendering and serving device-specific Windows answer files. It runs the Qt Core CLI rather than the desktop application, so it does not need a Windows GUI and does not run DISM inside Linux.

The provisioning path is deliberately split into two pieces:

1. The Docker service matches hardware identifiers to an operator-owned profile, fixed computer name, and allow-listed settings. It asks `WimForgeCli` to validate and render the final XML.
2. A deployment system downloads that XML **before Windows Setup starts**. The included WinPE script then launches `setup.exe /unattend:<local-file>`.

That second step is required for central per-device naming. Windows Setup automatically searches defined local and removable-media paths; it does not discover an arbitrary HTTP service. Microsoft documents both the [implicit answer-file search order](https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/windows-setup-automation-overview?view=windows-11) and the WinPE-only [`setup.exe /Unattend` option](https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/windows-setup-command-line-options?view=windows-11#unattend).

## What this automates

For a known device, the service can set a fixed `ComputerName` in the `specialize` pass, before OOBE. It can also override these common values:

| Provisioning key | Answer-file setting |
| --- | --- |
| `registeredOwner` | `specialize / Microsoft-Windows-Shell-Setup / RegisteredOwner` |
| `registeredOrganization` | `specialize / Microsoft-Windows-Shell-Setup / RegisteredOrganization` |
| `timeZone` | `specialize / Microsoft-Windows-Shell-Setup / TimeZone` |
| `inputLocale` | `windowsPE / Microsoft-Windows-International-Core-WinPE / InputLocale` |
| `systemLocale` | `windowsPE / Microsoft-Windows-International-Core-WinPE / SystemLocale` |
| `uiLanguage` | `windowsPE / Microsoft-Windows-International-Core-WinPE / UILanguage` |
| `userLocale` | `windowsPE / Microsoft-Windows-International-Core-WinPE / UserLocale` |
| `dynamicUpdate` | `windowsPE / Microsoft-Windows-Setup / DynamicUpdate / Enable` |
| `hideEulaPage` | `oobeSystem / Microsoft-Windows-Shell-Setup / OOBE / HideEULAPage` |
| `hideWirelessSetupInOobe` | `oobeSystem / Microsoft-Windows-Shell-Setup / OOBE / HideWirelessSetupInOOBE` |
| `protectYourPC` | `oobeSystem / Microsoft-Windows-Shell-Setup / OOBE / ProtectYourPC` |

A custom WimForge JSON profile may already contain other reviewed settings, including disk/image selection. The service preserves them and applies only the typed overrides above. The request cannot submit an answer-file path, setting, command, computer name, or profile name.

The built-in `full` template is a conservative baseline, not proof of a universally no-touch installation. It does not choose a disk, image/edition, or account. A genuinely keyboard-free deployment needs an operator-supplied profile that covers those decisions for the exact Windows image. Microsoft documents `ComputerName` as an answer-file value with a 15-byte limit and valid use in `specialize`; see the [ComputerName reference](https://learn.microsoft.com/en-us/windows-hardware/customize/desktop/unattend/microsoft-windows-shell-setup-computername).

That built-in template contains `HideEULAPage=true`. The sample provisioning configuration overrides it to `false`; Microsoft says OEMs and System Builders may hide that page only for testing before shipment. Review the [HideEULAPage restriction](https://learn.microsoft.com/en-us/windows-hardware/customize/desktop/unattend/microsoft-windows-shell-setup-oobe-hideeulapage) before changing the sample.

Provisioning schema v1 targets amd64 Windows profiles. Mixed x86 WinPE settings already present in an amd64 target profile are updated alongside amd64 values, but x86-only and ARM64 target profiles are rejected rather than being contaminated with an amd64 computer-name component.

## Start the service

Copy the sample inventory so local hardware identifiers do not become repository changes:

```powershell
Copy-Item deploy\provisioning\config.example.json `
  deploy\provisioning\config.local.json
$env:WIMFORGE_CONFIG_NAME = 'config.local.json'
docker compose up --build -d provisioning
docker compose ps
```

On a Linux Docker host, the equivalent is:

```bash
cp deploy/provisioning/config.example.json deploy/provisioning/config.local.json
export WIMFORGE_CONFIG_NAME=config.local.json
docker compose up --build -d provisioning
docker compose ps
```

The checked-in Compose file mounts the configuration **directory** so atomic file replacement is visible inside the container. It binds to `127.0.0.1:8080`, explicitly opts in to unauthenticated **loopback-only** use, drops Linux capabilities, uses a read-only root filesystem, bounds memory/PIDs/CPU and temporary storage, rotates size-bounded local logs, and runs as UID/GID `65532`. A CI contract renders both Compose files and fails if that opt-in is no longer coupled to the loopback port or if the security/resource/logging controls drift. Test the health endpoint:

```powershell
Invoke-RestMethod http://127.0.0.1:8080/healthz
```

The main-branch workflow is configured to publish this Linux/amd64 image with provenance and an SBOM:

```text
ghcr.io/ding-ding-projects/wimforge-provisioning:latest
```

Until a workflow run has published the package and its owner has made it public, use `--build` as above or authenticate with `docker login ghcr.io`. A `sha-<full-commit>` tag is easier to audit than `latest`, but a registry tag can be overwritten and rebuilding the same commit can pick up newer signed Debian packages. For production, pin a verified image digest and enforce registry tag immutability or a reviewed rebuild policy. The local build compiles the CLI and runs the Linux-compatible CTest gate before creating the runtime image.

## Inventory format

The configuration schema is `wimforge.provisioning`, version `1`:

Strict mode requires devices to be inventoried **before** their first boot. On an existing Windows installation or a technician WinPE session with the required WMI components, collect candidate values with:

```powershell
Get-CimInstance Win32_ComputerSystemProduct | Select-Object UUID
Get-CimInstance Win32_BIOS | Select-Object SerialNumber
Get-CimInstance Win32_NetworkAdapter |
  Where-Object { $_.PhysicalAdapter -and $_.MACAddress } |
  Select-Object Name, MACAddress
```

Use stable values from the actual fleet, not the example identifiers below. The service intentionally does not log request bodies, so a failed first boot is not an enrollment/discovery mechanism.

```json
{
  "schema": "wimforge.provisioning",
  "version": 1,
  "requireKnownDevice": true,
  "defaults": {
    "profile": "builtin:full",
    "computerName": "*",
    "settings": {
      "timeZone": "Eastern Standard Time",
      "userLocale": "en-CA"
    }
  },
  "devices": [
    {
      "id": "lab-node-001",
      "match": {
        "uuid": "4c4c4544-0042-4710-8058-cac04f564d31",
        "serial": "EXAMPLE-SERIAL-001",
        "mac": ["02:00:00:00:00:01"]
      },
      "computerName": "LAB-NODE-001",
      "settings": {
        "registeredOwner": "Lab Operations"
      }
    }
  ]
}
```

Matching has these safety rules:

- UUIDs, serials, and MAC addresses are normalized case-insensitively.
- Known placeholder IDs, zero/broadcast/multicast MAC addresses, duplicated inventory identifiers or fixed computer names, duplicate JSON properties, and unknown configuration fields are rejected.
- Invalid optional identifiers in a request are ignored when another usable identifier remains; a request with none receives `400`.
- Any matching identifier selects a device. If one request contains identifiers belonging to different devices, the response is `409` and no XML is returned.
- With `requireKnownDevice: true`, an unmatched device receives `404`; Windows Setup is not started by the included bootstrap.
- Configuration is reread for every request. An atomic file replacement changes future assignments without restarting the container; invalid configuration fails closed.
- Fixed names use WimForge's conservative 1–15 byte ASCII rules. `*` asks Windows to generate a random name.

These values are selectors, not device authentication or hardware attestation. Anyone holding the shared bearer token can claim a known identifier, and cloned/duplicated firmware identifiers can select the same assignment. Keep the endpoint on the trusted provisioning network and investigate real-world identifier collisions before rollout.

The following environment variables can override defaults without changing JSON:

- `WIMFORGE_DEFAULT_PROFILE`
- `WIMFORGE_DEFAULT_COMPUTER_NAME`
- `WIMFORGE_DEFAULT_REGISTERED_OWNER`
- `WIMFORGE_DEFAULT_REGISTERED_ORGANIZATION`
- `WIMFORGE_DEFAULT_TIME_ZONE`
- `WIMFORGE_DEFAULT_INPUT_LOCALE`
- `WIMFORGE_DEFAULT_SYSTEM_LOCALE`
- `WIMFORGE_DEFAULT_UI_LANGUAGE`
- `WIMFORGE_DEFAULT_USER_LOCALE`
- `WIMFORGE_DEFAULT_DYNAMIC_UPDATE`
- `WIMFORGE_DEFAULT_HIDE_EULA_PAGE`
- `WIMFORGE_DEFAULT_HIDE_WIRELESS_SETUP_IN_OOBE`
- `WIMFORGE_DEFAULT_PROTECT_YOUR_PC`

`WIMFORGE_MAX_HTTP_WORKERS` bounds live HTTP request threads (default `16`, maximum `128`). `WIMFORGE_MAX_CONCURRENT_RENDERS` separately bounds simultaneous CLI processes (default `4`, maximum `32`), and `WIMFORGE_RENDER_TIMEOUT_SECONDS` bounds each CLI render (default `20`, maximum `120`). A saturated HTTP pool returns `503`; a full render queue returns `429`. Successful health validation is serialized and briefly cached so unauthenticated orchestration probes cannot multiply CLI/profile checks.

The base Compose file passes through the concurrency/timeout values. To use any `WIMFORGE_DEFAULT_*` value with Compose, put it under the service's container environment in a local override—not merely in the host shell:

```yaml
services:
  provisioning:
    environment:
      WIMFORGE_DEFAULT_TIME_ZONE: Pacific Standard Time
      WIMFORGE_DEFAULT_USER_LOCALE: en-US
```

Device-level JSON overrides still take precedence. A fixed default name is allowed only when strict inventory resolves it to one unique device; unknown-device mode requires the default name `*`.

## Custom profiles

Export a portable JSON profile, place it under the mounted profile directory, and reference its filename without `.json`:

```powershell
.\WimForgeCli.exe unattend template full `
  --output deploy\provisioning\profiles\office.json
```

Then set a device or the defaults to `"profile": "office"`. Only simple file stems are accepted; absolute paths and traversal are rejected. Built-in choices are `builtin:full` and `builtin:ai-development`.

Health/preflight runs the canonical CLI validator for every referenced profile and rejects non-amd64 target profiles before serving requests. If a typed override appears more than once or in multiple architecture components, every occurrence is updated and the rendered XML is checked again before release.

The service refuses common secret-bearing paths such as passwords, credentials, product keys, wireless key material/profiles, and domain-join account blobs. That is a defensive heuristic, not secret detection: arbitrary command strings and vendor-specific fields cannot be classified reliably. Every hosted custom profile must be secret-free and kept under the same review discipline as any normal answer file. Validate the result against the exact image in Windows SIM.

## Render through the API

Only `POST /v1/unattend` renders an answer file. Hardware identity stays in the JSON body rather than the URL:

```powershell
$request = @{
  uuid = '4c4c4544-0042-4710-8058-cac04f564d31'
  serial = 'EXAMPLE-SERIAL-001'
  macs = @('02:00:00:00:00:01')
} | ConvertTo-Json -Compress

Invoke-WebRequest `
  -Uri http://127.0.0.1:8080/v1/unattend `
  -Method Post `
  -ContentType application/json `
  -Body $request `
  -OutFile Autounattend.xml
```

A successful response is `application/xml` and includes the assignment ID and SHA-256 digest in headers. Responses use `Cache-Control: no-store`. Request bodies are bounded at 16 KiB and rendered files at 8 MiB. Each CLI subprocess has a 20-second default timeout; a built-in render can use a template subprocess plus a final render after up to five seconds in the bounded queue.

The same image can perform a one-shot render without opening a port. Build a local tag first, create the output directory, and mount both the configuration directory and custom-profile directory:

```powershell
docker build -t wimforge-provisioning:local .
New-Item -ItemType Directory -Force out | Out-Null
docker run --rm --read-only --tmpfs /tmp:rw,size=64m `
  --cap-drop ALL --security-opt no-new-privileges `
  -e WIMFORGE_CONFIG_FILE=/config/config.local.json `
  -v "${PWD}/deploy/provisioning:/config:ro" `
  -v "${PWD}/deploy/provisioning/profiles:/profiles:ro" `
  -v "${PWD}/out:/output" `
  wimforge-provisioning:local `
  render --uuid 4c4c4544-0042-4710-8058-cac04f564d31 `
  --output /output/Autounattend.xml
```

On Linux, create a user-writable output directory and run the one-shot process with the caller's numeric IDs:

```bash
docker build -t wimforge-provisioning:local .
mkdir -p out
docker run --rm --read-only --tmpfs /tmp:rw,size=64m \
  --cap-drop ALL --security-opt no-new-privileges \
  --user "$(id -u):$(id -g)" \
  -e WIMFORGE_CONFIG_FILE=/config/config.local.json \
  -v "$PWD/deploy/provisioning:/config:ro" \
  -v "$PWD/deploy/provisioning/profiles:/profiles:ro" \
  -v "$PWD/out:/output" \
  wimforge-provisioning:local \
  render --uuid 4c4c4544-0042-4710-8058-cac04f564d31 \
  --output /output/Autounattend.xml
```

## Authentication and TLS

The runtime refuses to serve without a token unless `WIMFORGE_ALLOW_UNAUTHENTICATED=loopback-only` is explicitly set. The checked-in Compose default sets that acknowledgement and publishes only on `127.0.0.1`; do not copy the opt-in into a public `docker run -p 8080:8080 ...` invocation. The loopback-only Compose default is suitable for local testing. Before another machine can reach the service:

1. Put it on a dedicated, trusted provisioning network.
2. Terminate HTTPS at a reviewed reverse proxy.
3. Create a high-entropy bearer-token file outside the repository.
4. Start with the token overlay:

```powershell
$env:WIMFORGE_API_TOKEN_PATH = 'C:\Secure\wimforge-provisioning.token'
docker compose `
  -f compose.yaml `
  -f deploy/provisioning/compose.token.yaml `
  up -d provisioning
```

Authenticated requests use `Authorization: Bearer <token>`. Compose secrets are mounted files rather than a live secret store; after replacing the host token file, recreate the service with `docker compose up -d --force-recreate provisioning`. Never put the token in a URL, profile, Git repository, Docker image, command-line argument, or BuildKit context. A bearer token copied into a boot image can be extracted; scope and rotate it for the deployment window or have the PXE system inject short-lived material.

The response digest detects accidental corruption. It is not a signature and does not replace authenticated TLS; an HTTP attacker could alter both the XML and digest.

## WinPE/PXE handoff

`deploy/winpe/Invoke-WimForgeProvisioning.ps1` implements the client side. It:

1. waits for WinPE networking;
2. probes SMBIOS UUID, BIOS serial, and physical MAC addresses independently, keeping whatever usable selectors are available;
3. posts only those selectors to the service;
4. rejects redirects/downgrades and bounds each streamed response to 8 MiB;
5. retries network timeouts, `408`, `429`, and `5xx` with bounded exponential backoff;
6. verifies the response digest and parses XML with DTD processing disabled;
7. writes `X:\WimForge\Autounattend.xml` atomically; and
8. launches the configured `setup.exe` with `/Unattend:<file>`.

Any inventory, authentication, network, digest, XML, or rendering failure occurs before `setup.exe` starts. There is no silent fallback into an interactive or incorrectly named installation.

Windows PE does not include PowerShell support in every image. Add the Microsoft-documented [WinPE PowerShell optional components and their dependencies](https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/winpe-adding-powershell-support-to-windows-pe?view=windows-11), plus the exact network/storage drivers required by the target hardware. Before sealing `boot.wim`:

- import the HTTPS proxy/root CA into WinPE's trust store and verify DNS plus system time;
- make the Windows media reachable at the configured `SetupPath`;
- for an authenticated SMB/UNC source, map it in `Startnet.cmd` using deployment-scoped credentials supplied securely by the PXE environment—do not bake reusable credentials into the repository or script;
- copy the provisioning script and scoped token material into/inject them for the boot image; and
- adapt `deploy/winpe/Startnet.example.cmd` as `Startnet.cmd`.

HTTPS is required by default and redirects are refused. `-AllowHttp` exists only for an isolated lab. The default 60-second per-attempt timeout exceeds the service's default worst-case render budget; `-MaxAttempts`, `-InitialRetrySeconds`, `-TimeoutSeconds`, and `-MaxResponseBytes` remain bounded and configurable.

The client writes a redacted pre-Setup log to `X:\WimForge\Provisioning.log`. That RAM-disk log disappears on reboot. For persistent failure evidence, pass `-LogPath` to a technician-controlled writable network share that has already been authenticated; the log records stages/errors but not hardware selectors or token values. Panther logs begin only after Windows Setup starts and cannot explain an earlier TLS, identity, or download failure.

If the answer file is pre-rendered instead, save it as `Autounattend.xml` at a Windows Setup search location such as the root of removable media. No Docker service is needed during installation in that mode.

## Required deployment validation

The automated gates cover inventory/ambiguity rules, bounded render and HTTP concurrency, cached health behavior, canonical custom-profile validation, duplicate and cross-architecture override handling, effective fixed-name XML, common overrides, sensitive-path refusal, HTTP authentication/status/digests, the base/token-overlay Compose security contract, a locked-down running container, Docker `HEALTHCHECK`, one-shot rendering, and WinPE script syntax/relative-output resolution. They do not execute a real WinPE environment, TLS/SMB infrastructure, Windows SIM, or a Windows installation.

Before production use:

1. Validate every supported amd64 profile variant in Windows SIM against the exact WIM/catalog.
2. Perform a clean VM installation with no keyboard or mouse input.
3. Confirm the fixed name exists before OOBE, and validate its ordering against the exact offline/specialize domain-join workflow if one is present.
4. Exercise unknown/ambiguous hardware, server outage, invalid TLS, token rotation, and slow-network paths.
5. Preserve the WinPE provisioning log for pre-Setup failures; inspect Setup/Panther logs after Setup starts and remove cached sensitive answer-file material at the correct lifecycle point.

Microsoft's [answer-file authoring best practices](https://learn.microsoft.com/en-us/windows-hardware/customize/desktop/wsim/best-practices-for-authoring-answer-files) explain why answer files must be treated as sensitive and revalidated against the target image.
