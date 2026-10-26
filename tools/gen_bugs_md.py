"""Regenerate BUGS_<major>-<minor>.md (one per app version) from bugs/*.md, or create a new bug file.

Usage:
  python tools/gen_bugs_md.py [--severity | --status | --ver | --found | --fixed | --id] [--desc]
  python tools/gen_bugs_md.py --new            # creates bugs/BUG-<date>-<hex>.md, then regenerates

Default sort is --severity (High, Mid, Low), then id.
Each bug file has simple `key: value` frontmatter plus "## Description", "## Repro" (and optional
"## Notes") sections. IDs are unique (date + random hex), so two people adding bugs never collide.
"""
import argparse
import datetime
import re
import secrets
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUGS_DIR = ROOT / "bugs"
SEV = {"high": 0, "mid": 1, "low": 2}
COLS = ["id", "status", "severity", "app_version", "build_found", "build_fixed", "description", "repro"]
HEAD = ["ID", "Status", "Severity", "App ver", "Build found", "Build fixed", "Description", "Repro"]
SORT_FLAGS = {
    "severity": "severity",
    "status": "status",
    "ver": "app_version",
    "found": "build_found",
    "fixed": "build_fixed",
    "id": "id",
}
TEMPLATE = """---
id: {id}
status: Open
severity: Mid
app_version: {ver}
build_found:
build_fixed:
---
## Description

## Repro

"""


def current_app_version():
    settings = ROOT / "3OS_Unity_Sample_Project" / "ProjectSettings" / "ProjectSettings.asset"
    m = re.search(r"^\s*bundleVersion:\s*(\S+)", settings.read_text(encoding="utf-8", errors="ignore"), re.M)
    return m.group(1) if m else ""


def natural(s):
    return [(0, int(t)) if t.isdigit() else (1, t.lower()) for t in re.split(r"(\d+)", s)]


def sort_key(col):
    if col == "severity":
        return lambda r: (SEV.get(r["severity"].lower(), 9), natural(r["id"]))
    if col == "build_fixed":
        # unfixed (blank) bugs sort after fixed ones
        return lambda r: (r[col] == "", natural(r[col]), natural(r["id"]))
    return lambda r: (natural(r[col]), natural(r["id"]))


def sort_by(rows, col, desc=False):
    return sorted(rows, key=sort_key(col), reverse=desc)


def parse_bug(path):
    text = path.read_text(encoding="utf-8-sig").replace("\r\n", "\n")
    m = re.match(r"---\n(.*?)\n---\n(.*)", text, re.S)
    if not m:
        raise SystemExit(f"{path.name}: missing frontmatter")
    row = {}
    for line in m.group(1).split("\n"):
        if ":" in line:
            k, v = line.split(":", 1)
            row[k.strip()] = v.strip()
    sections = {}
    for name, body in re.findall(r"^## (.+?)\n(.*?)(?=^## |\Z)", m.group(2), re.S | re.M):
        sections[name.strip().lower()] = " ".join(body.split())
    row["description"] = sections.get("description", "")
    row["repro"] = sections.get("repro", "")
    for c in COLS:
        row.setdefault(c, "")
    return row


def load_bugs():
    rows = [parse_bug(p) for p in sorted(BUGS_DIR.glob("*.md"))]
    ids = [r["id"] for r in rows]
    dupes = {i for i in ids if ids.count(i) > 1}
    if dupes:
        raise SystemExit(f"duplicate bug ids: {', '.join(sorted(dupes))}")
    return rows


def new_bug():
    while True:
        bug_id = f"BUG-{datetime.date.today():%Y%m%d}-{secrets.token_hex(2)}"
        path = BUGS_DIR / f"{bug_id}.md"
        if not path.exists():
            path.write_text(TEMPLATE.format(id=bug_id, ver=current_app_version()), encoding="utf-8")
            print(f"created {path}")
            return


def main():
    ap = argparse.ArgumentParser()
    grp = ap.add_mutually_exclusive_group()
    for flag in SORT_FLAGS:
        grp.add_argument(f"--{flag}", action="store_true", help=f"sort by {SORT_FLAGS[flag]}")
    ap.add_argument("--desc", action="store_true", help="reverse the sort order")
    ap.add_argument("--new", action="store_true", help="create a new bug file with a unique id")
    args = ap.parse_args()
    col = next((SORT_FLAGS[f] for f in SORT_FLAGS if getattr(args, f)), "severity")

    if args.new:
        new_bug()
    groups = {}
    for r in load_bugs():
        m = re.match(r"(\d+)\.(\d+)", r["app_version"])
        if not m:
            raise SystemExit(f"{r['id']}: app_version '{r['app_version']}' must start with major.minor")
        groups.setdefault((m.group(1), m.group(2)), []).append(r)
    for (major, minor), bugs in groups.items():
        out = [
            f"# Bug Tracker: v{major}.{minor}",
            "",
            "Generated from the files in [`bugs/`](bugs/) by `tools/gen_bugs_md.py`. Edit those files, then re-run the script. Never hand-edit this table.",
            "",
            "| " + " | ".join(HEAD) + " |",
            "|" + "---|" * len(HEAD),
        ]
        for r in sort_by(bugs, col, args.desc):
            link = f"[{r['id']}](bugs/{r['id']}.md)"
            cells = [link] + [r[c].replace("|", "\\|") for c in COLS[1:]]
            out.append("| " + " | ".join(cells) + " |")
        (ROOT / f"BUGS_{major}-{minor}.md").write_text("\n".join(out) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
