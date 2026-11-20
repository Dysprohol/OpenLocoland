r"""Says whether two builds are the same files apart from timestamps.

    .venv\Scripts\python tools\compare_builds.py <a> <b>

`a` and `b` are two LocoFix.dll or two Setup.exe (two release builds of one commit: the local
`tools\release.py` and the release workflow's artifact, .github/workflows/release.yml).

- A DLL (any PE file) is compared with the fields a build stamps blanked: the header's time and
  checksum, the debug directory's times and its CodeView record (its GUID changes with every link).
  What still differs is named: the sections, the compilers and linker that made it (the Rich
  header: a different toolset gives different code), the version resource, the imports, the names
  MSVC gave anonymous namespaces (another source path or computer name: tools/build_locofix.py).
- A Setup.exe (PyInstaller's one-file exe) is opened as its archive: every entry of the package
  (the payload, the Python runtime, the DLLs) and every module of its PYZ are compared by their
  unpacked contents, the exe before the archive (the bootloader with Setup's icon, manifest and
  version resource) as a PE file above. An entry that is itself a PE file (LocoFix.dll,
  python3.dll ...) is compared as one, so its timestamp alone does not count, and a zip
  (base_library.zip) by its members.

Exit code 0: the same apart from timestamps; 1: they differ (each difference listed); 2: an error.
Needs pefile (tools/requirements.txt) and, for Setup.exe, PyInstaller (tools/requirements-release.txt).
"""
import argparse
import hashlib
import os
import re
import sys

import pefile

DEBUG_CODEVIEW = 2


def blank_pe(data):
    """`data` (a PE file) with the stamps of a build set to zero, and the PE object (None: not a PE)."""
    try:
        pe = pefile.PE(data=data, fast_load=True)
    except pefile.PEFormatError:
        return data, None
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY[d] for d in (
        "IMAGE_DIRECTORY_ENTRY_DEBUG", "IMAGE_DIRECTORY_ENTRY_EXPORT", "IMAGE_DIRECTORY_ENTRY_RESOURCE",
        "IMAGE_DIRECTORY_ENTRY_IMPORT")])
    out = bytearray(data)

    def zero(offset, size):
        if offset is not None and 0 <= offset and offset + size <= len(out):
            out[offset:offset + size] = bytes(size)

    zero(pe.FILE_HEADER.get_field_absolute_offset("TimeDateStamp"), 4)
    zero(pe.OPTIONAL_HEADER.get_field_absolute_offset("CheckSum"), 4)
    for d in getattr(pe, "DIRECTORY_ENTRY_DEBUG", []):
        zero(d.struct.get_field_absolute_offset("TimeDateStamp"), 4)
        if d.struct.Type == DEBUG_CODEVIEW:   # RSDS, the GUID and the age (the PDB's path stays)
            zero(d.struct.PointerToRawData + 4, 20)
    if hasattr(pe, "DIRECTORY_ENTRY_EXPORT"):
        zero(pe.DIRECTORY_ENTRY_EXPORT.struct.get_field_absolute_offset("TimeDateStamp"), 4)
    if hasattr(pe, "DIRECTORY_ENTRY_RESOURCE"):
        zero(pe.DIRECTORY_ENTRY_RESOURCE.struct.get_field_absolute_offset("TimeDateStamp"), 4)
    return bytes(out), pe


def rich(pe):
    """The Rich header's entries: the tools (product id, build) that made the file and how many objects each."""
    info = pe.parse_rich_header() if pe else None
    if not info:
        return []
    values = info["values"]
    return sorted((values[i] >> 16, values[i] & 0xFFFF, values[i + 1]) for i in range(0, len(values), 2))


def version_strings(pe):
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_RESOURCE"]])
    strings = {}
    for info in getattr(pe, "FileInfo", None) or []:
        for entry in info:
            for table in getattr(entry, "StringTable", []):
                strings.update({k.decode(errors="replace"): v.decode(errors="replace") for k, v in table.entries.items()})
    return strings


def imports(pe):
    return sorted(f"{e.dll.decode(errors='replace')}!{(i.name or str(i.ordinal).encode()).decode(errors='replace')}"
                  for e in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []) for i in e.imports)


