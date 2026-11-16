import io, os, re, sys

I18N = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "translations")
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "common", "i18n_table.h")

LANGS = ["zh-TW", "en", "ja", "ko", "ru", "es", "fr", "de", "pt"]

def read_tsv(path):
    rows = {}
    for line in io.open(path, encoding="utf-8").read().splitlines():
        if not line.strip():
            continue
        parts = line.split("\t")
        if len(parts) != 2:
            print(f"BAD LINE in {os.path.basename(path)}: {line!r}")
            continue
        if not parts[0].strip().isdigit():
            continue  # header row
        rows[int(parts[0])] = parts[1]
    return rows

keys = read_tsv(os.path.join(I18N, "keys.tsv"))
print("keys:", len(keys))

tables = {}
for lang in LANGS:
    p = os.path.join(I18N, f"lang_{lang}.tsv")
    tables[lang] = read_tsv(p)
    print(f"{lang}: {len(tables[lang])} rows")

# --- validation -------------------------------------------------------------
ph = re.compile(r"\{[^}]*\}")
problems = 0
for lang in LANGS:
    for i, zh in keys.items():
        tr = tables[lang].get(i)
        if tr is None:
            print(f"MISSING {lang} id={i}")
            problems += 1
            continue
        if sorted(ph.findall(zh)) != sorted(ph.findall(tr)):
            print(f"PLACEHOLDER MISMATCH {lang} id={i}: {zh!r} -> {tr!r}")
            problems += 1
        if zh.startswith(" ") and not tr.startswith(" "):
            print(f"LEADING SPACE DROPPED {lang} id={i}")
            problems += 1
        if zh.endswith(" ") and not tr.endswith(" "):
            print(f"TRAILING SPACE DROPPED {lang} id={i}")
            problems += 1
print("problems:", problems)

# --- emit C++ ---------------------------------------------------------------
def cpp_literal(text):
    # the key/translation text is *source literal text*: keep \n style escapes,
    # only neutralise quotes and real control characters.
    text = text.replace('"', '\\"')
    return '"' + text + '"'

lines = []
lines.append("#pragma once")
lines.append("")
lines.append("// Generated from i18n/keys.tsv + lang_*.tsv -- do not edit by hand.")
lines.append("// Order of Entry::values matches i18n::Lang minus ZhCN:")
lines.append("//   zh-TW, en, ja, ko, ru, es, fr, de, pt")
lines.append("namespace i18n_table {")
lines.append("\tstruct Entry {")
lines.append("\t\tconst char* zh;")
lines.append("\t\tconst char* values[9];")
lines.append("\t};")
lines.append("")
lines.append("\tinline constexpr Entry ENTRIES[] = {")

for i in sorted(keys):
    zh = cpp_literal(keys[i])
    vals = ", ".join(cpp_literal(tables[lang][i]) for lang in LANGS)
    lines.append(f"\t\t{{ {zh}, {{ {vals} }} }},")

lines.append("\t};")
lines.append("")
lines.append("\tinline constexpr int ENTRY_COUNT = static_cast<int>(sizeof(ENTRIES) / sizeof(ENTRIES[0]));")
lines.append("}")
lines.append("")

io.open(OUT, "w", encoding="utf-8", newline="\n").write("\n".join(lines))
print("wrote", OUT, os.path.getsize(OUT), "bytes")
