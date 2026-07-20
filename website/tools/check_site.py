#!/usr/bin/env python3
"""Dependency-free website and canonical-publication checks for WimForge."""

from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit


EXPECTED_GENERATED_ASSETS = (
    "assets/brand/wimforge-logo.png",
    "assets/site/hero-forge.webp",
    "assets/site/vm-lab.webp",
    "assets/site/image-servicing.webp",
    "assets/site/unattended.webp",
    "assets/site/package-studio.webp",
    "assets/site/gpo-studio.webp",
    "assets/site/history-time-machine.webp",
    "assets/site/safety-guardrails.webp",
    "assets/site/automation-cli.webp",
    "assets/site/workflow-overview.webp",
)

CANONICAL_REPOSITORY = "Ding-Ding-Projects/WimForge"
CANONICAL_REPOSITORY_URL = f"https://github.com/{CANONICAL_REPOSITORY}"
CANONICAL_PAGES_ROOT = "https://ding-ding-projects.github.io/WimForge/"
CANONICAL_DOCS_ROOT = f"{CANONICAL_PAGES_ROOT}docs/"
CANONICAL_CONTAINER_IMAGE = "ghcr.io/ding-ding-projects/wimforge-provisioning"

# Keep the former owner split in this source file so the guard does not match
# its own definition while scanning every production-facing text file.
LEGACY_OWNER = "codingmachine" + "edge"
LEGACY_PUBLICATION_FRAGMENTS = (
    f"github.com/{LEGACY_OWNER}",
    f"raw.githubusercontent.com/{LEGACY_OWNER}",
    f"{LEGACY_OWNER}.github.io",
    f"ghcr.io/{LEGACY_OWNER}",
    f"{LEGACY_OWNER}/WimForge",
)
PUBLICATION_SCAN_DIRECTORIES = (
    ".github",
    "assets",
    "deploy",
    "docs",
    "installer",
    "qml",
    "scripts",
    "server",
    "src",
    "templates",
    "tests",
    "tools",
    "website",
)
PUBLICATION_TEXT_SUFFIXES = {
    ".cmake",
    ".cmd",
    ".cpp",
    ".css",
    ".h",
    ".html",
    ".iss",
    ".js",
    ".json",
    ".manifest",
    ".md",
    ".ps1",
    ".py",
    ".sh",
    ".toml",
    ".txt",
    ".xml",
    ".yaml",
    ".yml",
}
PUBLICATION_TEXT_FILENAMES = {"Dockerfile"}
REQUIRED_CANONICAL_REFERENCES = {
    ".github/workflows/container.yml": (CANONICAL_CONTAINER_IMAGE,),
    ".github/workflows/pages.yml": (CANONICAL_DOCS_ROOT,),
    "compose.yaml": (f"{CANONICAL_CONTAINER_IMAGE}:latest",),
    "installer/WimForge.iss": (
        '#define MyAppPublisher "Ding-Ding-Projects"',
        f'#define MyAppUrl "{CANONICAL_REPOSITORY_URL}"',
    ),
    "mkdocs.yml": (
        CANONICAL_DOCS_ROOT,
        CANONICAL_REPOSITORY_URL,
        CANONICAL_REPOSITORY,
    ),
    "scripts/bootstrap-build.ps1": (f"{CANONICAL_REPOSITORY_URL}.git",),
    "scripts/sync_wiki.py": (CANONICAL_REPOSITORY,),
    "src/cli_main.cpp": ("github.com/Ding-Ding-Projects",),
    "src/main.cpp": ("github.com/Ding-Ding-Projects",),
    "website/index.html": (
        CANONICAL_PAGES_ROOT,
        CANONICAL_DOCS_ROOT,
        f"{CANONICAL_DOCS_ROOT}wiki/Contributing/",
        CANONICAL_REPOSITORY_URL,
        f"{CANONICAL_REPOSITORY_URL}/tree/main/templates",
    ),
}


