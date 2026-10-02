#!/usr/bin/env python3
# ps5-homebrew-ui - Writes the design and theme galleries of the documentation.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fills the generated sections of README.md, docs/DESIGNS.md and docs/THEMES.md.

usage: tools/gen-docs.py <manifest.json>

The manifest comes from the app itself (HUI_MANIFEST=<file>
tools/host-snapshots.sh), so the galleries list exactly the designs and themes
that are compiled in, in switcher order, with their own descriptions. A
generated section sits between two marker comments and is replaced whole;
everything outside the markers is written by hand and left alone.
"""

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MEDIA = ROOT / "docs/media"


def replace_section(path, name, body):
    text = path.read_text(encoding="utf-8")
    pattern = re.compile(rf"(<!-- BEGIN:{name} -->\n).*?(<!-- END:{name} -->)", re.S)
    if not pattern.search(text):
        raise SystemExit(f"{path}: no generated section named {name}")
    text = pattern.sub(lambda m: m.group(1) + body.rstrip() + "\n" + m.group(2), text)
    path.write_text(text, encoding="utf-8", newline="\n")


def pictures(design):
    """The design's pictures: the entrance first, then its tour states."""
    folder = MEDIA / "designs"
    first = folder / f"{design}.jpg"
    rest = sorted(p for p in folder.glob(f"{design}-*.jpg"))
    return ([first] if first.exists() else []) + rest


def grid(cells, columns):
    rows = ["<table>"]
    for start in range(0, len(cells), columns):
        rows.append("<tr>")
        for cell in cells[start:start + columns]:
            rows.append(f'<td width="{100 // columns}%" valign="top">{cell}</td>')
        rows.append("</tr>")
    rows.append("</table>")
    return "\n".join(rows)


def readme_designs(designs, prefix):
    cells = []
    for index, d in enumerate(designs, 1):
        cells.append(
            f'<a href="{prefix}docs/DESIGNS.md#{d["id"]}">'
            f'<img src="{prefix}docs/media/designs/{d["id"]}.jpg" alt="{d["name"]}"></a><br>'
            f'<b>{index:02d} &middot; {d["name"]}</b><br><sub>{d["tagline"]}</sub>')
    return grid(cells, 2)


def readme_themes(themes, prefix):
    cells = []
    for index, t in enumerate(themes, 1):
        cells.append(
            f'<a href="{prefix}docs/THEMES.md#{t["id"]}">'
            f'<img src="{prefix}docs/media/themes/{t["id"]}.jpg" alt="{t["name"]}"></a><br>'
            f'<b>{index:02d} &middot; {t["name"]}</b><br><sub>{t["family"]}</sub>')
    return grid(cells, 3)


def designs_page(designs):
    out = []
    out.append("| # | Design | What it is | Sound set | Source |")
    out.append("| --- | --- | --- | --- | --- |")
    for index, d in enumerate(designs, 1):
        out.append(f'| {index:02d} | [{d["name"]}](#{d["id"]}) | {d["tagline"]} | {d["sounds"]} '
                   f'| [`{Path(d["source"]).name}`](../{d["source"]}) |')
    for index, d in enumerate(designs, 1):
        out.append("")
        out.append(f'<a id="{d["id"]}"></a>')
        out.append("")
        out.append(f'## {index:02d} &middot; {d["name"]}')
        out.append("")
        out.append(f'{d["tagline"]}.')
        out.append("")
        clip = MEDIA / "designs" / f'{d["id"]}.webp'
        if clip.exists():
            out.append(f'<img src="media/designs/{d["id"]}.webp" width="640" '
                       f'alt="{d["name"]} in motion">')
            out.append("")
        out.append("**What it demonstrates**")
        out.append("")
        for technique in d["techniques"]:
            out.append(f"- {technique}")
        out.append("")
        out.append(f'Source: [`{d["source"]}`](../{d["source"]}) &middot; '
                   f'tests: [`tests/unit/{d["id"]}_test.cpp`](../tests/unit/{d["id"]}_test.cpp) '
                   f'&middot; sound set: `{d["sounds"]}`')
        shots = pictures(d["id"])
        if shots:
            out.append("")
            cells = [f'<img src="media/designs/{p.name}" alt="{d["name"]}: {p.stem}">' for p in shots]
            out.append(grid(cells, 2))
    return "\n".join(out)


