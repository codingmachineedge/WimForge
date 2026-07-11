#!/usr/bin/env python3
"""Read-only Docker provisioning service for WimForge unattended profiles.

The service deliberately accepts only hardware identity.  Profile selection,
computer names, and the small allow-list of per-device setting overrides live
in an operator-owned configuration file.  The existing WimForge CLI remains
the canonical validator and XML renderer.
"""

from __future__ import annotations

import argparse
import contextlib
import dataclasses
import hashlib
import hmac
import json
import logging
import os
import re
import signal
import subprocess
import sys
import tempfile
import threading
import time
import xml.etree.ElementTree as ET
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any, Iterable, Mapping, Sequence


SCHEMA = "wimforge.provisioning"
SCHEMA_VERSION = 1
MAX_CONFIG_BYTES = 4 * 1024 * 1024
MAX_PROFILE_BYTES = 4 * 1024 * 1024
MAX_REQUEST_BYTES = 16 * 1024
MAX_RESPONSE_BYTES = 8 * 1024 * 1024
COMPUTER_NAME_RE = re.compile(r"^(?![0-9]+$)[A-Za-z0-9](?:[A-Za-z0-9-]{0,13}[A-Za-z0-9])?$")
PROFILE_NAME_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$")
DEVICE_ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$")
GENERIC_ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._:/-]{0,127}$")

LOG = logging.getLogger("wimforge.provisioning")


class ProvisioningError(RuntimeError):
    """Base error with a stable API status/code."""

    status = HTTPStatus.UNPROCESSABLE_ENTITY
    code = "provisioning_error"


class ConfigError(ProvisioningError):
    status = HTTPStatus.SERVICE_UNAVAILABLE
    code = "configuration_unavailable"


class InvalidRequest(ProvisioningError):
    status = HTTPStatus.BAD_REQUEST
    code = "invalid_request"


class Unauthorized(ProvisioningError):
    status = HTTPStatus.UNAUTHORIZED
    code = "unauthorized"


class UnknownDevice(ProvisioningError):
    status = HTTPStatus.NOT_FOUND
    code = "unknown_device"


class AmbiguousDevice(ProvisioningError):
    status = HTTPStatus.CONFLICT
    code = "ambiguous_device"


class ServiceBusy(ProvisioningError):
    status = HTTPStatus.TOO_MANY_REQUESTS
    code = "service_busy"


class RenderError(ProvisioningError):
    status = HTTPStatus.UNPROCESSABLE_ENTITY
    code = "render_failed"


def _reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON property: {key}")
        result[key] = value
    return result


def strict_json_loads(text: str) -> Any:
    try:
        return json.loads(text, object_pairs_hook=_reject_duplicate_keys)
    except (json.JSONDecodeError, UnicodeError, ValueError) as exc:
        raise InvalidRequest("JSON is invalid or contains duplicate properties") from exc