def compare_pe(name, a, b):
    """The differences of two PE files (an empty list: the same apart from timestamps)."""
    na, pa = blank_pe(a)
    nb, pb = blank_pe(b)
    if na == nb:
        return []
    if not pa or not pb:
        return [f"{name}: different contents ({len(a)} and {len(b)} bytes; not a PE file)"]
    out = [f"{name}: different code or data apart from timestamps ({len(a)} and {len(b)} bytes)"]
    la = (pa.OPTIONAL_HEADER.MajorLinkerVersion, pa.OPTIONAL_HEADER.MinorLinkerVersion)
    lb = (pb.OPTIONAL_HEADER.MajorLinkerVersion, pb.OPTIONAL_HEADER.MinorLinkerVersion)
    if la != lb:
        out.append(f"  linker {la[0]}.{la[1]} and {lb[0]}.{lb[1]}: another MSVC toolset (LOCOFIX_MSVC)")
    ra, rb = rich(pa), rich(pb)
    if ra != rb:
        builds = lambda r: sorted({build for _, build, _ in r if build})
        out.append(f"  the tools that made it differ (Rich header): builds {builds(ra)} and {builds(rb)} - "
                   "another compiler, linker or library version (LOCOFIX_MSVC, LOCOFIX_WINSDK)")
    sa = {s.Name.rstrip(b"\0").decode(errors="replace"): s for s in pa.sections}
    sb = {s.Name.rstrip(b"\0").decode(errors="replace"): s for s in pb.sections}
    for s in sorted(set(sa) | set(sb)):
        if s not in sa or s not in sb:
            out.append(f"  section {s}: only in {'the first' if s in sa else 'the second'}")
            continue
        x = na[sa[s].PointerToRawData:sa[s].PointerToRawData + sa[s].SizeOfRawData]
        y = nb[sb[s].PointerToRawData:sb[s].PointerToRawData + sb[s].SizeOfRawData]
        if x != y:
            changed = sum(1 for i, j in zip(x, y) if i != j) + abs(len(x) - len(y))
            out.append(f"  section {s}: {len(x)} and {len(y)} bytes, {changed} differ")
    va, vb = version_strings(pa), version_strings(pb)
    for k in sorted(set(va) | set(vb)):
        if va.get(k) != vb.get(k):
            out.append(f"  version resource {k}: {va.get(k)!r} and {vb.get(k)!r}")
    pa.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]])
    pb.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]])
    ia, ib = set(imports(pa)), set(imports(pb))
    if ia != ib:
        out.append(f"  imports only in the first: {sorted(ia - ib)[:10]}, only in the second: {sorted(ib - ia)[:10]}")
    ha, hb = anonymous_namespaces(a), anonymous_namespaces(b)
    if ha != hb:
        out.append(f"  anonymous namespaces {' '.join(ha) or '-'} and {' '.join(hb) or '-'}: another source path or "
                   "computer name at compiling (a build without tools/build_locofix.py's release settings)")
    return out


def anonymous_namespaces(data):
    """The names MSVC gave anonymous namespaces (?A0x<hash>), as far as a DLL keeps them (its RTTI names)."""
    return sorted({m.decode() for m in re.findall(rb"\?A0x[0-9a-f]{8}", data)})


def sha(data):
    return hashlib.sha256(data).hexdigest()[:16]


def why(name):
    """A likely reason a payload entry differs."""
    low = name.lower().replace("\\", "/")
    if low.endswith("version.txt"):
        return "the version (git describe) differs: another commit, a tag, or -dirty"
    if low == "locofix.dll":
        return "LocoFix.dll was built differently: compare the two DLLs themselves"
    if low.endswith("payload.txt"):
        return "the payload has other files"
    if low.startswith(("game/", "lang/", "tools/", "external/")):
        return "the repository's file differs (another commit, or line endings: .gitattributes / core.autocrlf)"
    if low.startswith(("python", "base_library", "_tcl_data", "_tk_data", "tcl", "tk")) or low.endswith(".pyd"):
        return "another Python (the version of setup-python and the local venv)"
    if low.startswith(("pil/", "pil.")):
        return "another Pillow (tools/requirements-release.txt pins it)"
    return "another version of a package (pip freeze in both)"


