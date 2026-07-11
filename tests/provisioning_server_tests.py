#!/usr/bin/env python3

from __future__ import annotations

import concurrent.futures
import hashlib
import json
import os
import socket
import sys
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
import xml.etree.ElementTree as ET
from pathlib import Path
from socketserver import BaseRequestHandler
from unittest import mock


REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY_ROOT / "server"))

import provisioning_server as provisioning  # noqa: E402


UUID_ONE = "11111111-2222-4333-8444-555555555555"
UUID_TWO = "aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee"
SERIAL_ONE = "EXAMPLE-SERIAL-ONE"
SERIAL_TWO = "EXAMPLE-SERIAL-TWO"
MAC_ONE = "02:00:00:00:00:11"
MAC_TWO = "02:00:00:00:00:22"


def base_config() -> dict:
    return {
        "schema": "wimforge.provisioning",
        "version": 1,
        "requireKnownDevice": True,
        "defaults": {
            "profile": "builtin:full",
            "computerName": "*",
            "settings": {
                "timeZone": "UTC",
                "userLocale": "en-US",
                "dynamicUpdate": True,
            },
        },
        "devices": [
            {
                "id": "node-one",
                "match": {
                    "uuid": UUID_ONE,
                    "serial": SERIAL_ONE,
                    "mac": [MAC_ONE],
                },
                "computerName": "LAB-NODE-01",
                "settings": {
                    "timeZone": "Eastern Standard Time",
                    "registeredOwner": "Lab One",
                },
            },
            {
                "id": "node-two",
                "match": {
                    "uuid": UUID_TWO,
                    "serial": SERIAL_TWO,
                    "mac": [MAC_TWO],
                },
                "computerName": "LAB-NODE-02",
            },
        ],
    }


