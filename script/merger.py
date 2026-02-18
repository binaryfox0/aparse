from pathlib import Path

BANNER_WIDTH = 75
MAX_INT = 2_147_483_647


def append_banner(dest, content):
    content_len = len(content)
    pad_total = BANNER_WIDTH - len("/*  */") - content_len
    left = max(pad_total // 2, 0)
    right = max(pad_total - left, 0)
    dest.write(f"/* {'-' * left}{content}{'-' * right} */\n\n")


def construct_file_queue(src_dir: Path):
    entries = []

    for file in sorted(src_dir.iterdir()):
        if not file.is_file():
            continue

        with file.open("r", encoding="utf-8") as f:
            first_line = f.readline().strip()

        if not (first_line.startswith("/*") and "," in first_line):
            continue

        clean = first_line.removeprefix("/*").removesuffix("*/").strip()
        parts = [p.strip() for p in clean.split(",")]

        values = {}
        for part in parts:
            if "=" not in part:
                continue
            key, val = part.split("=", 1)
            values[key.strip()] = int(val.strip())

        if "Priority" not in values or "FileContentStart" not in values:
            continue

        priority = values.pop("Priority")
        entries.append((priority, file, values))

    entries.sort(key=lambda x: x[0], reverse=True)
    return entries


def collect_system_headers(file_queue):
    system_headers = set()

    for _, file, attrib in file_queue:
        with file.open("r", encoding="utf-8") as f:
            for _ in range(attrib["FileContentStart"] - 1):
                f.readline()

            while True:
                pos = f.tell()
                line = f.readline()
                if not line:
                    break

                stripped = line.strip()
                if not stripped:
                    continue

                if not stripped.startswith("#include"):
                    f.seek(pos)
                    break

                include_part = stripped.removeprefix("#include").strip()

                if include_part.startswith("<") and include_part.endswith(">"):
                    system_headers.add(include_part)

    return sorted(system_headers)


def write_implementation(dest, file_queue):
    for _, file, attrib in file_queue:
        macros = set()

        content_start = attrib["FileContentStart"]
        content_end = attrib.get("FileContentEnd", MAX_INT)

        append_banner(dest, f"{file.name} BEGIN")

        with file.open("r", encoding="utf-8") as f:
            for line_i, line in enumerate(f, start=1):

                if line_i < content_start:
                    continue
                if line_i > content_end:
                    break

                if line.startswith("#define "):
                    macro = line.split(maxsplit=2)[1].split("(")[0]
                    macros.add(macro)

                if line.startswith('#include "'):
                    continue

                dest.write(line)

        dest.write("\n")

        for m in sorted(macros):
            dest.write(f"#undef {m}\n")

        append_banner(dest, f"{file.name} END")


if __name__ == "__main__":
    repo_path = (Path(__file__).resolve().parent / "..").resolve()
    src_dir = repo_path / "src"

    output_path = Path("aparse_single.h")

    public_headers = [
        repo_path / "include/aparse_list.h",
        repo_path / "include/aparse.h",
    ]

    file_queue = construct_file_queue(src_dir)
    system_headers = collect_system_headers(file_queue)

    with output_path.open("w", encoding="utf-8") as dest:
        for header in public_headers:
            with header.open("r", encoding="utf-8") as f:
                for line in f:
                    stripped = line.strip()
                    if stripped.startswith('#include "'):
                        continue
                    if stripped == "#pragma once":
                        continue
                    dest.write(line)

        dest.write("\n\n")       
        dest.write("#ifdef APARSE_IMPLEMENTATION\n\n")

        append_banner(dest, "System Headers BEGIN")

        for header in system_headers:
            dest.write(f"#include {header}\n")
        dest.write("\n")
        append_banner(dest, "System Headers END")

        write_implementation(dest, file_queue)

        dest.write("\n#endif /* APARSE_IMPLEMENTATION */\n")
