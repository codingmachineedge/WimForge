#!/usr/bin/env python3
"""Validate the checked-in Compose deployment's security contract."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import tempfile
from pathlib import Path
from typing import Any


class ContractError(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ContractError(message)


def compose_config(root: Path, files: list[Path], environment: dict[str, str]) -> dict[str, Any]:
    command = ["docker", "compose"]
    for path in files:
        command.extend(["--file", str(path)])
    command.extend(["config", "--format", "json"])
    try:
        result = subprocess.run(
            command,
            cwd=root,
            env=environment,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=30,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise ContractError("Docker Compose could not render the deployment configuration") from exc
    if result.returncode != 0:
        detail = result.stderr.strip().splitlines()
        raise ContractError(
            "Docker Compose rejected the deployment configuration: "
            + (detail[-1] if detail else "unknown failure")
        )
    try:
        document = json.loads(result.stdout)
    except json.JSONDecodeError as exc:
        raise ContractError("Docker Compose did not return valid JSON") from exc
    require(isinstance(document, dict), "Docker Compose output must be an object")
    return document


def provisioning_service(document: dict[str, Any]) -> dict[str, Any]:
    services = document.get("services")
    require(isinstance(services, dict), "Compose output must contain services")
    service = services.get("provisioning")
    require(isinstance(service, dict), "Compose output must contain the provisioning service")
    return service


def validate_base(
    service: dict[str, Any], *, expect_unauthenticated: bool = True
) -> None:
    require(service.get("user") == "65532:65532", "Compose must pin the unprivileged runtime user")
    require(service.get("read_only") is True, "Compose must use a read-only root filesystem")
    require("ALL" in service.get("cap_drop", []), "Compose must drop every Linux capability")
    require(
        "no-new-privileges:true" in service.get("security_opt", []),
        "Compose must enable no-new-privileges",
    )
    require(int(service.get("pids_limit", 0)) == 128, "Compose must retain the 128-task limit")
    require(int(service.get("mem_limit", 0)) == 512 * 1024 * 1024, "Compose must retain the 512 MiB limit")
    require(float(service.get("cpus", 0)) == 2.0, "Compose must retain the two-CPU limit")

    ports = service.get("ports")
    require(isinstance(ports, list) and len(ports) == 1, "Compose must publish exactly one port")
    port = ports[0]
    require(isinstance(port, dict), "Compose port configuration must be normalized")
    require(port.get("host_ip") == "127.0.0.1", "Unauthenticated Compose must publish on IPv4 loopback only")
    require(int(port.get("target", 0)) == 8080, "Compose must target the service's port 8080")

    environment = service.get("environment")
    require(isinstance(environment, dict), "Compose must define the service environment")
    if expect_unauthenticated:
        require(
            environment.get("WIMFORGE_ALLOW_UNAUTHENTICATED") == "loopback-only",
            "Loopback Compose must explicitly opt in to unauthenticated operation",
        )
    else:
        require(
            not environment.get("WIMFORGE_ALLOW_UNAUTHENTICATED"),
            "Authenticated Compose must disable unauthenticated mode",
        )
    require(
        int(environment.get("WIMFORGE_MAX_HTTP_WORKERS", 0)) == 16,
        "Compose must retain the bounded HTTP worker default",
    )

    tmpfs = service.get("tmpfs", [])
    require(
        any(item.startswith("/tmp:") and "size=64m" in item and "mode=1777" in item for item in tmpfs),
        "Compose must retain the bounded /tmp filesystem",
    )
    volumes = service.get("volumes", [])
    readonly_targets = {
        item.get("target")
        for item in volumes
        if isinstance(item, dict) and item.get("read_only") is True
    }
    require(
        {"/config", "/profiles"}.issubset(readonly_targets),
        "Compose must mount configuration and profiles read-only",
    )

    logging = service.get("logging")
    require(isinstance(logging, dict) and logging.get("driver") == "local", "Compose must use bounded local logs")
    options = logging.get("options", {})
    require(
        options.get("max-size") == "10m" and str(options.get("max-file")) == "3",
        "Compose must retain log size and rotation limits",
    )


def validate_token_overlay(document: dict[str, Any], service: dict[str, Any]) -> None:
    environment = service.get("environment", {})
    require(
        not environment.get("WIMFORGE_ALLOW_UNAUTHENTICATED"),
        "Token overlay must disable unauthenticated mode",
    )
    require(
        environment.get("WIMFORGE_API_TOKEN_FILE") == "/run/secrets/wimforge_api_token",
        "Token overlay must configure the in-container token path",
    )
    service_secrets = service.get("secrets", [])
    require(
        any(
            (item == "wimforge_api_token")
            or (isinstance(item, dict) and item.get("source") == "wimforge_api_token")
            for item in service_secrets
        ),
        "Token overlay must mount the declared token secret",
    )
    secrets = document.get("secrets", {})
    require(
        isinstance(secrets, dict) and "wimforge_api_token" in secrets,
        "Token overlay must declare its top-level secret",
    )


def validate_dockerignore(root: Path) -> None:
    entries = {
        line.strip()
        for line in (root / ".dockerignore").read_text(encoding="utf-8").splitlines()
        if line.strip() and not line.lstrip().startswith("#")
    }
    require("/build" in entries, "Docker context must exclude the root build directory")
    require(
        "/build-*" in entries,
        "Docker context must exclude root build variants without hiding source build scripts",
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--repository-root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
    )
    arguments = parser.parse_args()
    root = arguments.repository_root.resolve()
    base = root / "compose.yaml"
    overlay = root / "deploy/provisioning/compose.token.yaml"
    validate_dockerignore(root)

    environment = dict(os.environ)
    environment.pop("WIMFORGE_PORT", None)
    environment.pop("WIMFORGE_CONFIG_DIR", None)
    environment.pop("WIMFORGE_PROFILE_PATH", None)
    with tempfile.TemporaryDirectory(prefix="wimforge-compose-contract-") as directory:
        token = Path(directory) / "token"
        token.write_text("compose-contract-token-material\n", encoding="utf-8")
        environment["WIMFORGE_API_TOKEN_PATH"] = str(token)

        base_document = compose_config(root, [base], environment)
        validate_base(provisioning_service(base_document))

        overlay_document = compose_config(root, [base, overlay], environment)
        overlay_service = provisioning_service(overlay_document)
        validate_base(overlay_service, expect_unauthenticated=False)
        validate_token_overlay(overlay_document, overlay_service)

    print("compose_contract: bounded context, base, and token-overlay security checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