class ConfigTests(unittest.TestCase):
    def test_schema_version_does_not_accept_boolean_json(self) -> None:
        document = base_config()
        document["version"] = True
        with self.assertRaises(provisioning.ConfigError):
            provisioning.parse_config(document, {})

    def test_known_device_merges_defaults_and_overrides(self) -> None:
        config = provisioning.parse_config(base_config(), {})
        assignment = config.resolve(
            provisioning.parse_request({"uuid": UUID_ONE, "macs": [MAC_ONE]})
        )
        self.assertEqual(assignment.assignment_id, "node-one")
        self.assertEqual(assignment.computer_name, "LAB-NODE-01")
        self.assertEqual(assignment.settings["timeZone"], "Eastern Standard Time")
        self.assertEqual(assignment.settings["userLocale"], "en-US")
        self.assertEqual(assignment.settings["dynamicUpdate"], "true")

    def test_unknown_and_conflicting_identity_fail_closed(self) -> None:
        config = provisioning.parse_config(base_config(), {})
        with self.assertRaises(provisioning.UnknownDevice):
            config.resolve(provisioning.parse_request({"serial": "not-in-inventory"}))
        with self.assertRaises(provisioning.AmbiguousDevice):
            config.resolve(
                provisioning.parse_request({"uuid": UUID_ONE, "serial": SERIAL_TWO})
            )

    def test_duplicate_inventory_identity_is_rejected(self) -> None:
        document = base_config()
        document["devices"][1]["match"]["uuid"] = UUID_ONE.upper()
        with self.assertRaises(provisioning.ConfigError):
            provisioning.parse_config(document, {})

    def test_duplicate_fixed_names_are_rejected_case_insensitively(self) -> None:
        document = base_config()
        document["devices"][1]["computerName"] = "lab-node-01"
        with self.assertRaises(provisioning.ConfigError):
            provisioning.parse_config(document, {})

    def test_unknown_devices_cannot_share_a_fixed_default_name(self) -> None:
        document = base_config()
        document["requireKnownDevice"] = False
        document["defaults"]["computerName"] = "SAME-FOR-ALL"
        with self.assertRaises(provisioning.ConfigError):
            provisioning.parse_config(document, {})

    def test_invalid_name_unknown_setting_and_profile_traversal_are_rejected(self) -> None:
        for mutation in ("name", "setting", "profile"):
            with self.subTest(mutation=mutation):
                document = base_config()
                if mutation == "name":
                    document["devices"][0]["computerName"] = "THIS-NAME-IS-TOO-LONG"
                elif mutation == "setting":
                    document["devices"][0]["settings"]["runCommand"] = "whoami"
                else:
                    document["devices"][0]["profile"] = "../outside"
                with self.assertRaises(provisioning.ConfigError):
                    provisioning.parse_config(document, {})

    def test_request_schema_is_strict(self) -> None:
        invalid = [
            {},
            {"uuid": UUID_ONE, "computerName": "ATTACKER"},
            {"uuid": "00000000-0000-0000-0000-000000000000"},
            {"macs": ["ff:ff:ff:ff:ff:ff"]},
        ]
        for request in invalid:
            with self.subTest(request=request), self.assertRaises(provisioning.InvalidRequest):
                provisioning.parse_request(request)

    def test_invalid_optional_identifier_does_not_poison_a_valid_one(self) -> None:
        identity = provisioning.parse_request(
            {"uuid": UUID_ONE, "serial": "serial with unsupported spaces"}
        )
        assignment = provisioning.parse_config(base_config(), {}).resolve(identity)
        self.assertEqual(assignment.assignment_id, "node-one")

    def test_environment_overrides_only_defaults(self) -> None:
        config = provisioning.parse_config(
            base_config(),
            {
                "WIMFORGE_DEFAULT_TIME_ZONE": "Pacific Standard Time",
                "WIMFORGE_DEFAULT_DYNAMIC_UPDATE": "false",
            },
        )
        node_one = config.resolve(provisioning.parse_request({"uuid": UUID_ONE}))
        node_two = config.resolve(provisioning.parse_request({"uuid": UUID_TWO}))
        self.assertEqual(node_one.settings["timeZone"], "Eastern Standard Time")
        self.assertEqual(node_two.settings["timeZone"], "Pacific Standard Time")
        self.assertEqual(node_two.settings["dynamicUpdate"], "false")

    def test_invalid_utf8_files_fail_with_controlled_errors(self) -> None:
        with tempfile.TemporaryDirectory(prefix="wimforge-invalid-utf8-") as directory:
            root = Path(directory)
            config_path = root / "provisioning.json"
            config_path.write_bytes(b"\xff\xfe\x00")
            with self.assertRaisesRegex(provisioning.ConfigError, "UTF-8"):
                provisioning.ConfigStore(config_path, {}).load()

            token_path = root / "token"
            token_path.write_bytes(b"\xff\xfe\x00")
            with self.assertRaisesRegex(provisioning.ConfigError, "UTF-8"):
                provisioning.TokenAuthorizer(token_path).check_startup()

            profile_path = root / "invalid-profile.json"
            profile_path.write_bytes(b"\xff\xfe\x00")
            renderer = provisioning.CliRenderer(["unused-cli"], root)
            with self.assertRaisesRegex(provisioning.RenderError, "UTF-8"):
                renderer._load_profile("invalid-profile", root)

    def test_http_authentication_is_fail_closed_by_default(self) -> None:
        with mock.patch.dict(os.environ, {}, clear=True):
            with self.assertRaisesRegex(provisioning.ConfigError, "requires"):
                provisioning._service_from_environment()
            render_service = provisioning._service_from_environment(
                require_api_authentication=False
            )
            self.assertIsNone(render_service.authorizer.token_file)

        with mock.patch.dict(
            os.environ,
            {"WIMFORGE_ALLOW_UNAUTHENTICATED": "loopback-only"},
            clear=True,
        ):
            service = provisioning._service_from_environment()
            self.assertIsNone(service.authorizer.token_file)

        with mock.patch.dict(
            os.environ,
            {"WIMFORGE_ALLOW_UNAUTHENTICATED": "true"},
            clear=True,
        ):
            with self.assertRaisesRegex(provisioning.ConfigError, "exactly"):
                provisioning._service_from_environment()

        with mock.patch.dict(
            os.environ,
            {
                "WIMFORGE_API_TOKEN_FILE": "token",
                "WIMFORGE_ALLOW_UNAUTHENTICATED": "loopback-only",
            },
            clear=True,
        ):
            with self.assertRaisesRegex(provisioning.ConfigError, "either"):
                provisioning._service_from_environment()

    def test_health_validation_is_serialized_and_cached(self) -> None:
        config = provisioning.parse_config(base_config(), {})

        class CountingStore:
            def __init__(self) -> None:
                self.loads = 0

            def load(self) -> provisioning.ProvisioningConfig:
                self.loads += 1
                return config

        class CountingRenderer:
            def __init__(self) -> None:
                self.profile_checks = 0

            def check_profile_reference(self, _profile: str) -> None:
                self.profile_checks += 1

        store = CountingStore()
        renderer = CountingRenderer()
        service = provisioning.ProvisioningService(
            store,  # type: ignore[arg-type]
            renderer,  # type: ignore[arg-type]
            provisioning.TokenAuthorizer(None),
            health_cache_seconds=60,
        )
        service.health()
        service.health()
        self.assertEqual(store.loads, 1)
        self.assertEqual(renderer.profile_checks, 1)
        service.health(force=True)
        self.assertEqual(store.loads, 2)
        self.assertEqual(renderer.profile_checks, 2)

    def test_http_worker_threads_are_bounded(self) -> None:
        started = threading.Event()
        release = threading.Event()

        class BlockingHandler(BaseRequestHandler):
            def handle(self) -> None:
                started.set()
                release.wait(timeout=5)

        server = provisioning.ProvisioningHttpServer(
            ("127.0.0.1", 0),
            BlockingHandler,  # type: ignore[arg-type]
            max_worker_threads=1,
        )
        server_thread = threading.Thread(target=server.serve_forever, daemon=True)
        server_thread.start()
        first: socket.socket | None = None
        second: socket.socket | None = None
        try:
            address = ("127.0.0.1", server.server_address[1])
            first = socket.create_connection(address, timeout=2)
            self.assertTrue(started.wait(timeout=2))
            second = socket.create_connection(address, timeout=2)
            second.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
            response = second.recv(4096)
            self.assertIn(b"503 Service Unavailable", response)
            self.assertIn(b'"error":"service_busy"', response)
        finally:
            release.set()
            if first is not None:
                first.close()
            if second is not None:
                second.close()
            server.shutdown()
            server.server_close()
            server_thread.join(timeout=2)
        self.assertFalse(server_thread.is_alive())

    def test_render_concurrency_is_bounded(self) -> None:
        document = base_config()
        document["requireKnownDevice"] = False
        document["devices"] = []
        config = provisioning.parse_config(document, {})

        class Store:
            @staticmethod
            def load() -> provisioning.ProvisioningConfig:
                return config

        class BlockingRenderer:
            def __init__(self) -> None:
                self.started = threading.Event()
                self.release = threading.Event()

            def render(self, _assignment: provisioning.Assignment) -> bytes:
                self.started.set()
                self.release.wait(timeout=5)
                return b"<unattend/>"

        renderer = BlockingRenderer()
        service = provisioning.ProvisioningService(
            Store(),  # type: ignore[arg-type]
            renderer,  # type: ignore[arg-type]
            provisioning.TokenAuthorizer(None),
            max_concurrent_renders=1,
            render_queue_timeout_seconds=0.01,
        )
        first = threading.Thread(
            target=service.render,
            args=({"serial": "first-device"}, None),
            daemon=True,
        )
        first.start()
        self.assertTrue(renderer.started.wait(timeout=2))
        try:
            with self.assertRaises(provisioning.ServiceBusy):
                service.render({"serial": "second-device"}, None)
        finally:
            renderer.release.set()
            first.join(timeout=2)
        self.assertFalse(first.is_alive())


class RendererAndHttpTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        configured = os.environ.get("WIMFORGE_TEST_CLI", "")
        cls.cli_path = Path(configured) if configured else None

    def setUp(self) -> None:
        if self.cli_path is None or not self.cli_path.is_file():
            self.skipTest("WIMFORGE_TEST_CLI does not name a built WimForgeCli")
        self.temporary = tempfile.TemporaryDirectory(prefix="wimforge-provisioning-test-")
        self.root = Path(self.temporary.name)
        self.config_path = self.root / "provisioning.json"
        self.profile_root = self.root / "profiles"
        self.profile_root.mkdir()
        self.write_config(base_config())
        self.renderer = provisioning.CliRenderer([str(self.cli_path)], self.profile_root)
        self.store = provisioning.ConfigStore(self.config_path, {})

    def tearDown(self) -> None:
        if hasattr(self, "temporary"):
            self.temporary.cleanup()

    def write_config(self, document: dict) -> None:
        self.config_path.write_text(
            json.dumps(document, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )

    @staticmethod
    def xml_values(data: bytes, name: str) -> list[str]:
        namespace = "{urn:schemas-microsoft-com:unattend}"
        root = ET.fromstring(data)
        return [element.text or "" for element in root.iter(namespace + name)]

    def test_cli_renderer_applies_fixed_name_and_other_settings(self) -> None:
        self.renderer.check()
        assignment = self.store.load().resolve(
            provisioning.parse_request({"uuid": UUID_ONE})
        )
        data = self.renderer.render(assignment)
        self.assertEqual(self.xml_values(data, "ComputerName"), ["LAB-NODE-01"])
        self.assertEqual(self.xml_values(data, "TimeZone"), ["Eastern Standard Time"])
        self.assertEqual(self.xml_values(data, "UserLocale"), ["en-US"])
        self.assertEqual(self.xml_values(data, "RegisteredOwner"), ["Lab One"])
        namespace = "{urn:schemas-microsoft-com:unattend}"
        root = ET.fromstring(data)
        specialize_names = [
            child.text
            for settings in root.findall(namespace + "settings")
            if settings.attrib.get("pass") == "specialize"
            for component in settings.findall(namespace + "component")
            for child in component.findall(namespace + "ComputerName")
        ]
        self.assertEqual(specialize_names, ["LAB-NODE-01"])

    def test_custom_profile_settings_survive_device_render(self) -> None:
        profile_path = self.profile_root / "custom.json"
        self.renderer._run(  # Exercise the same canonical CLI template source.
            [
                "unattend",
                "template",
                "full",
                "--output",
                str(profile_path),
                "--format",
                "json",
            ]
        )
        profile = json.loads(profile_path.read_text(encoding="utf-8"))
        profile["settings"].append(
            {
                "pass": "specialize",
                "component": "Microsoft-Windows-Shell-Setup",
                "architecture": "amd64",
                "publicKeyToken": "31bf3856ad364e35",
                "language": "neutral",
                "versionScope": "nonSxS",
                "path": [
                    {"name": "OEMInformation", "attributes": {}},
                    {"name": "Manufacturer", "attributes": {}},
                ],
                "value": "Example Hardware Lab",
            }
        )
        profile_path.write_text(json.dumps(profile), encoding="utf-8")
        data = self.renderer.render(
            provisioning.Assignment("custom", "custom", "CUSTOM-NODE", {})
        )
        self.assertEqual(self.xml_values(data, "ComputerName"), ["CUSTOM-NODE"])
        self.assertEqual(self.xml_values(data, "Manufacturer"), ["Example Hardware Lab"])

    def test_duplicate_and_non_amd64_overrides_cannot_win(self) -> None:
        profile_path = self.profile_root / "duplicates.json"
        self.renderer._run(
            [
                "unattend",
                "template",
                "full",
                "--output",
                str(profile_path),
                "--format",
                "json",
            ]
        )
        profile = json.loads(profile_path.read_text(encoding="utf-8"))
        time_zone = next(
            setting
            for setting in profile["settings"]
            if setting["path"][-1]["name"] == "TimeZone"
        )
        stale_duplicate = json.loads(json.dumps(time_zone))
        stale_duplicate["value"] = "Pacific Standard Time"
        profile["settings"].append(stale_duplicate)
        arm64_duplicate = json.loads(json.dumps(time_zone))
        arm64_duplicate["architecture"] = "arm64"
        arm64_duplicate["value"] = "Tokyo Standard Time"
        profile["settings"].append(arm64_duplicate)
        profile_path.write_text(json.dumps(profile), encoding="utf-8")
        data = self.renderer.render(
            provisioning.Assignment(
                "duplicates",
                "duplicates",
                "DUPLICATE-PC",
                {"timeZone": "Eastern Standard Time"},
            )
        )
        values = self.xml_values(data, "TimeZone")
        self.assertTrue(values)
        self.assertTrue(all(value == "Eastern Standard Time" for value in values))

    def test_non_amd64_target_profile_is_rejected(self) -> None:
        profile_path = self.profile_root / "arm64-only.json"
        self.renderer._run(
            [
                "unattend",
                "template",
                "full",
                "--output",
                str(profile_path),
                "--format",
                "json",
            ]
        )
        profile = json.loads(profile_path.read_text(encoding="utf-8"))
        for setting in profile["settings"]:
            setting["architecture"] = "arm64"
        profile_path.write_text(json.dumps(profile), encoding="utf-8")
        document = base_config()
        document["devices"][0]["profile"] = "arm64-only"
        self.write_config(document)
        service = provisioning.ProvisioningService(
            self.store, self.renderer, provisioning.TokenAuthorizer(None)
        )
        with self.assertRaises(provisioning.RenderError):
            service.health()
        with self.assertRaises(provisioning.RenderError):
            self.renderer.render(
                provisioning.Assignment("arm", "arm64-only", "ARM-NODE", {})
            )

    def test_fixed_assignment_replaces_builtin_serial_rename(self) -> None:
        data = self.renderer.render(
            provisioning.Assignment(
                "ai-node", "builtin:ai-development", "AI-NODE-01", {}
            )
        )
        self.assertEqual(self.xml_values(data, "ComputerName"), ["AI-NODE-01"])
        self.assertNotIn(b"Get-CimInstance Win32_BIOS", data)
        self.assertNotIn(b"WimForge serial-number computer name", data)

    def test_configuration_reloads_without_container_restart(self) -> None:
        first = self.store.load().resolve(provisioning.parse_request({"uuid": UUID_ONE}))
        first_xml = self.renderer.render(first)
        document = base_config()
        document["devices"][0]["computerName"] = "LAB-UPDATED"
        self.write_config(document)
        second = self.store.load().resolve(provisioning.parse_request({"uuid": UUID_ONE}))
        second_xml = self.renderer.render(second)
        self.assertEqual(self.xml_values(first_xml, "ComputerName"), ["LAB-NODE-01"])
        self.assertEqual(self.xml_values(second_xml, "ComputerName"), ["LAB-UPDATED"])

    def test_health_fails_when_a_referenced_custom_profile_is_missing(self) -> None:
        document = base_config()
        document["devices"][0]["profile"] = "missing-profile"
        self.write_config(document)
        service = provisioning.ProvisioningService(
            self.store, self.renderer, provisioning.TokenAuthorizer(None)
        )
        with self.assertRaises(provisioning.RenderError):
            service.health()

    def test_health_cli_validates_referenced_custom_profile(self) -> None:
        profile_path = self.profile_root / "invalid-pass.json"
        self.renderer._run(
            [
                "unattend",
                "template",
                "full",
                "--output",
                str(profile_path),
                "--format",
                "json",
            ]
        )
        profile = json.loads(profile_path.read_text(encoding="utf-8"))
        profile["settings"][0]["pass"] = "futurePass"
        profile_path.write_text(json.dumps(profile), encoding="utf-8")
        document = base_config()
        document["devices"][0]["profile"] = "invalid-pass"
        self.write_config(document)
        service = provisioning.ProvisioningService(
            self.store, self.renderer, provisioning.TokenAuthorizer(None)
        )
        with self.assertRaises(provisioning.RenderError):
            service.health()

    def test_sensitive_custom_profile_is_not_served(self) -> None:
        sensitive = {
            "schema": "wimforge.unattend",
            "version": 1,
            "name": "Sensitive",
            "description": "",
            "settings": [
                {
                    "pass": "oobeSystem",
                    "component": "Microsoft-Windows-Shell-Setup",
                    "architecture": "amd64",
                    "publicKeyToken": "31bf3856ad364e35",
                    "language": "neutral",
                    "versionScope": "nonSxS",
                    "path": [
                        {"name": "UserAccounts", "attributes": {}},
                        {"name": "AdministratorPassword", "attributes": {}},
                        {"name": "Value", "attributes": {}},
                    ],
                    "value": "do-not-serve-this",
                }
            ],
            "placement": {"mediaRoot": True},
            "computerName": {"mode": 0, "value": "", "serialPrefix": ""},
            "metadata": {},
        }
        (self.profile_root / "sensitive.json").write_text(
            json.dumps(sensitive), encoding="utf-8"
        )
        assignment = provisioning.Assignment("sensitive", "sensitive", "SAFE-NAME", {})
        with self.assertRaises(provisioning.RenderError):
            self.renderer.render(assignment)

    def start_http_server(self) -> tuple[provisioning.ProvisioningHttpServer, str, str]:
        token = "test-token-with-at-least-sixteen-characters"
        token_file = self.root / "token"
        token_file.write_text(token + "\n", encoding="utf-8")
        service = provisioning.ProvisioningService(
            self.store,
            self.renderer,
            provisioning.TokenAuthorizer(token_file),
        )
        service.check()
        server = provisioning.ProvisioningHttpServer(
            ("127.0.0.1", 0), provisioning.handler_for(service)
        )
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        self.addCleanup(server.server_close)
        self.addCleanup(server.shutdown)
        return server, f"http://127.0.0.1:{server.server_address[1]}", token

    @staticmethod
    def post(url: str, document: dict, token: str | None = None) -> urllib.response.addinfourl:
        headers = {"Content-Type": "application/json"}
        if token:
            headers["Authorization"] = f"Bearer {token}"
        request = urllib.request.Request(
            url,
            data=json.dumps(document).encode("utf-8"),
            headers=headers,
            method="POST",
        )
        return urllib.request.urlopen(request, timeout=30)

    def test_http_auth_assignment_and_fail_closed_behavior(self) -> None:
        _server, base_url, token = self.start_http_server()
        with urllib.request.urlopen(base_url + "/healthz", timeout=10) as response:
            self.assertEqual(response.status, 200)
        with self.assertRaises(urllib.error.HTTPError) as unauthorized:
            self.post(base_url + "/v1/unattend", {"uuid": UUID_ONE})
        try:
            self.assertEqual(unauthorized.exception.code, 401)
        finally:
            unauthorized.exception.close()
        with self.post(base_url + "/v1/unattend", {"uuid": UUID_ONE}, token) as response:
            data = response.read()
            self.assertEqual(response.status, 200)
            self.assertEqual(response.headers["X-WimForge-Assignment"], "node-one")
            self.assertEqual(self.xml_values(data, "ComputerName"), ["LAB-NODE-01"])
        with self.assertRaises(urllib.error.HTTPError) as unknown:
            self.post(
                base_url + "/v1/unattend",
                {"serial": "unassigned-device"},
                token,
            )
        try:
            self.assertEqual(unknown.exception.code, 404)
        finally:
            unknown.exception.close()

    def test_concurrent_requests_are_deterministic(self) -> None:
        _server, base_url, token = self.start_http_server()

        def render_once() -> tuple[bytes, str]:
            with self.post(
                base_url + "/v1/unattend", {"uuid": UUID_ONE}, token
            ) as response:
                return response.read(), response.headers["X-WimForge-SHA256"]

        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
            results = list(executor.map(lambda _index: render_once(), range(4)))
        self.assertTrue(all(data == results[0][0] for data, _digest in results))
        self.assertTrue(all(digest == results[0][1] for _data, digest in results))
        self.assertEqual(hashlib.sha256(results[0][0]).hexdigest(), results[0][1])


if __name__ == "__main__":
    unittest.main(verbosity=2)
