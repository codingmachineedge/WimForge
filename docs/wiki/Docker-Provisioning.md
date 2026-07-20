# Docker Provisioning

The checked-in `.dockerignore` excludes the root `build` directory and every root `build-*` variant. Local Qt, capture, documentation-smoke, and install-smoke outputs therefore never inflate or leak into the Docker build context.

Repository 入面嘅 `.dockerignore` 會排除 root `build` 同所有 root `build-*` 變體，所以本機 Qt、capture、文件 smoke 同 install smoke output 都唔會塞大或者混入 Docker build context。

WimForge can run a small Linux container that assigns a reviewed unattended profile and fixed pre-OOBE computer name to known hardware. It is a Qt Core renderer/API, not the desktop application or a Linux replacement for DISM.

## Quick start

```powershell
Copy-Item deploy\provisioning\config.example.json `
  deploy\provisioning\config.local.json
$env:WIMFORGE_CONFIG_NAME = 'config.local.json'
docker compose up --build -d provisioning
Invoke-RestMethod http://127.0.0.1:8080/healthz
```

Linux hosts can use `cp`, `export WIMFORGE_CONFIG_NAME=config.local.json`, and the same Compose commands. The Compose default is loopback-only, non-root, read-only, capability-free, resource-bounded, uses a bounded temporary filesystem, and rotates bounded local logs. It explicitly opts in to unauthenticated loopback use; the runtime otherwise refuses to serve without a token. CI renders the base and token-overlay Compose models and enforces that coupling. The main-branch workflow publishes a Linux/amd64 image with provenance and an SBOM; use `--build` until the GHCR package exists/is public, then pin a verified digest for production because even a commit-addressed registry tag can be overwritten or rebuilt from newer package indexes.

In the sample's strict mode, the inventory maps a pre-registered UUID, BIOS serial, or MAC address to a fixed computer name, a built-in/operator JSON profile, and typed overrides for owner, organization, time zone, WinPE locales, Dynamic Update, and selected OOBE values. Duplicate/placeholder hardware IDs, unknown fields, invalid names, ambiguous matches, and unmatched devices fail closed. `requireKnownDevice:false` and random `*` names are available explicitly. The mounted configuration directory is reread on every request, so an atomic file replacement does not require a container restart.

## How Setup receives it

The service does not magically appear in Windows Setup's answer-file search paths. Use one of these supported deployment models:

- pre-render `Autounattend.xml` and place it at the root of recognized installation/removable media; or
- add `deploy/winpe/Invoke-WimForgeProvisioning.ps1` to a PowerShell-enabled WinPE/PXE image. It gathers hardware identity, downloads and verifies the XML, then launches `setup.exe /unattend:<local-file>`.

The WinPE client fails before Setup starts on an unknown device, authentication/network error, invalid digest, or invalid XML. A known fixed `ComputerName` is rendered in `specialize`, before OOBE. See Microsoft's [answer-file search behavior](https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/windows-setup-automation-overview?view=windows-11), [`/Unattend` option](https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/windows-setup-command-line-options?view=windows-11#unattend), and [`ComputerName` setting](https://learn.microsoft.com/en-us/windows-hardware/customize/desktop/unattend/microsoft-windows-shell-setup-computername).

## Important boundaries

- `builtin:full` is a conservative baseline, not a complete disk/image/account decision set. No-touch installation requires an exact-image profile covering those choices. It contains `HideEULAPage=true`; the sample overrides that to `false` because Microsoft restricts OEM/System Builder use to pre-shipment testing.
- Provisioning v1 supports amd64 target profiles. x86-only and ARM64 targets fail preflight.
- Clients submit hardware identity only. They cannot choose a name, profile, setting, path, or command.
- UUID/serial/MAC values are selectors, not authentication or attestation; any shared-token holder can claim a known/cloned selector.
- Common secret-bearing paths are rejected, but arbitrary command/blob values cannot be classified reliably; hosted custom profiles must be secret-free.
- Use a dedicated provisioning network, HTTPS reverse proxy, and the token-file Compose overlay before exposing the API beyond loopback.
- A response digest detects corruption; it is not a signature and does not replace TLS.
- The WinPE client refuses redirects/downgrades, streams a size-bounded response, retries transient failures with bounded backoff, and can write its redacted pre-Setup log to a persistent technician share.
- Validate the final XML in Windows SIM and perform a clean VM installation with no input before production use.

The complete schema, custom-profile, API, token, one-shot-render, WinPE, and test instructions are in [Docker provisioning](https://ding-ding-projects.github.io/WimForge/docs/docker-provisioning/).

---

[← Unattended Studio](Unattended-Studio) · [WinForge Bridge →](WinForge-Bridge)