def _require_object(value: Any, context: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ConfigError(f"{context} must be an object")
    return value


def _reject_unknown(value: Mapping[str, Any], allowed: set[str], context: str) -> None:
    unknown = sorted(set(value) - allowed)
    if unknown:
        raise ConfigError(f"{context} has unknown properties: {', '.join(unknown)}")


def _require_bool(value: Any, context: str) -> bool:
    if type(value) is not bool:
        raise ConfigError(f"{context} must be true or false")
    return value


def _clean_text(value: Any, context: str, *, allow_empty: bool = False, maximum: int = 256) -> str:
    if not isinstance(value, str):
        raise ConfigError(f"{context} must be a string")
    result = value.strip()
    if not allow_empty and not result:
        raise ConfigError(f"{context} cannot be empty")
    if len(result) > maximum or any(ord(char) < 32 or ord(char) == 127 for char in result):
        raise ConfigError(f"{context} is too long or contains control characters")
    return result


def validate_computer_name(value: Any, context: str = "computerName") -> str:
    name = _clean_text(value, context, maximum=15)
    if name == "*":
        return name
    if not COMPUTER_NAME_RE.fullmatch(name) or len(name.encode("utf-8")) > 15:
        raise ConfigError(
            f"{context} must be '*', or a 1-15 byte ASCII name containing only "
            "letters, numbers, and interior hyphens, and not be numeric-only"
        )
    return name


PLACEHOLDER_IDENTIFIERS = {
    "0",
    "00000000-0000-0000-0000-000000000000",
    "default string",
    "none",
    "not applicable",
    "system serial number",
    "to be filled by o.e.m.",
    "unknown",
}


def normalize_generic_identifier(value: Any, context: str) -> str:
    identifier = _clean_text(value, context, maximum=128).strip("{} ").casefold()
    if identifier in PLACEHOLDER_IDENTIFIERS or not GENERIC_ID_RE.fullmatch(identifier):
        raise ConfigError(f"{context} is a placeholder or contains unsupported characters")
    return identifier


def normalize_mac(value: Any, context: str) -> str:
    source = _clean_text(value, context, maximum=32)
    compact = re.sub(r"[:-]", "", source).casefold()
    if not re.fullmatch(r"[0-9a-f]{12}", compact):
        raise ConfigError(f"{context} must be a 48-bit MAC address")
    first_octet = int(compact[:2], 16)
    if compact in {"000000000000", "ffffffffffff"} or first_octet & 1:
        raise ConfigError(f"{context} cannot be zero, broadcast, or multicast")
    return compact


SETTING_PATCHES: dict[str, tuple[str, str, tuple[str, ...]]] = {
    "registeredOwner": (
        "specialize",
        "Microsoft-Windows-Shell-Setup",
        ("RegisteredOwner",),
    ),
    "registeredOrganization": (
        "specialize",
        "Microsoft-Windows-Shell-Setup",
        ("RegisteredOrganization",),
    ),
    "timeZone": (
        "specialize",
        "Microsoft-Windows-Shell-Setup",
        ("TimeZone",),
    ),
    "inputLocale": (
        "windowsPE",
        "Microsoft-Windows-International-Core-WinPE",
        ("InputLocale",),
    ),
    "systemLocale": (
        "windowsPE",
        "Microsoft-Windows-International-Core-WinPE",
        ("SystemLocale",),
    ),
    "uiLanguage": (
        "windowsPE",
        "Microsoft-Windows-International-Core-WinPE",
        ("UILanguage",),
    ),
    "userLocale": (
        "windowsPE",
        "Microsoft-Windows-International-Core-WinPE",
        ("UserLocale",),
    ),
    "dynamicUpdate": (
        "windowsPE",
        "Microsoft-Windows-Setup",
        ("DynamicUpdate", "Enable"),
    ),
    "hideEulaPage": (
        "oobeSystem",
        "Microsoft-Windows-Shell-Setup",
        ("OOBE", "HideEULAPage"),
    ),
    "hideWirelessSetupInOobe": (
        "oobeSystem",
        "Microsoft-Windows-Shell-Setup",
        ("OOBE", "HideWirelessSetupInOOBE"),
    ),
    "protectYourPC": (
        "oobeSystem",
        "Microsoft-Windows-Shell-Setup",
        ("OOBE", "ProtectYourPC"),
    ),
}

BOOLEAN_SETTINGS = {"dynamicUpdate", "hideEulaPage", "hideWirelessSetupInOobe"}
TEXT_SETTINGS = {
    "registeredOwner",
    "registeredOrganization",
    "timeZone",
    "inputLocale",
    "systemLocale",
    "uiLanguage",
    "userLocale",
}

ENV_SETTING_NAMES = {
    "registeredOwner": "WIMFORGE_DEFAULT_REGISTERED_OWNER",
    "registeredOrganization": "WIMFORGE_DEFAULT_REGISTERED_ORGANIZATION",
    "timeZone": "WIMFORGE_DEFAULT_TIME_ZONE",
    "inputLocale": "WIMFORGE_DEFAULT_INPUT_LOCALE",
    "systemLocale": "WIMFORGE_DEFAULT_SYSTEM_LOCALE",
    "uiLanguage": "WIMFORGE_DEFAULT_UI_LANGUAGE",
    "userLocale": "WIMFORGE_DEFAULT_USER_LOCALE",
    "dynamicUpdate": "WIMFORGE_DEFAULT_DYNAMIC_UPDATE",
    "hideEulaPage": "WIMFORGE_DEFAULT_HIDE_EULA_PAGE",
    "hideWirelessSetupInOobe": "WIMFORGE_DEFAULT_HIDE_WIRELESS_SETUP_IN_OOBE",
    "protectYourPC": "WIMFORGE_DEFAULT_PROTECT_YOUR_PC",
}


def validate_settings(value: Any, context: str) -> dict[str, str]:
    if value is None:
        return {}
    settings = _require_object(value, context)
    _reject_unknown(settings, set(SETTING_PATCHES), context)
    result: dict[str, str] = {}
    for key, raw in settings.items():
        item_context = f"{context}.{key}"
        if key in BOOLEAN_SETTINGS:
            result[key] = "true" if _require_bool(raw, item_context) else "false"
        elif key == "protectYourPC":
            if type(raw) is not int or raw not in {1, 2, 3}:
                raise ConfigError(f"{item_context} must be 1, 2, or 3")
            result[key] = str(raw)
        elif key in TEXT_SETTINGS:
            result[key] = _clean_text(
                raw,
                item_context,
                allow_empty=(key == "registeredOrganization"),
            )
    return result


def environment_setting_overrides(environment: Mapping[str, str]) -> dict[str, str]:
    result: dict[str, str] = {}
    for key, variable in ENV_SETTING_NAMES.items():
        if variable not in environment:
            continue
        raw = environment[variable].strip()
        if key in BOOLEAN_SETTINGS:
            lowered = raw.casefold()
            if lowered not in {"true", "false", "1", "0", "yes", "no"}:
                raise ConfigError(f"{variable} must be true or false")
            result[key] = "true" if lowered in {"true", "1", "yes"} else "false"
        elif key == "protectYourPC":
            if raw not in {"1", "2", "3"}:
                raise ConfigError(f"{variable} must be 1, 2, or 3")
            result[key] = raw
        else:
            result[key] = _clean_text(
                raw,
                variable,
                allow_empty=(key == "registeredOrganization"),
            )
    return result


def validate_profile_name(value: Any, context: str) -> str:
    profile = _clean_text(value, context, maximum=72)
    if profile in {"builtin:full", "builtin:ai-development"}:
        return profile
    if not PROFILE_NAME_RE.fullmatch(profile):
        raise ConfigError(f"{context} must name a built-in or a profile file stem")
    return profile


@dataclasses.dataclass(frozen=True)
class Assignment:
    assignment_id: str
    profile: str
    computer_name: str
    settings: Mapping[str, str]


@dataclasses.dataclass(frozen=True)
class RequestIdentity:
    uuid: str | None = None
    serial: str | None = None
    macs: tuple[str, ...] = ()


@dataclasses.dataclass(frozen=True)
class ProvisioningConfig:
    require_known_device: bool
    default_assignment: Assignment
    assignments: Mapping[str, Assignment]
    uuid_index: Mapping[str, str]
    serial_index: Mapping[str, str]
    mac_index: Mapping[str, str]

    def resolve(self, identity: RequestIdentity) -> Assignment:
        matches: set[str] = set()
        if identity.uuid and identity.uuid in self.uuid_index:
            matches.add(self.uuid_index[identity.uuid])
        if identity.serial and identity.serial in self.serial_index:
            matches.add(self.serial_index[identity.serial])
        for mac in identity.macs:
            if mac in self.mac_index:
                matches.add(self.mac_index[mac])
        if len(matches) > 1:
            raise AmbiguousDevice("supplied hardware identifiers resolve to different devices")
        if not matches:
            if self.require_known_device:
                raise UnknownDevice("no configured device matches the supplied hardware identifiers")
            return self.default_assignment
        return self.assignments[next(iter(matches))]


def _as_identifier_list(value: Any, context: str) -> list[Any]:
    if isinstance(value, str):
        return [value]
    if isinstance(value, list) and value:
        return value
    raise ConfigError(f"{context} must be a string or a non-empty string array")


def parse_config(document: Any, environment: Mapping[str, str] | None = None) -> ProvisioningConfig:
    environment = environment if environment is not None else os.environ
    root = _require_object(document, "configuration")
    _reject_unknown(root, {"schema", "version", "requireKnownDevice", "defaults", "devices"}, "configuration")
    if (
        root.get("schema") != SCHEMA
        or type(root.get("version")) is not int
        or root.get("version") != SCHEMA_VERSION
    ):
        raise ConfigError(f"configuration must use {SCHEMA} version {SCHEMA_VERSION}")
    require_known = _require_bool(root.get("requireKnownDevice", True), "requireKnownDevice")

    defaults = _require_object(root.get("defaults", {}), "defaults")
    _reject_unknown(defaults, {"profile", "computerName", "settings"}, "defaults")
    profile = validate_profile_name(
        environment.get("WIMFORGE_DEFAULT_PROFILE", defaults.get("profile", "builtin:full")),
        "defaults.profile",
    )
    computer_name = validate_computer_name(
        environment.get("WIMFORGE_DEFAULT_COMPUTER_NAME", defaults.get("computerName", "*")),
        "defaults.computerName",
    )
    default_settings = validate_settings(defaults.get("settings", {}), "defaults.settings")
    default_settings.update(environment_setting_overrides(environment))
    default_assignment = Assignment("defaults", profile, computer_name, default_settings)

    devices = root.get("devices", [])
    if not isinstance(devices, list):
        raise ConfigError("devices must be an array")
    assignments: dict[str, Assignment] = {}
    assignment_ids: dict[str, str] = {}
    indexes: dict[str, dict[str, str]] = {"uuid": {}, "serial": {}, "mac": {}}

    for index, raw_device in enumerate(devices):
        context = f"devices[{index}]"
        device = _require_object(raw_device, context)
        _reject_unknown(device, {"id", "match", "profile", "computerName", "settings"}, context)
        device_id = _clean_text(device.get("id"), f"{context}.id", maximum=64)
        folded_device_id = device_id.casefold()
        if not DEVICE_ID_RE.fullmatch(device_id) or folded_device_id in assignment_ids:
            raise ConfigError(f"{context}.id is invalid or duplicated")
        assignment_ids[folded_device_id] = device_id
        match = _require_object(device.get("match"), f"{context}.match")
        _reject_unknown(match, {"uuid", "serial", "mac"}, f"{context}.match")
        if not match:
            raise ConfigError(f"{context}.match must contain uuid, serial, or mac")

        resolved_profile = validate_profile_name(device.get("profile", profile), f"{context}.profile")
        resolved_name = validate_computer_name(
            device.get("computerName", computer_name), f"{context}.computerName"
        )
        resolved_settings = dict(default_settings)
        resolved_settings.update(validate_settings(device.get("settings", {}), f"{context}.settings"))
        assignments[device_id] = Assignment(
            device_id, resolved_profile, resolved_name, resolved_settings
        )

        for identity_type, raw_values in match.items():
            normalizer = normalize_mac if identity_type == "mac" else normalize_generic_identifier
            for raw_value in _as_identifier_list(raw_values, f"{context}.match.{identity_type}"):
                normalized = normalizer(raw_value, f"{context}.match.{identity_type}")
                owner = indexes[identity_type].get(normalized)
                if owner and owner != device_id:
                    raise ConfigError(
                        f"{context}.match.{identity_type} duplicates an identifier owned by {owner}"
                    )
                indexes[identity_type][normalized] = device_id

    computer_names: dict[str, str] = {}
    for device_id, assignment in assignments.items():
        if assignment.computer_name == "*":
            continue
        folded_name = assignment.computer_name.casefold()
        owner = computer_names.get(folded_name)
        if owner:
            raise ConfigError(
                f"devices {owner} and {device_id} resolve to the same fixed computer name"
            )
        computer_names[folded_name] = device_id
    if not require_known and default_assignment.computer_name != "*":
        raise ConfigError(
            "defaults.computerName must be '*' when unknown devices are allowed"
        )

    return ProvisioningConfig(
        require_known,
        default_assignment,
        assignments,
        indexes["uuid"],
        indexes["serial"],
        indexes["mac"],
    )


class ConfigStore:
    def __init__(self, path: Path, environment: Mapping[str, str] | None = None):
        self.path = path
        self.environment = dict(environment if environment is not None else os.environ)

    def load(self) -> ProvisioningConfig:
        try:
            size = self.path.stat().st_size
            if size <= 0 or size > MAX_CONFIG_BYTES:
                raise ConfigError("configuration file is empty or too large")
            text = self.path.read_text(encoding="utf-8")
            document = strict_json_loads(text)
            return parse_config(document, self.environment)
        except InvalidRequest as exc:
            raise ConfigError(str(exc)) from exc
        except UnicodeError as exc:
            raise ConfigError("configuration file is not valid UTF-8") from exc
        except OSError as exc:
            raise ConfigError("configuration file cannot be read") from exc


def parse_request(document: Any) -> RequestIdentity:
    if not isinstance(document, dict):
        raise InvalidRequest("request body must be a JSON object")
    unknown = sorted(set(document) - {"uuid", "serial", "macs"})
    if unknown:
        raise InvalidRequest(f"request has unknown properties: {', '.join(unknown)}")
    uuid: str | None = None
    serial: str | None = None
    macs: list[str] = []
    if "uuid" in document:
        try:
            uuid = normalize_generic_identifier(document["uuid"], "uuid")
        except ConfigError:
            uuid = None
    if "serial" in document:
        try:
            serial = normalize_generic_identifier(document["serial"], "serial")
        except ConfigError:
            serial = None
    if "macs" in document:
        raw_macs = document["macs"]
        if not isinstance(raw_macs, list) or len(raw_macs) > 32:
            raise InvalidRequest("macs must be an array containing at most 32 addresses")
        for value in raw_macs:
            try:
                macs.append(normalize_mac(value, "macs"))
            except ConfigError:
                continue
    if not uuid and not serial and not macs:
        raise InvalidRequest("request must contain at least one usable uuid, serial, or MAC address")
    return RequestIdentity(uuid, serial, tuple(dict.fromkeys(macs)))


def _path_names(setting: Mapping[str, Any]) -> tuple[str, ...]:
    result: list[str] = []
    path = setting.get("path")
    if not isinstance(path, list):
        return ()
    for segment in path:
        if not isinstance(segment, dict) or not isinstance(segment.get("name"), str):
            return ()
        result.append(segment["name"])
    return tuple(result)


def _contains_sensitive_values(profile: Mapping[str, Any]) -> bool:
    settings = profile.get("settings")
    if not isinstance(settings, list):
        return True
    for setting in settings:
        if not isinstance(setting, dict):
            return True
        folded = {part.casefold() for part in _path_names(setting)}
        if any(
            "password" in part
            or "credential" in part
            or part in {"productkey", "keymaterial", "accountdata", "wlanprofile"}
            for part in folded
        ):
            return True
    return False


def _profile_path_matches(setting: Mapping[str, Any], spec: tuple[str, str, tuple[str, ...]]) -> bool:
    setup_pass, component, path = spec
    if setting.get("pass") != setup_pass or setting.get("component") != component:
        return False
    segments = setting.get("path")
    if not isinstance(segments, list) or len(segments) != len(path):
        return False
    for segment, expected in zip(segments, path):
        if not isinstance(segment, dict) or segment.get("name") != expected:
            return False
        attributes = segment.get("attributes", {})
        if attributes not in ({}, None):
            return False
    return True


def apply_setting_patches(profile: dict[str, Any], patches: Mapping[str, str]) -> None:
    settings = profile.get("settings")
    if not isinstance(settings, list):
        raise RenderError("selected profile has no valid settings array")
    for key, value in patches.items():
        spec = SETTING_PATCHES[key]
        existing = [
            item
            for item in settings
            if isinstance(item, dict) and _profile_path_matches(item, spec)
        ]
        if existing:
            for item in existing:
                item["value"] = value
            if any(item.get("architecture", "amd64") == "amd64" for item in existing):
                continue
        setup_pass, component, path = spec
        settings.append(
            {
                "pass": setup_pass,
                "component": component,
                "architecture": "amd64",
                "publicKeyToken": "31bf3856ad364e35",
                "language": "neutral",
                "versionScope": "nonSxS",
                "path": [{"name": segment, "attributes": {}} for segment in path],
                "value": value,
            }
        )


def prepare_computer_name_settings(profile: dict[str, Any], computer_name: str) -> None:
    settings = profile.get("settings")
    if not isinstance(settings, list):
        raise RenderError("selected profile has no valid settings array")
    architectures = {
        setting.get("architecture", "amd64")
        for setting in settings
        if isinstance(setting, dict)
    }
    if architectures and "amd64" not in architectures:
        raise RenderError("Docker provisioning currently requires an amd64 target profile")
    for setting in settings:
        if not isinstance(setting, dict):
            continue
        if (
            setting.get("pass") in {"offlineServicing", "specialize"}
            and setting.get("component") == "Microsoft-Windows-Shell-Setup"
            and _path_names(setting) == ("ComputerName",)
        ):
            if setting.get("architecture", "amd64") != "amd64":
                raise RenderError(
                    "Docker provisioning cannot override a non-amd64 ComputerName setting"
                )
            setting["value"] = computer_name


def verify_rendered_assignment(data: bytes, assignment: Assignment) -> None:
    namespace = "{urn:schemas-microsoft-com:unattend}"
    try:
        root = ET.fromstring(data)
    except ET.ParseError as exc:
        raise RenderError("rendered output is not well-formed XML") from exc

    def values_for(
        spec: tuple[str, str, tuple[str, ...]], architecture: str | None = None
    ) -> list[str]:
        setup_pass, component_name, path = spec
        values: list[str] = []
        for settings in root.findall(namespace + "settings"):
            if settings.attrib.get("pass") != setup_pass:
                continue
            for component in settings.findall(namespace + "component"):
                if component.attrib.get("name") != component_name:
                    continue
                if (
                    architecture is not None
                    and component.attrib.get("processorArchitecture") != architecture
                ):
                    continue
                candidates = [component]
                for segment in path:
                    candidates = [
                        child
                        for parent in candidates
                        for child in parent.findall(namespace + segment)
                    ]
                values.extend((candidate.text or "") for candidate in candidates)
        return values

    computer_names: list[str] = []
    for settings in root.findall(namespace + "settings"):
        if settings.attrib.get("pass") != "specialize":
            continue
        for component in settings.findall(namespace + "component"):
            if (
                component.attrib.get("name") == "Microsoft-Windows-Shell-Setup"
                and component.attrib.get("processorArchitecture") == "amd64"
            ):
                computer_names.extend(
                    (element.text or "")
                    for element in component.findall(namespace + "ComputerName")
                )
    if not computer_names or any(value != assignment.computer_name for value in computer_names):
        raise RenderError("rendered answer file did not contain the assigned amd64 computer name")

    for key, expected in assignment.settings.items():
        values = values_for(SETTING_PATCHES[key])
        amd64_values = values_for(SETTING_PATCHES[key], "amd64")
        if (
            not values
            or not amd64_values
            or any(value != expected for value in values)
        ):
            raise RenderError(f"rendered answer file did not contain the assigned {key} value")


class CliRenderer:
    def __init__(
        self,
        command: Sequence[str],
        profile_root: Path,
        *,
        timeout_seconds: float = 20.0,
    ):
        if not command:
            raise ValueError("CLI command cannot be empty")
        self.command = tuple(command)
        self.profile_root = profile_root.resolve()
        self.timeout_seconds = timeout_seconds
        self.profile_validation_cache: dict[str, str] = {}
        self.profile_validation_lock = threading.Lock()

    def _run(self, arguments: Sequence[str]) -> None:
        try:
            result = subprocess.run(
                [*self.command, *arguments],
                stdin=subprocess.DEVNULL,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=self.timeout_seconds,
                check=False,
                text=True,
                encoding="utf-8",
                errors="replace",
            )
        except (OSError, subprocess.TimeoutExpired) as exc:
            raise RenderError("WimForge CLI could not complete the render") from exc
        if result.returncode != 0:
            detail = result.stderr.strip().splitlines()
            LOG.warning("WimForge CLI rejected a render: %s", detail[-1][:300] if detail else "exit failure")
            raise RenderError("WimForge CLI rejected the selected profile or settings")

    def check(self) -> None:
        self._run(["help"])

    def check_profile_reference(self, profile_name: str) -> None:
        with self.profile_validation_lock:
            candidate: Path | None = None
            if profile_name.startswith("builtin:"):
                cache_key = f"builtin:{profile_name}"
            else:
                candidate = (self.profile_root / f"{profile_name}.json").resolve()
                try:
                    candidate.relative_to(self.profile_root)
                    stat = candidate.stat()
                except (ValueError, OSError) as exc:
                    raise RenderError("selected profile cannot be read safely") from exc
                cache_key = (
                    f"file:{stat.st_dev}:{stat.st_ino}:{stat.st_size}:"
                    f"{stat.st_mtime_ns}:{stat.st_ctime_ns}"
                )
            if self.profile_validation_cache.get(profile_name) == cache_key:
                return

            with tempfile.TemporaryDirectory(prefix="wimforge-profile-check-") as directory:
                temporary = Path(directory)
                profile = self._load_profile(profile_name, temporary)
                canonical = json.dumps(
                    profile, ensure_ascii=False, sort_keys=True, separators=(",", ":")
                ).encode("utf-8")
                candidate = temporary / "validate.json"
                candidate.write_bytes(canonical)
                self._run(["unattend", "validate", str(candidate)])
                compatibility_profile = json.loads(canonical.decode("utf-8"))
                prepare_computer_name_settings(compatibility_profile, "*")
                self.profile_validation_cache[profile_name] = cache_key

    def _load_profile(self, profile_name: str, temporary: Path) -> dict[str, Any]:
        if profile_name.startswith("builtin:"):
            template = profile_name.removeprefix("builtin:")
            destination = temporary / "base.json"
            self._run(
                [
                    "unattend",
                    "template",
                    template,
                    "--output",
                    str(destination),
                    "--format",
                    "json",
                ]
            )
        else:
            candidate = (self.profile_root / f"{profile_name}.json").resolve()
            try:
                candidate.relative_to(self.profile_root)
            except ValueError as exc:
                raise RenderError("selected profile escapes the configured profile directory") from exc
            destination = candidate
        try:
            size = destination.stat().st_size
            if size <= 0 or size > MAX_PROFILE_BYTES:
                raise RenderError("selected profile is empty or too large")
            document = strict_json_loads(destination.read_text(encoding="utf-8"))
        except InvalidRequest as exc:
            raise RenderError("selected profile is not strict JSON") from exc
        except UnicodeError as exc:
            raise RenderError("selected profile is not valid UTF-8") from exc
        except OSError as exc:
            raise RenderError("selected profile cannot be read") from exc
        if not isinstance(document, dict):
            raise RenderError("selected profile must be a JSON object")
        if (
            document.get("schema") != "wimforge.unattend"
            or type(document.get("version")) is not int
            or document.get("version") != 1
        ):
            raise RenderError("selected profile has an unsupported schema or version")
        if _contains_sensitive_values(document):
            raise RenderError(
                "hosted profiles cannot contain known secret-bearing settings"
            )
        return document

    def render(self, assignment: Assignment) -> bytes:
        with tempfile.TemporaryDirectory(prefix="wimforge-provisioning-") as directory:
            temporary = Path(directory)
            profile = self._load_profile(assignment.profile, temporary)
            prepare_computer_name_settings(profile, assignment.computer_name)
            apply_setting_patches(profile, assignment.settings)
            patched = temporary / "patched.json"
            patched.write_text(
                json.dumps(profile, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
                encoding="utf-8",
            )
            output = temporary / "Autounattend.xml"
            mode = "random" if assignment.computer_name == "*" else "fixed"
            arguments = [
                "unattend",
                "computer-name",
                str(patched),
                "--mode",
                mode,
            ]
            if mode == "fixed":
                arguments.extend(["--value", assignment.computer_name])
            arguments.extend(["--output", str(output), "--format", "xml"])
            self._run(arguments)
            try:
                data = output.read_bytes()
            except OSError as exc:
                raise RenderError("WimForge CLI did not produce an answer file") from exc
            if not data or len(data) > MAX_RESPONSE_BYTES:
                raise RenderError("rendered answer file is empty or too large")
            if b"urn:schemas-microsoft-com:unattend" not in data:
                raise RenderError("rendered output is not a Windows answer file")
            verify_rendered_assignment(data, assignment)
            return data


class TokenAuthorizer:
    def __init__(self, token_file: Path | None):
        self.token_file = token_file

    def _token(self) -> str | None:
        if self.token_file is None:
            return None
        try:
            token = self.token_file.read_text(encoding="utf-8").strip()
        except UnicodeError as exc:
            raise ConfigError("API token file is not valid UTF-8") from exc
        except OSError as exc:
            raise ConfigError("API token file cannot be read") from exc
        if len(token) < 16 or len(token) > 4096 or any(ord(char) < 33 for char in token):
            raise ConfigError("API token must contain 16-4096 non-whitespace characters")
        return token

    def check_startup(self) -> None:
        self._token()

    def authorize(self, header: str | None) -> None:
        expected = self._token()
        if expected is None:
            return
        prefix = "Bearer "
        supplied = header[len(prefix) :] if header and header.startswith(prefix) else ""
        if not hmac.compare_digest(supplied, expected):
            raise Unauthorized("a valid bearer token is required")


class ProvisioningService:
    def __init__(
        self,
        config: ConfigStore,
        renderer: CliRenderer,
        authorizer: TokenAuthorizer,
        *,
        max_concurrent_renders: int = 4,
        render_queue_timeout_seconds: float = 5.0,
        health_cache_seconds: float = 2.0,
    ):
        if max_concurrent_renders < 1:
            raise ValueError("max_concurrent_renders must be positive")
        if not 0.1 <= health_cache_seconds <= 60:
            raise ValueError("health_cache_seconds must be between 0.1 and 60")
        self.config = config
        self.renderer = renderer
        self.authorizer = authorizer
        self.render_slots = threading.BoundedSemaphore(max_concurrent_renders)
        self.render_queue_timeout_seconds = render_queue_timeout_seconds
        self.health_cache_seconds = health_cache_seconds
        self.health_lock = threading.Lock()
        self.health_valid_until = 0.0

    def check(self) -> None:
        self.renderer.check()
        self.health(force=True)

    def health(self, *, force: bool = False) -> None:
        # Health is intentionally public for container orchestration. Serialize
        # and briefly cache the expensive config/profile/CLI validation so a
        # request burst cannot amplify into an unbounded subprocess workload.
        with self.health_lock:
            now = time.monotonic()
            if not force and now < self.health_valid_until:
                return
            config = self.config.load()
            profiles = {config.default_assignment.profile}
            profiles.update(assignment.profile for assignment in config.assignments.values())
            for profile in sorted(profiles):
                self.renderer.check_profile_reference(profile)
            self.authorizer.check_startup()
            self.health_valid_until = time.monotonic() + self.health_cache_seconds

    def render(self, request: Any, authorization: str | None) -> tuple[bytes, Assignment]:
        self.authorizer.authorize(authorization)
        identity = parse_request(request)
        assignment = self.config.load().resolve(identity)
        if not self.render_slots.acquire(timeout=self.render_queue_timeout_seconds):
            raise ServiceBusy("the bounded render queue is full; retry later")
        try:
            return self.renderer.render(assignment), assignment
        finally:
            self.render_slots.release()


class ProvisioningHttpServer(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True
    request_queue_size = 32

    def __init__(
        self,
        server_address: Any,
        request_handler_class: type[BaseHTTPRequestHandler],
        bind_and_activate: bool = True,
        *,
        max_worker_threads: int = 16,
    ):
        if not 1 <= max_worker_threads <= 128:
            raise ValueError("max_worker_threads must be between 1 and 128")
        self.worker_slots = threading.BoundedSemaphore(max_worker_threads)
        super().__init__(server_address, request_handler_class, bind_and_activate)

    def process_request(self, request: Any, client_address: Any) -> None:
        if not self.worker_slots.acquire(blocking=False):
            # Consume a bounded amount of an already-arrived request before
            # closing. On Windows, closing a socket with unread receive data
            # can turn the intended 503 into a TCP reset.
            with contextlib.suppress(OSError):
                request.settimeout(0.05)
                request.recv(MAX_REQUEST_BYTES)
            body = b'{"error":"service_busy","message":"HTTP worker limit reached"}\n'
            response = (
                b"HTTP/1.1 503 Service Unavailable\r\n"
                b"Content-Type: application/json; charset=utf-8\r\n"
                + f"Content-Length: {len(body)}\r\n".encode("ascii")
                + b"Cache-Control: no-store\r\nRetry-After: 5\r\nConnection: close\r\n\r\n"
                + body
            )
            with contextlib.suppress(OSError):
                request.sendall(response)
            self.shutdown_request(request)
            return
        try:
            super().process_request(request, client_address)
        except BaseException:
            self.worker_slots.release()
            raise

    def process_request_thread(self, request: Any, client_address: Any) -> None:
        try:
            super().process_request_thread(request, client_address)
        finally:
            self.worker_slots.release()

    def get_request(self) -> tuple[Any, Any]:
        connection, address = super().get_request()
        connection.settimeout(15)
        return connection, address


def handler_for(service: ProvisioningService) -> type[BaseHTTPRequestHandler]:
    class Handler(BaseHTTPRequestHandler):
        server_version = "WimForgeProvisioning/1"
        sys_version = ""

        def _json(self, status: HTTPStatus, document: Mapping[str, Any], headers: Mapping[str, str] | None = None) -> None:
            body = json.dumps(document, sort_keys=True, separators=(",", ":")).encode("utf-8") + b"\n"
            self.send_response(status.value)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            for key, value in (headers or {}).items():
                self.send_header(key, value)
            self.end_headers()
            if self.command != "HEAD":
                self.wfile.write(body)

        def _error(self, error: ProvisioningError) -> None:
            headers = (
                {"WWW-Authenticate": "Bearer"}
                if isinstance(error, Unauthorized)
                else {"Retry-After": "5"}
                if isinstance(error, ServiceBusy)
                else None
            )
            self._json(error.status, {"error": error.code, "message": str(error)}, headers)

        def do_HEAD(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
            self.do_GET()

        def do_GET(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
            if self.path == "/healthz":
                try:
                    service.health()
                    self._json(HTTPStatus.OK, {"status": "ok"})
                except ProvisioningError as exc:
                    LOG.error("health check failed: %s", exc)
                    self._json(
                        HTTPStatus.SERVICE_UNAVAILABLE,
                        {"error": ConfigError.code, "status": "unavailable"},
                    )
                return
            if self.path == "/":
                self._json(
                    HTTPStatus.OK,
                    {
                        "service": "WimForge Provisioning Service",
                        "version": 1,
                        "renderEndpoint": "/v1/unattend",
                    },
                )
                return
            self._json(HTTPStatus.NOT_FOUND, {"error": "not_found"})

        def do_POST(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
            if self.path != "/v1/unattend":
                self._json(HTTPStatus.NOT_FOUND, {"error": "not_found"})
                return
            try:
                service.authorizer.authorize(self.headers.get("Authorization"))
                content_type = self.headers.get_content_type()
                if content_type != "application/json":
                    self._json(
                        HTTPStatus.UNSUPPORTED_MEDIA_TYPE,
                        {"error": "unsupported_media_type", "message": "use application/json"},
                    )
                    return
                raw_length = self.headers.get("Content-Length")
                if raw_length is None:
                    self._json(
                        HTTPStatus.LENGTH_REQUIRED,
                        {"error": "length_required"},
                    )
                    return
                try:
                    length = int(raw_length)
                except ValueError as exc:
                    raise InvalidRequest("Content-Length is invalid") from exc
                if length <= 0 or length > MAX_REQUEST_BYTES:
                    self._json(
                        HTTPStatus.REQUEST_ENTITY_TOO_LARGE,
                        {"error": "request_too_large"},
                    )
                    return
                raw = self.rfile.read(length)
                try:
                    text = raw.decode("utf-8")
                except UnicodeDecodeError as exc:
                    raise InvalidRequest("request body must be UTF-8") from exc
                document = strict_json_loads(text)
                data, assignment = service.render(document, self.headers.get("Authorization"))
                digest = hashlib.sha256(data).hexdigest()
                filename = f"Autounattend-{assignment.assignment_id}.xml"
                self.send_response(HTTPStatus.OK.value)
                self.send_header("Content-Type", "application/xml; charset=utf-8")
                self.send_header("Content-Length", str(len(data)))
                self.send_header("Content-Disposition", f'attachment; filename="{filename}"')
                self.send_header("Cache-Control", "no-store")
                self.send_header("X-Content-Type-Options", "nosniff")
                self.send_header("X-WimForge-Assignment", assignment.assignment_id)
                self.send_header("X-WimForge-SHA256", digest)
                self.send_header("ETag", f'"sha256:{digest}"')
                self.end_headers()
                self.wfile.write(data)
            except ProvisioningError as exc:
                if not isinstance(exc, (UnknownDevice, AmbiguousDevice, Unauthorized, InvalidRequest)):
                    LOG.warning("request failed safely: %s", exc)
                self._error(exc)
            except (BrokenPipeError, ConnectionResetError):
                return
            except Exception:
                LOG.exception("unexpected request failure")
                with contextlib.suppress(BrokenPipeError, ConnectionResetError):
                    self._json(
                        HTTPStatus.INTERNAL_SERVER_ERROR,
                        {"error": "internal_error", "message": "request failed safely"},
                    )

        def log_message(self, format: str, *args: Any) -> None:
            LOG.info("client=%s %s", self.client_address[0], format % args)

    return Handler


def _service_from_environment(*, require_api_authentication: bool = True) -> ProvisioningService:
    config_path = Path(os.environ.get("WIMFORGE_CONFIG_FILE", "/config/provisioning.json"))
    profile_root = Path(os.environ.get("WIMFORGE_PROFILE_DIR", "/profiles"))
    cli_path = os.environ.get("WIMFORGE_CLI", "/usr/local/bin/WimForgeCli")
    token_value = os.environ.get("WIMFORGE_API_TOKEN_FILE", "").strip()
    token_file = Path(token_value) if token_value else None
    unauthenticated = os.environ.get("WIMFORGE_ALLOW_UNAUTHENTICATED", "").strip()
    if unauthenticated not in {"", "loopback-only"}:
        raise ConfigError(
            "WIMFORGE_ALLOW_UNAUTHENTICATED must be unset or exactly 'loopback-only'"
        )
    if token_file is not None and unauthenticated:
        raise ConfigError(
            "configure either an API token or explicit unauthenticated mode, not both"
        )
    if require_api_authentication and token_file is None and not unauthenticated:
        raise ConfigError(
            "HTTP service requires WIMFORGE_API_TOKEN_FILE; loopback-only deployments "
            "must explicitly set WIMFORGE_ALLOW_UNAUTHENTICATED=loopback-only"
        )
    try:
        timeout = float(os.environ.get("WIMFORGE_RENDER_TIMEOUT_SECONDS", "20"))
    except ValueError as exc:
        raise ConfigError("WIMFORGE_RENDER_TIMEOUT_SECONDS must be a number") from exc
    if not 1 <= timeout <= 120:
        raise ConfigError("WIMFORGE_RENDER_TIMEOUT_SECONDS must be between 1 and 120")
    try:
        max_concurrent = int(os.environ.get("WIMFORGE_MAX_CONCURRENT_RENDERS", "4"))
    except ValueError as exc:
        raise ConfigError("WIMFORGE_MAX_CONCURRENT_RENDERS must be an integer") from exc
    if not 1 <= max_concurrent <= 32:
        raise ConfigError("WIMFORGE_MAX_CONCURRENT_RENDERS must be between 1 and 32")
    return ProvisioningService(
        ConfigStore(config_path),
        CliRenderer([cli_path], profile_root, timeout_seconds=timeout),
        TokenAuthorizer(token_file),
        max_concurrent_renders=max_concurrent,
    )


def _write_output(path: str, data: bytes) -> None:
    if path == "-":
        sys.stdout.buffer.write(data)
        sys.stdout.buffer.flush()
        return
    destination = Path(path)
    destination.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(
        dir=destination.parent,
        prefix=f".{destination.name}.",
        suffix=".partial",
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, destination)
    finally:
        with contextlib.suppress(OSError):
            temporary.unlink()


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="WimForge Docker provisioning service")
    subparsers = parser.add_subparsers(dest="command")
    serve = subparsers.add_parser("serve", help="run the HTTP service")
    serve.add_argument("--listen", default=os.environ.get("WIMFORGE_LISTEN", "0.0.0.0"))
    serve.add_argument("--port", type=int, default=os.environ.get("WIMFORGE_PORT", "8080"))
    subparsers.add_parser("check", help="validate configuration, token, and CLI")
    render = subparsers.add_parser("render", help="render one answer file without HTTP")
    render.add_argument("--uuid")
    render.add_argument("--serial")
    render.add_argument("--mac", action="append", default=[])
    render.add_argument("--output", required=True)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    logging.basicConfig(
        level=os.environ.get("WIMFORGE_LOG_LEVEL", "INFO").upper(),
        format="%(asctime)s %(levelname)s %(name)s %(message)s",
    )
    parser = build_parser()
    arguments = parser.parse_args(argv)
    command = arguments.command or "serve"
    try:
        service = _service_from_environment(require_api_authentication=command != "render")
        service.check()
        if command == "check":
            print("WimForge provisioning configuration is valid.")
            return 0
        if command == "render":
            request: dict[str, Any] = {}
            if arguments.uuid:
                request["uuid"] = arguments.uuid
            if arguments.serial:
                request["serial"] = arguments.serial
            if arguments.mac:
                request["macs"] = arguments.mac
            identity = parse_request(request)
            assignment = service.config.load().resolve(identity)
            data = service.renderer.render(assignment)
            _write_output(arguments.output, data)
            return 0
        listen = getattr(arguments, "listen", os.environ.get("WIMFORGE_LISTEN", "0.0.0.0"))
        try:
            port = int(getattr(arguments, "port", os.environ.get("WIMFORGE_PORT", "8080")))
        except ValueError as exc:
            raise ConfigError("listen port must be an integer") from exc
        if not 1 <= port <= 65535:
            raise ConfigError("listen port must be between 1 and 65535")
        try:
            max_http_workers = int(os.environ.get("WIMFORGE_MAX_HTTP_WORKERS", "16"))
        except ValueError as exc:
            raise ConfigError("WIMFORGE_MAX_HTTP_WORKERS must be an integer") from exc
        if not 1 <= max_http_workers <= 128:
            raise ConfigError("WIMFORGE_MAX_HTTP_WORKERS must be between 1 and 128")
        server = ProvisioningHttpServer(
            (listen, port), handler_for(service), max_worker_threads=max_http_workers
        )
        LOG.info("listening on %s:%d", listen, port)

        def stop_server(_signum: int, _frame: Any) -> None:
            threading.Thread(target=server.shutdown, daemon=True).start()

        signal.signal(signal.SIGTERM, stop_server)
        signal.signal(signal.SIGINT, stop_server)
        try:
            server.serve_forever(poll_interval=0.5)
        finally:
            server.server_close()
        return 0
    except ProvisioningError as exc:
        LOG.error("%s", exc)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
