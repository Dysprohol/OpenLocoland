"""Merges address;kind;name;description lines into re/symbols.csv: replaces lines with the same
address and kind, appends the others under a section header (after an anchor line).

    python tools/merge_symbols.py re/symbols.csv <new-lines.csv> "<header>" "<anchor>" [old=new ...]

<new-lines.csv> holds the new symbols, one per line. <header> ("# --- ...") is the section the new
lines go into; when it is missing, they are appended after the anchor line's section. The optional
old=new pairs rename text in the whole file (e.g. a name that changed).
"""
import sys

csv_path, new_path, header, anchor = sys.argv[1:5]
renames = [a.split("=", 1) for a in sys.argv[5:]]
lines = open(csv_path, encoding="utf-8").read().split("\n")
new = [l for l in open(new_path, encoding="utf-8").read().split("\n") if l.strip()]
key = lambda l: tuple(l.split(";", 2)[:2])
index = {key(l): i for i, l in enumerate(lines) if l and not l.startswith("#") and l.count(";") >= 3}
added = []
for l in new:
    k = key(l)
    if k in index:
        lines[index[k]] = l
    else:
        added.append(l)
if added and header in lines:
    pos = lines.index(header) + 1
    end = pos
    while end < len(lines) and lines[end].strip() and not lines[end].startswith("# ---"):
        end += 1
    section = sorted(lines[pos:end] + added, key=lambda l: int(l.split(";")[0], 16))
    lines[pos:end] = section
elif added:
    pos = next(i for i, l in enumerate(lines) if l.startswith(anchor))
    # end of the anchor's section: next header or blank line
    end = pos + 1
    while end < len(lines) and lines[end].strip() and not lines[end].startswith("# ---"):
        end += 1
    block = [header] + sorted(added, key=lambda l: int(l.split(";")[0], 16))
    lines[end:end] = block
text = "\n".join(lines)
for a, b in renames:
    text = text.replace(a, b)
open(csv_path, "w", encoding="utf-8", newline="").write(text)
print(f"replaced {len(new) - len(added)}, added {len(added)}")
