from pathlib import Path
from typing import TextIO
from datetime import UTC, datetime

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

OUTPUT_FILE = "aparse.h"
HEADER_GUARD = "APARSE_H"
HEADER_IMPLEMENTATION = "APARSE_IMPLEMENTATION"

REPO_DIR = (Path(__file__).resolve().parent / "..").resolve()
HEADER_DIR = REPO_DIR / "include"
HEADER_ENTRY = "aparse.h"
SOURCE_DIR = REPO_DIR / "src"
SOURCES = [ "aparse_list.c", "aparse.c" ]

def append_banner(dest, content):
    content_len = len(content)
    pad_total = BANNER_WIDTH - len("/*  */") - content_len
    left = max(pad_total // 2, 0)
    right = max(pad_total - left, 0)
    dest.write(f"/* {'-' * left}{content}{'-' * right} */\n\n")

def collect_headers(include_dir: Path, entry):
    header_queue = [include_dir / entry]
    visited = set()
    system_headers = set()

    while header_queue:
        path = header_queue.pop()
        if path in visited:
            continue
        visited.add(path)

        in_comment = False
        with path.open("r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if in_comment:
                    if "*/" in line:
                        in_comment = False
                    continue

                if line.startswith("/*"):
                    if "*/" not in line:
                        in_comment = True
                    continue

                if not line:
                    continue
                
                if line.startswith("#include"):
                    include_part = line.removeprefix("#include").strip()
                    if include_part.startswith('<') and include_part.endswith('>'):
                        path2 = include_dir / include_part[1:-1]
                        if path2.exists():
                            header_queue.append(path2)
                        else:
                            system_headers.add(include_part[1:-1])
                elif not line.startswith("#"):
                    break

    return sorted(visited, reverse=True), system_headers

def emit_implementation(dest: TextIO, path: Path):
    in_comment = False
    comment_count = 0
    found_include = False
    after_includes = False
    skipped_guard = False
    pending_ifndef = False

    prev_line: str | None = None

    with path.open("r", encoding="utf-8") as f:
        for line in f:
            stripped = line.strip()
            if comment_count == 1 and in_comment:
                if "*/" in stripped:
                    in_comment = False
                continue

            if comment_count < 1 and stripped.startswith("/*"):
                comment_count += 1
                if "*/" not in stripped:
                    in_comment = True
                continue

            if not skipped_guard:
                if pending_ifndef and stripped.startswith("#define"):
                    skipped_guard = True
                    continue
                if stripped.startswith("#ifndef"):
                    pending_ifndef = True
                    continue

            if not after_includes:
                if not stripped:
                    continue
                if stripped.startswith("#include"):
                    found_include = True
                    continue
                elif found_include:
                    after_includes = True

            if prev_line is not None:
                dest.write(prev_line)
            prev_line = line

    if prev_line is not None and \
            not (skipped_guard and prev_line.strip().startswith("#endif")):
        dest.write(prev_line)

if __name__ == "__main__":
    repo_path = (Path(__file__).resolve().parent / "..").resolve()
    include_dir = repo_path / "include"
    src_dir = repo_path / "src"

    output_path = Path(OUTPUT_FILE)

    header_queue, include_sysheaders = \
            collect_headers(include_dir, "aparse.h")
    
    source_queue, src_sysheaders = set(), set()
    for fname in SOURCES:
        queue, sysheaders = \
                collect_headers(SOURCE_DIR, fname)
        source_queue.update(queue)
        src_sysheaders.update(sysheaders)
        

    with output_path.open("w", encoding="utf-8") as dest:
        dest.write("/*\n")
        dest.write(MIT_LICENSE)
        dest.write("*/\n\n")

        dest.write(f"#ifndef {HEADER_GUARD}\n")
        dest.write(f"#define {HEADER_GUARD}\n\n")

        for header in include_sysheaders:
            dest.write(f"#include <{header}>\n")
        dest.write("\n")

        for path in header_queue:
            emit_implementation(dest, path)

        dest.write("\n\n")       
        dest.write(f"#ifdef {HEADER_IMPLEMENTATION}\n\n")
        
        for header in src_sysheaders:
            dest.write(f"#include <{header}>\n")
        dest.write("\n")

        for path in source_queue:
            append_banner(dest, f"{path} BEGIN")
            emit_implementation(dest, path)
            append_banner(dest, f"{path} END")

        dest.write(f"\n#endif /* {HEADER_IMPLEMENTATION} */\n")
        dest.write(f"#endif /* {HEADER_GUARD} */\n")
