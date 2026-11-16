"""Download the real flag graphics and embed them as a C++ header.

Flags come from flagcdn.com (public domain, sourced from Wikimedia Commons) so
they are the actual national flags instead of something hand drawn.
"""

import io, os, struct, urllib.request

OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".flags")
OUT_HEADER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "gui", "ui", "flags.h")

# order must match i18n::Lang: zh-CN, zh-TW, en, ja, ko, ru, es, fr, de, pt
FLAGS = [
    ("CN", "cn", "china"),
    ("HK", "hk", "hong kong"),
    ("US", "us", "usa"),
    ("JP", "jp", "japan"),
    ("KR", "kr", "south korea"),
    ("RU", "ru", "russia"),
    ("ES", "es", "spain"),
    ("FR", "fr", "france"),
    ("DE", "de", "germany"),
    ("BR", "br", "brazil"),
]

os.makedirs(OUT_DIR, exist_ok=True)

blobs = {}
for code, slug, name in FLAGS:
    path = os.path.join(OUT_DIR, f"{slug}.png")
    if not os.path.exists(path):
        url = f"https://flagcdn.com/w160/{slug}.png"
        print("downloading", url)
        req = urllib.request.Request(url, headers={"User-Agent": "blur-ui-build"})
        with urllib.request.urlopen(req, timeout=60) as resp:
            io.open(path, "wb").write(resp.read())
    data = io.open(path, "rb").read()
    blobs[code] = data
    print(f"{code} ({slug}, {name}): {len(data)} bytes")

lines = [
    "#pragma once",
    "",
    "#include <array>",
    "#include <cstddef>",
    "",
    "// Real national flag graphics, public domain (flagcdn.com, sourced from",
    "// Wikimedia Commons). Embedded so the exe needs no extra asset files.",
    "namespace flags {",
]

for code, slug, name in FLAGS:
    data = blobs[code]
    lines.append(f"\t// {name}")
    lines.append(f"\tinline constexpr std::array<unsigned char, {len(data)}> {code} = {{")
    for i in range(0, len(data), 32):
        lines.append("\t\t" + ",".join(str(b) for b in data[i:i + 32]) + ",")
    lines.append("\t};")
    lines.append("")

lines.append("\t// width / height of each flag png, so chips keep the real proportion")
lines.append("\tinline constexpr float ASPECT[] = {")
for code, slug, name in FLAGS:
    data = blobs[code]
    w, h = struct.unpack(">II", data[16:24])
    lines.append(f"\t\t{round(w / h, 4)}f, // {name} {w}x{h}")
lines.append("\t};")
lines.append("")
lines.append("\tstruct Image {")
lines.append("\t\tconst unsigned char* data;")
lines.append("\t\tstd::size_t size;")
lines.append("\t};")
lines.append("")
lines.append("\t// indexed by i18n::Lang")
lines.append("\tinline constexpr Image BY_LANG[] = {")
for code, slug, name in FLAGS:
    lines.append(f"\t\t{{ {code}.data(), {code}.size() }},")
lines.append("\t};")
lines.append("}")
lines.append("")

io.open(OUT_HEADER, "w", encoding="utf-8", newline="\n").write("\n".join(lines))
print("wrote", OUT_HEADER, os.path.getsize(OUT_HEADER), "bytes")
