from pathlib import Path

def construct_file_queue(srcs_path):
    files = [f for f in srcs_path.iterdir() if f.is_file()]
    out = []
    for file in files:
        with open(file, "r") as f:
            first_line = f.readline().strip()
            if(first_line.startswith("/*") and ("," in first_line)):
                clean_line = first_line.removeprefix("/*").strip()
                parts = [p.strip() for p in clean_line.split(",")]
                values = {}
                for part in parts:
                    key, val = part.split("=", 1)
                    values[key.strip()] = int(val)
                if "Priority" in values:
                    priority = values.pop("Priority")
                    out.append((priority, file, values))

    out.sort(key=lambda x: x[0], reverse=True)
    return out

def append_banner(dest_file, content):
    content_len = len(content)
    banner_max_size = 75 - (3 * 2) # 3 is the len of `/* ` and ` */`
    minus_left = max((banner_max_size - content_len) // 2, 0)
    minus_right = max(banner_max_size - content_len - minus_left, 0)
    dest_file.write("/* " + "-" * minus_left + content + "-" * minus_right + " */" + "\n\n")

def construct_headers_list(file_lists):
    system_headers = set()
    handles = []
    for _, fname, attrib in file_lists:
        f = open(fname, "r")  # keep open, caller must close later

        # skip until FileContentStart
        line_i = 1
        for _ in range(attrib["FileContentStart"] - 1):
            f.readline()
            line_i += 1
            # print(f.readline(), end="")
        # print(f.readline())
        while True:
            pos = f.tell()
            line = f.readline()
            if not line:
                break  # EOF
            line_i += 1
            
            stripped = line.strip("\n")
            if not stripped:
                continue
            if stripped and not stripped.startswith("#include"):
                f.seek(pos)  # rewind to before this line
                line_i -= 1
                break
            
            header_name = stripped.removeprefix("#include").strip()
            if(header_name.startswith("<")):
                system_headers.add(header_name)
        handles.append((f, line_i))
        
    return (system_headers, handles)

if __name__ == '__main__':
    repo_path = (Path(__file__).resolve().parent / "..").resolve()
    dest_file = open("aparse_single.h", "w")

    # Append original header
    append_headers = {"include/aparse.h"}
    for header_path in append_headers:
        with open(repo_path / header_path, "r") as f:
            for line in f:
                dest_file.write(line)
    dest_file.write("\n\n#ifdef APARSE_IMPLEMENTATION\n\n")
    
    file_lists = construct_file_queue(repo_path / "src")
    headers = construct_headers_list(file_lists)
    allowed_headers = {Path(p).name for p in append_headers}

    append_banner(dest_file, "Headers BEGIN")
    for line in headers[0]:
        dest_file.write("#include " + line + "\n")
    dest_file.write("\n")
    append_banner(dest_file, "Headers END")

    for i, pair in enumerate(headers[1]):
        fname = file_lists[i][1].name
        handle = pair[0]
        content_start = pair[1]

        append_banner(dest_file, fname + " BEGIN")
        content_end = file_lists[i][2]["FileContentEnd"] if "FileContentEnd" in file_lists[i][2] else 2147483647 # A big number
        # print(str(i) + " " + str(file_lists[i][2]["FileContentStart"]) + " " + str(content_end))
        for line_i, line in enumerate(handle, start=content_start):
            if not line or line_i > content_end:
                break
            dest_file.write(line)
        handle.close()
        append_banner(dest_file, fname + " END")

    dest_file.write("\n#endif /* APARSE_IMPLEMENTATION */")
