"""Merge new symbol lines into the section whose header starts with <header_prefix>
(same logic as merge_symbols.py, but the header comes from the file itself, so the
command line needs no double quotes).

    python merge_prefix.py <new-lines.csv> "<header prefix>"
"""
import sys

csv_path, new_path, header_prefix = sys.argv[1:4]
lines = open(csv_path, encoding='utf-8').read().split('\n')
header = next(l for l in lines if l.startswith(header_prefix))
new = [l for l in open(new_path, encoding='utf-8').read().split('\n') if l.strip()]
key = lambda l: tuple(l.split(';', 2)[:2])
index = {key(l): i for i, l in enumerate(lines)
         if l and not l.startswith('#') and l.count(';') >= 3}
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
    while end < len(lines) and lines[end].strip() and not lines[end].startswith('# ---'):
        end += 1
    section = sorted(lines[pos:end] + added, key=lambda l: int(l.split(';')[0], 16))
    lines[pos:end] = section
    open(csv_path, 'w', encoding='utf-8', newline='').write('\n'.join(lines))
    print(f'added {len(added)} under [{header[:60]}]')
else:
    print(f'nothing added (added={len(added)}, header found={header in lines})')
