#!/usr/bin/env python3
"""Convert an offline copy of /golf/courses from v1 to v2 (Python 3.9+)."""

import argparse
import json
from pathlib import Path
import re
import sys


def slug(name):
    """CONTRACTS §5: ASCII alphanumerics, separators, then a 40-char limit."""
    return re.sub(r"[^A-Za-z0-9]+", "-", name, flags=re.ASCII).lower().strip("-")[:40]


def valid_text(value, limit, comma=False):
    return (isinstance(value, str) and 1 <= len(value.encode("utf-8")) <= limit
            and not any(c in value for c in "\0\r\n")
            and (not comma or "," not in value))


def numbers(value, count, low, high, unique=False):
    return (isinstance(value, list) and len(value) == count
            and all(type(n) is int and low <= n <= high for n in value)
            and (not unique or len(set(value)) == count))


def validate_v1(course):
    """Return a rejection reason, or None for a complete legacy course."""
    if not isinstance(course, dict) or type(course.get("v")) is not int or course["v"] != 1:
        return "expected a v1 course object"
    if not valid_text(course.get("name"), 39):
        return "name must be 1..39 UTF-8 bytes on one line"
    holes = course.get("holes")
    if type(holes) is not int or holes not in (9, 18):
        return "holes must be 9 or 18"
    if not numbers(course.get("par"), holes, 3, 6):
        return "par must contain exactly holes integers in 3..6"
    if "si" in course and not numbers(course["si"], holes, 1, holes, unique=True):
        return "si must contain exactly holes distinct integers in 1..holes"
    if "yards" in course and not numbers(course["yards"], holes, 1, 999):
        return "yards must contain exactly holes integers in 1..999"
    tee = course.get("tees", "")
    if tee != "" and not valid_text(tee, 11, comma=True):
        return "tees must be empty or 1..11 UTF-8 bytes without commas or newlines"
    return None


def split_nines(values):
    return [values[i:i + 9] for i in range(0, len(values), 9)]


def convert_group(rows):
    first = rows[0][1]
    for path, course in rows[1:]:
        for field in ("holes", "par", "si"):
            if course.get(field) != first.get(field):
                raise ValueError(f"{field} mismatch in {path.name}")
    nines = []
    for i, par in enumerate(split_nines(first["par"])):
        nine = {"name": ("Out", "In")[i], "par": par}
        if "si" in first:
            nine["si"] = first["si"][i * 9:(i + 1) * 9]
        nines.append(nine)
    tees = []
    for path, course in rows:
        label = course.get("tees", "")
        if not label:
            continue
        if any(tee["name"] == label for tee in tees):
            raise ValueError(f"duplicate tee {label!r} in {path.name}")
        tee = {"name": label}
        if "yards" in course:
            tee["yards"] = split_nines(course["yards"])
        tees.append(tee)
    if len(tees) > 6:
        raise ValueError("more than 6 tees")
    return {"v": 2, "name": first["name"], "nines": nines, "tees": tees}


def format_course(course):
    """Indent objects by two spaces, keeping each numeric array on one line."""
    def render(value, depth=0):
        indent = "  " * depth
        child_indent = "  " * (depth + 1)
        if isinstance(value, dict) and value:
            fields = [child_indent + json.dumps(key, ensure_ascii=False) + ": "
                      + render(item, depth + 1) for key, item in value.items()]
            return "{\n" + ",\n".join(fields) + "\n" + indent + "}"
        if isinstance(value, list) and value and not all(type(n) is int for n in value):
            fields = [child_indent + render(item, depth + 1) for item in value]
            return "[\n" + ",\n".join(fields) + "\n" + indent + "]"
        return json.dumps(value, ensure_ascii=False)

    return render(course) + "\n"


def convert_directory(in_dir, out_dir, force=False):
    groups = {}
    outputs = []
    failed = False
    for path in sorted(in_dir.glob("*.json"), key=lambda p: p.name):
        try:
            raw = path.read_bytes()
            course = json.loads(raw)
            if isinstance(course, dict) and type(course.get("v")) is int and course["v"] == 2:
                print(f"Copy {path.name}: already v2, unchanged")
                outputs.append((path.name, raw, course))
                continue
            reason = validate_v1(course)
            if reason:
                print(f"Skip {path.name}: {reason}")
                continue
            groups.setdefault(course["name"], []).append((path, course))
        except (OSError, ValueError, UnicodeError) as exc:
            print(f"Skip {path.name}: {exc}")

    for name, rows in groups.items():
        try:
            course = convert_group(rows)
            filename = slug(name) + ".json"
            if filename == ".json":
                raise ValueError("name has no ASCII alphanumerics for an output slug")
            outputs.append((filename, format_course(course).encode("utf-8"), course))
        except ValueError as exc:
            print(f"Error {name}: {exc}")
            failed = True

    # Detect collisions before writing, including v2 passthrough filenames.
    counts = {}
    for filename, _, _ in outputs:
        counts[filename] = counts.get(filename, 0) + 1
    out_dir.mkdir(parents=True, exist_ok=True)
    for filename, raw, course in outputs:
        if counts[filename] > 1:
            print(f"Error {filename}: multiple courses target this filename; nothing written")
            failed = True
            continue
        destination = out_dir / filename
        try:
            with destination.open("wb" if force else "xb") as output:
                output.write(raw)
            nines = course.get("nines")
            tees = course.get("tees")
            print(f"{course.get('name', filename)}: "
                  f"{len(nines) if isinstance(nines, list) else 0} nines, "
                  f"{len(tees) if isinstance(tees, list) else 0} tees -> {filename}")
        except FileExistsError:
            print(f"Error {filename}: output exists; use --force to overwrite")
            failed = True
        except OSError as exc:
            print(f"Error {filename}: {exc}")
            failed = True
    return 1 if failed else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("in_dir", type=Path)
    parser.add_argument("out_dir", type=Path)
    parser.add_argument("--force", action="store_true", help="overwrite existing output files")
    args = parser.parse_args()
    if not args.in_dir.is_dir():
        parser.error("in_dir must be an existing directory")
    if args.in_dir.resolve() == args.out_dir.resolve():
        parser.error("use a separate output directory to preserve your originals")
    try:
        return convert_directory(args.in_dir, args.out_dir, args.force)
    except OSError as exc:
        parser.exit(1, f"Error: {exc}\n")


if __name__ == "__main__":
    sys.exit(main())
