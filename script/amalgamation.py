#!/usr/bin/env python3

import argparse
from pathlib import Path
import re
from typing import TextIO
from datetime import UTC, datetime
import subprocess

BANNER_WIDTH = 75

MIT_LICENSE = f"""MIT License

Copyright (c) {datetime.now(UTC).year} binaryfox0

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
"""

HEADER_GUARD = "APARSE_H"
HEADER_IMPLEMENTATION = "APARSE_IMPLEMENTATION"

REPO_DIR = (Path(__file__).resolve().parent / "..").resolve()
HEADER_DIR = REPO_DIR / "include"
HEADER_ENTRY = "aparse.h"
SOURCE_DIR = REPO_DIR / "src"
SOURCES = [
    "aparse.c",
    "aparse_parse.c",
    "aparse_help.c",
    "aparse_list.c",
]

def get_git_commit():
    try:
        result = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            check=True,
        )
        return result.stdout.strip()
    except subprocess.CalledProcessError:
        return "unknown"

def append_banner(dest, content):
    content_len = len(content)
    pad_total = BANNER_WIDTH - len("/*  */") - content_len
    left = max(pad_total // 2, 0)
    right = max(pad_total - left, 0)
    dest.write(f"/* {'-' * left}{content}{'-' * right} */\n\n")

def resolve_project_include(including_path: Path, include_name: str):
    for candidate in (
            including_path.parent / include_name,
            HEADER_DIR / include_name,
            SOURCE_DIR / include_name):
        if candidate.is_file():
            return candidate.resolve()
    return None

def project_includes(path: Path):
    in_comment = False
    with path.open("r", encoding="utf-8") as source:
        for line in source:
            stripped = line.strip()
            if in_comment:
                if "*/" in stripped:
                    in_comment = False
                continue
            if stripped.startswith("/*"):
                if "*/" not in stripped:
                    in_comment = True
                continue
            match = re.match(r"#\s*include\s*[<\"]([^>\"]+)[>\"]", stripped)
            if not match:
                continue
            included = resolve_project_include(path, match.group(1))
            if included:
                yield included

def collect_headers(include_dir: Path, entry):
    ordered = []
    visited = set()

    def visit(path: Path):
        path = path.resolve()
        if path in visited:
            return
        visited.add(path)
        for dependency in project_includes(path):
            visit(dependency)
        ordered.append(path)

    visit(include_dir / entry)
    return ordered

def strip_leading_comment(lines):
    start = next((i for i, line in enumerate(lines) if line.strip()), None)
    if start is None or not lines[start].lstrip().startswith("/*"):
        return lines
    end = next((i for i in range(start, len(lines)) if "*/" in lines[i]), None)
    if end is None:
        return lines
    return lines[:start] + lines[end + 1:]

def strip_outer_guard(lines):
    meaningful = [i for i, line in enumerate(lines) if line.strip()]
    if len(meaningful) < 3:
        return lines

    first, second = meaningful[:2]
    guard = re.fullmatch(r"#\s*ifndef\s+(\w+)", lines[first].strip())
    define = re.fullmatch(r"#\s*define\s+(\w+)(?:\s+.*)?", lines[second].strip())
    if not guard or not define or guard.group(1) != define.group(1):
        return lines

    lines[first] = ""
    lines[second] = ""
    last = meaningful[-1]
    if re.match(r"#\s*endif\b", lines[last].strip()):
        lines[last] = ""
    return lines

def emit_implementation(dest: TextIO, path: Path):
    lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
    lines = strip_outer_guard(strip_leading_comment(lines))

    for line in lines:
        stripped = line.strip()
        match = re.match(r"#\s*include\s*[<\"]([^>\"]+)[>\"]", stripped)
        if match and resolve_project_include(path, match.group(1)):
            continue
        dest.write(line)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
            "-o", "--output", 
            type=str, default="aparse.h", 
            help="Path to amalgamation header")
    args = parser.parse_args()

    header_queue = collect_headers(HEADER_DIR, HEADER_ENTRY)
    source_queue = [SOURCE_DIR / "aparse_internal.h"] + [
        SOURCE_DIR / fname for fname in SOURCES
    ]

    with Path(args.output).open("w", encoding="utf-8") as dest:
        dest.write("/*\n")
        dest.write(MIT_LICENSE)
        dest.write("*/\n\n")

        dest.write(f"/* Source commit: {get_git_commit()} */\n\n")

        dest.write(f"#ifndef {HEADER_GUARD}\n")
        dest.write(f"#define {HEADER_GUARD}\n\n")

        for path in header_queue:
            emit_implementation(dest, path)

        dest.write("\n\n")       
        dest.write(f"#ifdef {HEADER_IMPLEMENTATION}\n\n")
        
        for path in source_queue:
            append_banner(dest, f"{path} BEGIN")
            emit_implementation(dest, path)
            append_banner(dest, f"{path} END")

        dest.write(f"\n#endif /* {HEADER_IMPLEMENTATION} */\n")
        dest.write(f"#endif /* {HEADER_GUARD} */\n")
