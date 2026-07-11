#!/usr/bin/env python3
"""Exercise the built provisioning image as a locked-down running container."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import tempfile
import time
import urllib.error
import urllib.request
import uuid
import xml.etree.ElementTree as ET
from pathlib import Path


UUID = "4c4c4544-0042-4710-8058-cac04f564d31"
EXPECTED_NAME = "LAB-NODE-001"


def run(arguments: list[str], *, capture: bool = True) -> str:
    result = subprocess.run(
        arguments,
        check=False,
        text=True,
        encoding="utf-8",
        errors="replace",
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.PIPE if capture else None,
    )
    if result.returncode != 0:
        detail = (result.stderr or result.stdout or "").strip()
        raise RuntimeError(f"command failed ({result.returncode}): {' '.join(arguments)}\n{detail}")
    return (result.stdout or "").strip()


def post(url: str, token: str) -> tuple[bytes, object]:
    request = urllib.request.Request(
        url,
        data=json.dumps({"uuid": UUID}).encode("utf-8"),
        headers={
            "Authorization": f"Bearer {token}",
            "Content-Type": "application/json",
        },
        method="POST",
    )
    response = urllib.request.urlopen(request, timeout=30)
    return response.read(), response.headers


def computer_name(data: bytes) -> str:
    namespace = "{urn:schemas-microsoft-com:unattend}"
    root = ET.fromstring(data)
    values = [element.text or "" for element in root.iter(namespace + "ComputerName")]
    if values != [EXPECTED_NAME]:
        raise RuntimeError(f"unexpected ComputerName values: {values}")
    return values[0]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--image", required=True)
    parser.add_argument(
        "--repository-root", type=Path, default=Path(__file__).resolve().parents[1]
    )
    arguments = parser.parse_args()
    container = f"wimforge-provisioning-smoke-{uuid.uuid4().hex[:10]}"

    with tempfile.TemporaryDirectory(prefix="wimforge-container-smoke-") as directory:
        root = Path(directory)
        config_directory = root / "config"
        profile_directory = root / "profiles"
        output_directory = root / "output"
        config_directory.mkdir()
        profile_directory.mkdir()
        output_directory.mkdir()
        if os.name != "nt":
            output_directory.chmod(0o777)
        shutil.copyfile(
            arguments.repository_root / "deploy/provisioning/config.example.json",
            config_directory / "provisioning.json",
        )
        token = "container-smoke-token-with-more-than-sixteen-characters"
        token_path = root / "token"
        token_path.write_text(token + "\n", encoding="utf-8")

        try:
            run(
                [
                    "docker",
                    "run",
                    "--detach",
                    "--name",
                    container,
                    "--read-only",
                    "--tmpfs",
                    "/tmp:rw,size=64m,mode=1777",
                    "--cap-drop",
                    "ALL",
                    "--security-opt",
                    "no-new-privileges",
                    "--memory",
                    "512m",
                    "--pids-limit",
                    "128",
                    "--health-interval",
                    "1s",
                    "--health-timeout",
                    "2s",
                    "--health-start-period",
                    "1s",
                    "--mount",
                    f"type=bind,source={config_directory.resolve()},destination=/config,readonly",
                    "--mount",
                    f"type=bind,source={profile_directory.resolve()},destination=/profiles,readonly",
                    "--mount",
                    f"type=bind,source={token_path.resolve()},destination=/run/secrets/token,readonly",
                    "--env",
                    "WIMFORGE_API_TOKEN_FILE=/run/secrets/token",
                    "--publish",
                    "127.0.0.1::8080",
                    arguments.image,
                ]
            )
            port_output = run(["docker", "port", container, "8080/tcp"])
            port = int(port_output.rsplit(":", 1)[1])
            base_url = f"http://127.0.0.1:{port}"

            ready = False
            for _attempt in range(60):
                try:
                    with urllib.request.urlopen(base_url + "/healthz", timeout=2) as response:
                        ready = response.status == 200
                    if ready:
                        break
                except (OSError, urllib.error.URLError):
                    time.sleep(0.25)
            if not ready:
                raise RuntimeError("container health endpoint did not become ready")

            unauthorized = urllib.request.Request(
                base_url + "/v1/unattend",
                data=json.dumps({"uuid": UUID}).encode("utf-8"),
                headers={"Content-Type": "application/json"},
                method="POST",
            )
            try:
                urllib.request.urlopen(unauthorized, timeout=10)
                raise RuntimeError("unauthorized render unexpectedly succeeded")
            except urllib.error.HTTPError as exc:
                try:
                    if exc.code != 401:
                        raise RuntimeError(f"unauthorized render returned HTTP {exc.code}")
                finally:
                    exc.close()

            data, headers = post(base_url + "/v1/unattend", token)
            computer_name(data)
            if headers["X-WimForge-Assignment"] != "lab-node-001":
                raise RuntimeError("assignment response header was incorrect")
            if headers["X-WimForge-SHA256"] != hashlib.sha256(data).hexdigest():
                raise RuntimeError("response digest header was incorrect")

            inspect = json.loads(run(["docker", "inspect", container]))[0]
            if inspect["Config"]["User"] != "65532:65532":
                raise RuntimeError("container did not run as the declared unprivileged user")
            if not inspect["HostConfig"]["ReadonlyRootfs"]:
                raise RuntimeError("container root filesystem was not read-only")
            if "ALL" not in (inspect["HostConfig"].get("CapDrop") or []):
                raise RuntimeError("container capabilities were not dropped")

            for _attempt in range(20):
                health = run(
                    [
                        "docker",
                        "inspect",
                        "--format",
                        "{{.State.Health.Status}}",
                        container,
                    ]
                )
                if health == "healthy":
                    break
                time.sleep(0.5)
            else:
                raise RuntimeError("Docker HEALTHCHECK did not become healthy")
        except Exception:
            logs = run(["docker", "logs", container]) if run(
                ["docker", "ps", "--all", "--quiet", "--filter", f"name=^{container}$"]
            ) else ""
            if logs:
                print(logs)
            raise
        finally:
            subprocess.run(
                ["docker", "rm", "--force", container],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                check=False,
            )

        output = output_directory / "Autounattend.xml"
        run(
            [
                "docker",
                "run",
                "--rm",
                "--read-only",
                "--tmpfs",
                "/tmp:rw,size=64m,mode=1777",
                "--cap-drop",
                "ALL",
                "--security-opt",
                "no-new-privileges",
                "--mount",
                f"type=bind,source={config_directory.resolve()},destination=/config,readonly",
                "--mount",
                f"type=bind,source={profile_directory.resolve()},destination=/profiles,readonly",
                "--mount",
                f"type=bind,source={output_directory.resolve()},destination=/output",
                arguments.image,
                "render",
                "--uuid",
                UUID,
                "--output",
                "/output/Autounattend.xml",
            ]
        )
        computer_name(output.read_bytes())

    print("container_smoke: health, auth, render, digest, isolation, and one-shot checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
