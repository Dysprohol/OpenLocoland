"""Binary patches applied to the original Steam.exe.

The fixes to how the game works are in C++ (src/LocoFix, loaded as LocoFix.dll). The exe
only gets what cannot be done from the library: loading it, and the look of the launcher
window (the dialog resource).

Every patch first checks that the expected bytes are in place, so running it on
a different exe version ends with an error instead of a broken file.

Addresses are virtual addresses (VA). In this exe the .text/.rdata/.data sections are at
the same file offset as their RVA, so file offset = VA - 0x400000. The exception is
.rsrc (VA 0x5d6000, file 0x8a000) - for the dialog we use the file offset directly.

The loader code goes into the zeros at the end of .text (from 0x474830). For the system
to map it, the section's VirtualSize is raised to SizeOfRawData.
"""
import struct
import sys

BASE = 0x400000
IAT_LOAD_LIBRARY_A = 0x475104
NEW_CODE = 0x474830          # start of the free space at the end of .text
START_DIALOG = 0x8ADB8       # file offset of the START_DIALOG template


class Exe:
    def __init__(self, data):
        self.d = bytearray(data)

    @staticmethod
    def off(va):
        return va - BASE

    def check(self, va, hexbytes):
        b = bytes.fromhex(hexbytes)
        found = bytes(self.d[self.off(va):self.off(va) + len(b)])
        if found != b:
            raise ValueError(f"{va:#x}: found {found.hex()}, expected {hexbytes} - a different exe version?")

    def write(self, va, data):
        self.d[self.off(va):self.off(va) + len(data)] = data

    def is_free(self, va, size):
        return set(self.d[self.off(va):self.off(va) + size]) == {0}


def patch_load_locofix(exe, va):
    """Before WinMain the game loads LocoFix.dll with the C++ fixes (src/LocoFix).

    entry calls WinMain at 0x45952b (call 0x4186f0). That call now goes to our code:
    LoadLibraryA("LocoFix.dll"), then a jump to WinMain with the same stack. Without
    the library the game starts normally, just without the fixes.
    Returns the address of the end of the inserted code.
    """
    call_site = 0x45952b
    winmain = 0x4186f0
    exe.check(call_site, "e8" + struct.pack("<i", winmain - (call_site + 5)).hex())

    name = va + 16                                      # library name after the code
    code = bytearray()
    code += bytes.fromhex("68") + struct.pack("<I", name)                    # push "LocoFix.dll"
    code += bytes.fromhex("ff15") + struct.pack("<I", IAT_LOAD_LIBRARY_A)    # call [LoadLibraryA]
    code += bytes.fromhex("e9") + struct.pack("<i", winmain - (va + len(code) + 5))  # jmp WinMain
    assert len(code) <= 16
    code = code.ljust(16, bytes.fromhex("cc")) + b"LocoFix.dll" + bytes(1)

    assert exe.is_free(va, len(code))
    exe.write(va, code)
    exe.write(call_site, bytes.fromhex("e8") + struct.pack("<i", va - (call_site + 5)))
    return va + len(code)


def patch_start_dialog(exe):
    """Launcher: picture, "Pełny ekran" (full screen) box, resolution, OK and Anuluj (Cancel).

    Field 1005 moves under the picture with a Polish label (same string length, so the
    template does not shift). Fields 1006-1012 (don't show dialog, triple buffer, palette,
    low detail, demo recording, HQ sound) and the device list 1000 are moved out of the
    window - all devices are dgVoodoo and differ only by monitor, so the game stays on
    device 0. The resolution list takes the place of the device list, the buttons and the
    bottom of the window move up. "Cancel" -> "Anuluj".
    """
    d = exe.d

    def field(offset, cid):
        assert struct.unpack_from("<H", d, START_DIALOG + offset + 16)[0] == cid
        return START_DIALOG + offset + 8       # x, y, width, height

    full_screen = field(0xFC, 1005)
    assert struct.unpack_from("<hhhh", d, full_screen) == (7, 99, 88, 14)
    struct.pack_into("<hhhh", d, full_screen, 7, 166, 120, 10)
    label = START_DIALOG + 0xFC + 18 + 4                # after the class 0xFFFF,0x0080
    assert d[label:label + 22].decode("utf-16-le") == "Full-Screen"
    d[label:label + 22] = "Pełny ekran".encode("utf-16-le")

    for offset, cid in [(0x12C, 1006), (0x188, 1007), (0x1C8, 1008),
                        (0x1F8, 1009), (0x228, 1010), (0x254, 1012), (0x90, 1000)]:
        struct.pack_into("<h", d, field(offset, cid), -2000)

    struct.pack_into("<h", d, field(0xAC, 1001) + 2, 178)     # resolution
    struct.pack_into("<h", d, field(0x48, 1) + 2, 197)        # OK
    struct.pack_into("<h", d, field(0x68, 2) + 2, 197)        # Cancel
    assert struct.unpack_from("<h", d, START_DIALOG + 16)[0] == 234
    struct.pack_into("<h", d, START_DIALOG + 16, 216)         # window height

    cancel = START_DIALOG + 0x68 + 18 + 4
    assert d[cancel:cancel + 12].decode("utf-16-le") == "Cancel"
    d[cancel:cancel + 12] = "Anuluj".encode("utf-16-le")


def grow_text_section(exe, code_end):
    d = exe.d
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    sec = pe + 24 + struct.unpack_from("<H", d, pe + 20)[0]
    assert d[sec:sec + 5] == b".text"
    vsize, va, rawsize = struct.unpack_from("<III", d, sec + 8)
    needed = code_end - BASE - va
    assert needed <= rawsize
    if vsize < needed:
        struct.pack_into("<I", d, sec + 8, rawsize)


def build(original, output):
    exe = Exe(open(original, "rb").read())
    end = patch_load_locofix(exe, NEW_CODE)
    grow_text_section(exe, end)
    patch_start_dialog(exe)
    open(output, "wb").write(exe.d)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit("usage: patch_exe.py <Steam.exe> <Locoland.exe>")
    build(sys.argv[1], sys.argv[2])