class SiteParser(HTMLParser):
    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.ids: list[str] = []
        self.references: list[tuple[str, str, int]] = []
        self.images: list[tuple[dict[str, str | None], int]] = []
        self.copy_targets: list[tuple[str, int]] = []
        self.title_count = 0
        self.description_count = 0
        self.lang: str | None = None

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        values = dict(attrs)
        if tag == "html":
            self.lang = values.get("lang")
        if tag == "title":
            self.title_count += 1
        if tag == "meta" and values.get("name", "").lower() == "description":
            self.description_count += 1
        if values.get("id"):
            self.ids.append(values["id"] or "")
        if tag in {"a", "link"} and values.get("href"):
            self.references.append((tag, values["href"] or "", self.getpos()[0]))
        if tag in {"img", "script", "source"} and values.get("src"):
            self.references.append((tag, values["src"] or "", self.getpos()[0]))
        if tag == "img":
            self.images.append((values, self.getpos()[0]))
        if tag == "button" and values.get("data-copy-target"):
            self.copy_targets.append((values["data-copy-target"] or "", self.getpos()[0]))


def is_external(value: str) -> bool:
    scheme = urlsplit(value).scheme.lower()
    return bool(scheme) or value.startswith("//")


def resolve_local(source_file: Path, repo_root: Path, value: str) -> Path | None:
    path = unquote(urlsplit(value).path)
    if not path or path.startswith("/"):
        return None
    if path.startswith("assets/"):
        return repo_root / path
    return source_file.parent / path


def check_html(html_file: Path, repo_root: Path) -> tuple[list[str], list[str]]:
    errors: list[str] = []
    warnings: list[str] = []
    parser = SiteParser()
    parser.feed(html_file.read_text(encoding="utf-8"))

    if parser.lang != "en":
        errors.append(f"{html_file}: expected html lang='en'")
    if parser.title_count != 1:
        errors.append(f"{html_file}: expected exactly one title element")
    if parser.description_count != 1:
        errors.append(f"{html_file}: expected exactly one meta description")

    duplicates = sorted({value for value in parser.ids if parser.ids.count(value) > 1})
    if duplicates:
        errors.append(f"{html_file}: duplicate ids: {', '.join(duplicates)}")

    known_ids = set(parser.ids)
    for target, line in parser.copy_targets:
        if target not in known_ids:
            errors.append(f"{html_file}:{line}: copy control targets missing id #{target}")

    for tag, value, line in parser.references:
        if value.startswith("#"):
            if value[1:] and value[1:] not in known_ids:
                errors.append(f"{html_file}:{line}: unresolved fragment {value}")
            continue
        if is_external(value) or value.startswith(("mailto:", "tel:", "data:")):
            continue
        local = resolve_local(html_file, repo_root, value)
        if local is not None and not local.exists():
            if local.as_posix().endswith(tuple(EXPECTED_GENERATED_ASSETS)):
                warnings.append(f"planned generated asset missing: {local.relative_to(repo_root)}")
            else:
                errors.append(f"{html_file}:{line}: missing local {tag} target {value}")

    for values, line in parser.images:
        if "alt" not in values:
            errors.append(f"{html_file}:{line}: image is missing alt text")
        if not values.get("width") and "data-lightbox-image" not in values:
            warnings.append(f"{html_file}:{line}: image has no intrinsic width")
        if not values.get("height") and "data-lightbox-image" not in values:
            warnings.append(f"{html_file}:{line}: image has no intrinsic height")

    return errors, warnings


def check_css(css_file: Path, repo_root: Path) -> list[str]:
    errors: list[str] = []
    text = css_file.read_text(encoding="utf-8")
    for value in re.findall(r"url\(\s*['\"]?([^)'\"]+)", text):
        if is_external(value) or value.startswith("data:"):
            continue
        local = resolve_local(css_file, repo_root, value)
        if local is not None and not local.exists():
            errors.append(f"{css_file}: missing CSS asset {value}")
    return errors


def read_png_dimensions(path: Path) -> tuple[int, int] | None:
    try:
        header = path.read_bytes()[:24]
    except OSError:
        return None
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        return None
    return struct.unpack(">II", header[16:24])


