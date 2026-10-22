import pathlib

path = 're/c/00440000.c'
lines = pathlib.Path(path).read_text(encoding='utf-8').split('\n')

addrs = ['0x442e50','0x442e80','0x442f70','0x443020','0x4430c0','0x4430f0','0x443120','0x4434e0','0x443550','0x443630','0x443650','0x443700','0x443780','0x443a00','0x443bd0','0x443bf0','0x443cf0','0x443e40','0x443f40']
for addr in addrs:
    for i,l in enumerate(lines):
        if l.startswith('// ---- ') and addr in l:
            end = next((k for k,l2 in enumerate(lines[i+1:], i+1) if l2.startswith('// ---- ')), len(lines))
            print(f'===== {addr} ({end-i} lines) =====')
            for j in range(i, min(i+12, end)):
                print(f'  {lines[j]}')
            print()
            break