def themes_page(themes):
    out = []
    out.append("| # | Theme | Design language | Recipe |")
    out.append("| --- | --- | --- | --- |")
    for index, t in enumerate(themes, 1):
        out.append(f'| {index:02d} | [{t["name"]}](#{t["id"]}) | {t["family"]} | {t["summary"]} |')
    for index, t in enumerate(themes, 1):
        out.append("")
        out.append(f'<a id="{t["id"]}"></a>')
        out.append("")
        out.append(f'### {index:02d} &middot; {t["name"]}')
        out.append("")
        out.append(f'*{t["family"]}.* {t["summary"]}. Theme id `{t["id"]}`.')
        out.append("")
        still = f'<img src="media/themes/{t["id"]}.jpg" alt="{t["name"]}">'
        clip = MEDIA / "themes" / f'{t["id"]}.webp'
        moving = (f'<img src="media/themes/{t["id"]}.webp" alt="{t["name"]} in motion">'
                  if clip.exists() else "")
        out.append(grid([still, moving] if moving else [still], 2 if moving else 1))
    return "\n".join(out)


COMPONENT_GROUPS = [
    ("lists", "Lists"),
    ("collections", "Collections"),
    ("navigation", "Navigation"),
    ("overlays", "Overlays"),
    ("forms", "Forms"),
    ("indicators", "Indicators"),
]


def components_catalogue():
    """One block per group: its components (the level-two headings of its
    guide), a link to the guide, and the pictures of its gallery page."""
    out = []
    out.append("| Group | Components | Guide |")
    out.append("| --- | --- | --- |")
    names = {}
    for group, title in COMPONENT_GROUPS:
        guide = ROOT / "docs/components" / f"{group}.md"
        found = []
        if guide.exists():
            for line in guide.read_text(encoding="utf-8").splitlines():
                if line.startswith("## "):
                    # "## Avatar and AvatarStack" names two; prose headings
                    # ("## The gallery page") name none.
                    for name in line[3:].strip().split(" and "):
                        if re.fullmatch(r"[A-Z][A-Za-z]+", name):
                            found.append(name)
        names[group] = found
        listed = ", ".join(f"`{name}`" for name in found) or "(in progress)"
        out.append(f"| [{title}](#{group}) | {listed} | [components/{group}.md](components/{group}.md) |")
    for group, title in COMPONENT_GROUPS:
        folder = MEDIA / "designs"
        shots = [p for p in sorted(folder.glob(f"components-{group}*.jpg"))]
        out.append("")
        out.append(f'<a id="{group}"></a>')
        out.append("")
        out.append(f"### {title}")
        out.append("")
        if names[group]:
            out.append(", ".join(f"`ui::{name}`" for name in names[group]) +
                       f" &middot; [knobs, slots, events and cues](components/{group}.md)")
            out.append("")
        if shots:
            cells = [f'<img src="media/designs/{p.name}" alt="{title}: {p.stem}">' for p in shots]
            out.append(grid(cells, 2))
    themed = [p for p in sorted((MEDIA / "designs").glob("components-*.jpg"))
              if not any(p.stem.startswith(f"components-{g}") for g, _ in COMPONENT_GROUPS)]
    if themed:
        out.append("")
        out.append("### The same components in other themes")
        out.append("")
        cells = [f'<img src="media/designs/{p.name}" alt="{p.stem}">' for p in themed]
        out.append(grid(cells, 2))
    return "\n".join(out)


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    manifest = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
    designs, themes = manifest["designs"], manifest["themes"]
    replace_section(ROOT / "README.md", "designs", readme_designs(designs, ""))
    replace_section(ROOT / "README.md", "themes", readme_themes(themes, ""))
    replace_section(ROOT / "README.md", "counts",
                    f"**{len(designs)} designs** &middot; **{len(themes)} themes**")
    replace_section(ROOT / "docs/DESIGNS.md", "designs", designs_page(designs))
    replace_section(ROOT / "docs/THEMES.md", "themes", themes_page(themes))
    replace_section(ROOT / "docs/COMPONENTS.md", "components", components_catalogue())
    print(f"gen-docs: {len(designs)} designs, {len(themes)} themes")


if __name__ == "__main__":
    main()
