"""Build-time compatibility between GitHub Wiki links and MkDocs links."""

from __future__ import annotations

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
WIKI_PAGES = {path.stem for path in (ROOT / "docs" / "wiki").glob("*.md")}
WIKI_LINK = re.compile(
    r"(?P<prefix>\]\()"
    r"(?P<page>[A-Za-z0-9_.-]+)"
    r"(?P<fragment>#[^)\s]+)?"
    r"(?P<suffix>\))"
)


def on_page_markdown(markdown: str, page, **kwargs) -> str:
    """Append .md to Wiki-style page slugs only for the Material site build."""
    if not page.file.src_uri.replace("\\", "/").startswith("wiki/"):
        return markdown

    def replace(match: re.Match[str]) -> str:
        page_name = match.group("page")
        if page_name not in WIKI_PAGES:
            return match.group(0)
        fragment = match.group("fragment") or ""
        return f"]({page_name}.md{fragment})"

    return WIKI_LINK.sub(replace, markdown)