def compare_zip(name, a, b):
    """The differences of two zip files by their members (their dates and order do not count)."""
    import io
    import zipfile
    za, zb = zipfile.ZipFile(io.BytesIO(a)), zipfile.ZipFile(io.BytesIO(b))
    ma, mb = set(za.namelist()), set(zb.namelist())
    out = [f"{name}/{m}: only in the first" for m in sorted(ma - mb)]
    out += [f"{name}/{m}: only in the second" for m in sorted(mb - ma)]
    out += [f"{name}/{m}: differs - {why(name)}" for m in sorted(ma & mb) if za.read(m) != zb.read(m)]
    if not out and za.namelist() != zb.namelist():
        print(f"{name}: the same members in another order (PYTHONHASHSEED was not fixed)")
    return out


def compare_setup(a, b):
    from PyInstaller.archive.readers import CArchiveReader, PKG_ITEM_PYZ
    ra, rb = CArchiveReader(a), CArchiveReader(b)
    out = []
    da, db = open(a, "rb").read(), open(b, "rb").read()
    out += compare_pe("the exe before the archive (bootloader, icon, manifest, version resource)",
                      da[:ra._start_offset], db[:rb._start_offset])
    ta, tb = ra.toc, rb.toc
    for name in sorted(set(ta) - set(tb)):
        out.append(f"{name}: only in the first")
    for name in sorted(set(tb) - set(ta)):
        out.append(f"{name}: only in the second")
    same = 0
    for name in sorted(set(ta) & set(tb)):
        if ta[name][4] == PKG_ITEM_PYZ:
            za, zb = ra.open_embedded_archive(name), rb.open_embedded_archive(name)
            for m in sorted(set(za.toc) - set(zb.toc)):
                out.append(f"{name}/{m}: a module only in the first")
            for m in sorted(set(zb.toc) - set(za.toc)):
                out.append(f"{name}/{m}: a module only in the second")
            for m in sorted(set(za.toc) & set(zb.toc)):
                x, y = za.extract(m, raw=True), zb.extract(m, raw=True)
                if x != y:
                    out.append(f"{name}/{m}: the module differs ({sha(x or b'')} and {sha(y or b'')}) - {why(m)}")
                else:
                    same += 1
            continue
        x, y = ra.extract(name), rb.extract(name)
        if x == y:
            same += 1
            continue
        if x[:4] == b"PK\x03\x04" and y[:4] == b"PK\x03\x04":   # base_library.zip
            d = compare_zip(name, x, y)
            if not d:
                same += 1
                continue
            out += d
        elif x[:2] == b"MZ" and y[:2] == b"MZ":
            d = compare_pe(name, x, y)
            if not d:
                same += 1
                continue
            out += d + [f"  - {why(name)}"]
        else:
            out.append(f"{name}: differs ({len(x)} and {len(y)} bytes) - {why(name)}")
    print(f"{same} entries the same apart from timestamps")
    return out


def main():
    p = argparse.ArgumentParser(description="Are two builds (LocoFix.dll or Setup.exe) the same files apart from timestamps?")
    p.add_argument("a")
    p.add_argument("b")
    args = p.parse_args()
    try:
        a, b = open(args.a, "rb").read(), open(args.b, "rb").read()
    except OSError as e:
        print(f"cannot read: {e}")
        return 2
    print(f"{args.a}: {len(a)} bytes, SHA-256 {hashlib.sha256(a).hexdigest()}")
    print(f"{args.b}: {len(b)} bytes, SHA-256 {hashlib.sha256(b).hexdigest()}")
    if a == b:
        print("THE SAME: identical files")
        return 0
    try:
        from PyInstaller.archive.readers import CArchiveReader, NotAnArchiveError
        try:
            CArchiveReader(args.a), CArchiveReader(args.b)
            setup = True
        except (NotAnArchiveError, Exception):
            setup = False
    except ImportError:
        setup = False
    differences = compare_setup(args.a, args.b) if setup else compare_pe(os.path.basename(args.a), a, b)
    if not differences:
        print("THE SAME apart from timestamps")
        return 0
    print("DIFFERENT:")
    for d in differences:
        print(d)
    return 1


if __name__ == "__main__":
    sys.exit(main())