def check_manifest(manifest_file: Path, repo_root: Path) -> list[str]:
    errors: list[str] = []
    try:
        manifest = json.loads(manifest_file.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        return [f"manifest.webmanifest: {exc}"]

    icons = manifest.get("icons") if isinstance(manifest, dict) else None
    if not isinstance(icons, list) or not icons:
        return ["manifest.webmanifest: expected at least one icon"]

    for index, icon in enumerate(icons):
        if not isinstance(icon, dict):
            errors.append(f"manifest.webmanifest: icon {index} must be an object")
            continue
        source = icon.get("src")
        declared_sizes = icon.get("sizes")
        if not isinstance(source, str) or not source:
            errors.append(f"manifest.webmanifest: icon {index} is missing src")
            continue
        local = resolve_local(manifest_file, repo_root, source)
        if local is None or not local.is_file():
            errors.append(f"manifest.webmanifest: icon {index} target is missing: {source}")
            continue
        if local.suffix.lower() != ".png" or not isinstance(declared_sizes, str):
            continue
        actual_size = read_png_dimensions(local)
        if actual_size is None:
            errors.append(f"manifest.webmanifest: icon {index} is not a valid PNG: {source}")
            continue
        expected = f"{actual_size[0]}x{actual_size[1]}"
        if expected not in declared_sizes.split():
            errors.append(
                f"manifest.webmanifest: icon {index} declares {declared_sizes!r}, "
                f"but {source} is {expected}"
            )
    return errors


def iter_publication_text_files(repo_root: Path):
    candidates = [path for path in repo_root.iterdir() if path.is_file()]
    for directory in PUBLICATION_SCAN_DIRECTORIES:
        root = repo_root / directory
        if root.is_dir():
            candidates.extend(path for path in root.rglob("*") if path.is_file())

    for path in sorted(set(candidates)):
        relative = path.relative_to(repo_root)
        # Dated audits are append-only evidence of what was true at the time;
        # changing their former-owner references would falsify that record.
        if relative.parts[:2] == ("docs", "audits"):
            continue
        if (
            path.name not in PUBLICATION_TEXT_FILENAMES
            and path.suffix.lower() not in PUBLICATION_TEXT_SUFFIXES
        ):
            continue
        yield path


def check_publication_contract(repo_root: Path) -> list[str]:
    errors: list[str] = []
    for path in iter_publication_text_files(repo_root):
        try:
            lines = path.read_text(encoding="utf-8").splitlines()
        except (OSError, UnicodeDecodeError) as exc:
            errors.append(f"{path}: unable to inspect publication references: {exc}")
            continue
        for line_number, line in enumerate(lines, start=1):
            comparable_line = line.casefold()
            for fragment in LEGACY_PUBLICATION_FRAGMENTS:
                if fragment.casefold() in comparable_line:
                    errors.append(
                        f"{path}:{line_number}: stale production publication reference {fragment!r}"
                    )
                    break

    for relative, required_fragments in REQUIRED_CANONICAL_REFERENCES.items():
        path = repo_root / relative
        try:
            text = path.read_text(encoding="utf-8")
        except OSError as exc:
            errors.append(f"{path}: unable to verify canonical publication reference: {exc}")
            continue
        for fragment in required_fragments:
            if fragment not in text:
                errors.append(f"{path}: missing canonical publication reference {fragment!r}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--require-assets", action="store_true", help="Fail when planned generated images are missing")
    args = parser.parse_args()
    repo_root = args.repo_root.resolve()
    site_root = repo_root / "website"

    errors: list[str] = []
    warnings: list[str] = []
    errors.extend(check_publication_contract(repo_root))
    for html_file in sorted(site_root.glob("*.html")):
        html_errors, html_warnings = check_html(html_file, repo_root)
        errors.extend(html_errors)
        warnings.extend(html_warnings)
    for css_file in sorted(site_root.glob("*.css")):
        errors.extend(check_css(css_file, repo_root))

    errors.extend(check_manifest(site_root / "manifest.webmanifest", repo_root))

    missing_assets = [path for path in EXPECTED_GENERATED_ASSETS if not (repo_root / path).is_file()]
    if args.require_assets and missing_assets:
        errors.extend(f"required generated asset missing: {path}" for path in missing_assets)

    for warning in sorted(set(warnings)):
        print(f"warning: {warning}")
    for error in errors:
        print(f"error: {error}", file=sys.stderr)

    if errors:
        print(f"site check failed with {len(errors)} error(s)", file=sys.stderr)
        return 1
    print(f"site check passed ({len(set(warnings))} warning(s), {len(EXPECTED_GENERATED_ASSETS) - len(missing_assets)} generated assets present)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
