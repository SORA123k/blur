"""Build the embedded UI font.

Noto Sans SC covers latin/cyrillic/kana/CJK but not hangul, so the hangul part of
the subset is taken from Noto Sans KR and merged in. Both stay TrueType (glyf),
which is what stb_truetype handles best.

Output: src/gui/fonts/cn_sans.h (raw TTF bytes, AddFontFromMemoryTTF)
"""

import io, os, subprocess, sys

I18N = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "translations")
SC = os.path.join(I18N, "NotoSansSC.ttf")
KR = os.path.join(I18N, "NotoSansKR.ttf")
WORK = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".font-work")
OUT_HEADER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "gui", "fonts", "cn_sans.h")

os.makedirs(WORK, exist_ok=True)

# --- every character the ui can show ---------------------------------------
chars = set(chr(c) for c in range(0x20, 0x7F))
chars.update("简体中文繁體中文English日本語한국어РусскийEspañolFrançaisDeutschPortuguês")
chars.update("　、。，．・：；？！ー－（）「」『』【】〜…‥“”‘’《》〈〉·×÷±°§¶•●○■□▲▼◀▶★☆→←↑↓↔⇄")

for name in ["keys.tsv"] + [f"lang_{l}.tsv" for l in
                            ["zh-TW", "en", "ja", "ko", "ru", "es", "fr", "de", "pt"]]:
    for line in io.open(os.path.join(I18N, name), encoding="utf-8").read().splitlines():
        if "\t" in line:
            chars.update(line.split("\t", 1)[1])

hangul = {c for c in chars if 0xAC00 <= ord(c) <= 0xD7A3}
rest = chars - hangul
print(f"characters: {len(chars)} total, {len(hangul)} hangul, {len(rest)} from Noto Sans SC")

from fontTools.ttLib import TTFont
from fontTools.varLib import instancer


def static_font(path, out):
    font = TTFont(path)
    if "fvar" in font:
        instancer.instantiateVariableFont(font, {"wght": 400}, inplace=True)
    font.save(out)
    return out


def subset(src, text, out):
    tf = os.path.join(WORK, "chars_" + os.path.basename(out) + ".txt")
    io.open(tf, "w", encoding="utf-8").write("".join(sorted(text)))
    cmd = [sys.executable, "-m", "fontTools.subset", src, f"--text-file={tf}",
           f"--output-file={out}", "--layout-features=kern", "--no-hinting",
           "--drop-tables+=DSIG", "--name-IDs=*"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout[-1500:], r.stderr[-3000:])
        sys.exit(1)
    return out


sc_static = static_font(SC, os.path.join(WORK, "sc_static.ttf"))
sc_subset = subset(sc_static, rest, os.path.join(WORK, "sc_subset.ttf"))
print("sc subset:", os.path.getsize(sc_subset), "bytes")

parts = [sc_subset]
if hangul:
    kr_static = static_font(KR, os.path.join(WORK, "kr_static.ttf"))
    kr_subset = subset(kr_static, hangul, os.path.join(WORK, "kr_subset.ttf"))
    print("kr subset:", os.path.getsize(kr_subset), "bytes")
    parts.append(kr_subset)

final = os.path.join(WORK, "cn_sans_final.ttf")
if len(parts) == 1:
    io.open(final, "wb").write(io.open(parts[0], "rb").read())
else:
    from fontTools.merge import Merger
    merged = Merger().merge(parts)
    merged.save(final)
print("final font:", os.path.getsize(final), "bytes")

# --- verification ----------------------------------------------------------
cmap = TTFont(final).getBestCmap()
missing = sorted(c for c in chars if c != " " and ord(c) not in cmap)
print("glyphs:", len(cmap), "missing:", len(missing))
if missing:
    io.open(os.path.join(WORK, "still_missing.txt"), "w", encoding="utf-8").write("".join(missing))
    print("missing written to still_missing.txt")

# --- emit header -----------------------------------------------------------
data = io.open(final, "rb").read()
lines = ["#pragma once", "", "#include <array>", "",
         "// Generated subset of Noto Sans SC + Noto Sans KR (SIL Open Font License 1.1)",
         "// covering every character used by the ui translations (latin, cyrillic, kana,",
         "// hangul and the CJK used by the zhs/zht strings).",
         "// Loaded with ImFontAtlas::AddFontFromMemoryTTF.",
         f"inline constexpr std::array<unsigned char, {len(data)}> CN_SANS_DATA = {{"]
for i in range(0, len(data), 32):
    lines.append("\t" + ",".join(str(b) for b in data[i:i + 32]) + ",")
lines.append("};")
lines.append("")
io.open(OUT_HEADER, "w", encoding="utf-8", newline="\n").write("\n".join(lines))
print("wrote", OUT_HEADER, os.path.getsize(OUT_HEADER), "bytes")
