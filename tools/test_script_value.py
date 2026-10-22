r"""Differential test of the native script value helpers.

    python tools\test_script_value.py [cases]

Builds tools\test_script_value.cpp into build\LocoFix\test_script_value.exe, emulates
the original String_ToInt (0x43d8c0), ScriptValue_ToString (0x4424c0) and
ScriptValue_BinaryOp (0x43da60) in Unicorn and compares three results for every
generated case:

  * a reference model written here from the machine code,
  * the original functions (with atoi, sscanf, itoa, String_Append, String_Assign and
    Log_Write stubbed; ScriptValue_ToString's SEH frame in fs:[0] is dropped),
  * the C++ implementation in src\LocoFix\script_value.h.

The original crashes on `a % 0` (BUGS.md F8), so those cases are compared with the
model and the C++ only.
"""
import argparse
import os
import random
import struct
import subprocess
import sys

import unicorn
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EIP,
                               UC_X86_REG_ESP)

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from build_locofix import vcvars32  # noqa: E402
from languages import STEAM  # noqa: E402

OUT_DIR = os.path.join(REPO, "build", "LocoFix")
TEST_EXE = os.path.join(OUT_DIR, "test_script_value.exe")

STRING_TO_INT = 0x43D8C0
BINARY_OP = 0x43DA60
TO_STRING = 0x4424C0
POP_INT = 0x4429C0
POP_STRING = 0x442A20
POP_OBJECT = 0x442A40
POP_VID = 0x442AE0
SCRIPT_POP_OBJECT = 0x442670
STRING_APPEND = 0x4265B0
STRING_ASSIGN = 0x4264C0
LOG_WRITE = 0x426080
LOG_ERROR = 0x426140
ITOA = 0x41F9E0
CRT_DELETE = 0x456A00
ATOI = 0x457892
SSCANF = 0x4577D3
G_LOG = 0x4A9D5C
G_EMPTY_VID = 0x5D3A60

STACK = 0x01000000
STACK_SIZE = 0x100000
HEAP = 0x02000000
HEAP_SIZE = 0x100000
VALUES = 0x03000000
STOP = 0x03001000
WORLD = 0x03002000             # the World for World_PopInt/World_PopString

WORLD_COUNT = 0x150            # int, values on the script stack
WORLD_VALUES = 0x158           # sv::Value*, the stack itself
WORLD_MACHINE = 0x14C          # the script machine of the World
WORLD_VID_COUNT = 0x290        # int, number of loaded VIDs
WORLD_VIDS = 0x294             # void**, the VIDs by number

# The VIDs of the test World and the empty one (the game uses g_emptyVid for a number
# that is not loaded); the same numbers are used by the test host.
TEST_VIDS = [0x1000, 0x1001, 0x1002, 0]
TEST_EMPTY_VID = 0x2000

INT32_MIN = -0x80000000
INT32_MAX = 0x7FFFFFFF


# ---------------------------------------------------------------- reference model

def s32(value):
    value &= 0xFFFFFFFF
    return value - 0x100000000 if value & 0x80000000 else value


def trunc_div(a, b):
    """C integer division (truncates toward zero)."""
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b >= 0) else -q


def _atoi(text):
    i = 0
    while i < len(text) and text[i] in b" \t\n\v\f\r":
        i += 1
    sign = 1
    if i < len(text) and text[i] in b"+-":
        if text[i] == ord("-"):
            sign = -1
        i += 1
    value = 0
    while i < len(text) and ord("0") <= text[i] <= ord("9"):
        value = (value * 10 + text[i] - ord("0")) & 0xFFFFFFFF
        i += 1
    return s32(value * sign)


def _sscanf_i(text):
    """Returns the int or None when "%i" would not convert anything."""
    i = 0
    while i < len(text) and text[i] in b" \t\n\v\f\r":
        i += 1
    sign = 1
    if i < len(text) and text[i] in b"+-":
        if text[i] == ord("-"):
            sign = -1
        i += 1
    if text[i:i + 2].lower() == b"0x":
        i += 2
        start = i
        value = 0
        while i < len(text) and chr(text[i]) in "0123456789abcdefABCDEF":
            value = value * 16 + int(chr(text[i]), 16)
            i += 1
        if i == start:
            return None
        return s32(value * sign)
    if i < len(text) and text[i] == ord("0"):
        # %i reads a leading zero as octal, but String_ToInt only takes this path for
        # strings whose second character is 'x', so a valid octal cannot reach it.
        value = 0
        while i < len(text) and ord("0") <= text[i] <= ord("7"):
            value = value * 8 + text[i] - ord("0")
            i += 1
        return s32(value * sign)
    if i < len(text) and ord("1") <= text[i] <= ord("9"):
        value = 0
        while i < len(text) and ord("0") <= text[i] <= ord("9"):
            value = value * 10 + text[i] - ord("0")
            i += 1
        return s32(value * sign)
    return None


def parse_text(text):
    if text[:1] and len(text) > 1 and text[1] == ord("x"):
        value = _sscanf_i(text)
        return 0 if value is None else value       # the original would leave the pointer
    return _atoi(text)


def value_to_int(flags, value, text):
    return parse_text(text) if flags & 1 else value


def model_to_string(flags, value, text):
    """Value as text: an int becomes its decimal text, a text stays (flags unchanged)."""
    if not flags & 2:
        return flags, value, text
    return flags, value, str(s32(value)).encode()


def model_pop_int(flags, value, text, count=1):
    """World_PopInt: the value as an int and the stack one shorter."""
    return value_to_int(flags, value, text), count - 1, b""


def model_pop_string(flags, value, text, count=1):
    """World_PopString: the value as a String and the stack one shorter."""
    return flags, count - 1, model_to_string(flags, value, text)[2]


def model_pop_object(flags, value, text, count=1):
    """World_PopObject: the value as an int; a value that is not an object is reported."""
    reported = int(value != 0 and not flags & 0x10)
    return value_to_int(flags, value, text), count - 1, reported


def model_script_pop_object(flags, value, text, count=1):
    """Script_PopObject: like World_PopObject, on the stack of the script machine."""
    return model_pop_object(flags, value, text, count)


def model_pop_vid(flags, value, text, count=1):
    """World_PopVid: the VID of the number; a number that is not loaded is reported."""
    number, converted = text_number(flags, value, text)
    vid = TEST_VIDS[number] if converted and 0 <= number < len(TEST_VIDS) else 0
    if not vid:
        vid = TEST_EMPTY_VID
    return vid, count - 1, int(vid == TEST_EMPTY_VID)


def text_number(flags, value, text):
    """(number, converted) of a value: a text is read with atoi, a text with 'x' as its
    second character with sscanf "%i"."""
    if not flags & 1:
        return value, True
    if text[:1] and len(text) > 1 and text[1] == ord("x"):
        number = _sscanf_i(text)
        return (0 if number is None else number), number is not None
    return _atoi(text), True


def model_binary(op, left, right):
    """left/right are (flags, value, text); returns the resulting (flags, value, text)."""
    lflags, lvalue, ltext = left
    rflags, rvalue, rtext = right
    r = value_to_int(rflags, rvalue, rtext)
    if lflags & 1:
        lvalue = parse_text(ltext)
    l = lvalue
    flags, value, text = lflags, lvalue, ltext

    if op == 0x06:
        flags = 2
        value = 0x0FFFFFFF if r == 0 else (INT32_MIN if (l == INT32_MIN and r == -1)
                                           else s32(trunc_div(l, r)))
    elif op == 0x07:
        flags = 2
        if r == 0:
            value = 0x0FFFFFFF
        elif r == -1:
            value = 0
        else:
            value = s32(l - trunc_div(l, r) * r)
    elif op == 0x08:
        if (rflags & 1) and (lflags & 1):
            flags, text = 1, ltext + rtext
        else:
            flags, value = 2, s32(l + r)
    elif op == 0x09:
        flags, value = 2, s32(l - r)
    elif op == 0x0A:
        flags, value = 2, s32(l ^ r)
    elif op == 0x0B:
        flags, value = 2, s32(l | r)
    elif op == 0x0C:
        flags, value = 2, s32(l & r)
    elif op == 0x0D:
        flags = 2
        value = int((ltext == rtext) if ((rflags & 1) and (lflags & 1)) else (l == r))
    elif op == 0x0E:
        flags, value = 2, int(l != 0 or r != 0)
    elif op == 0x0F:
        flags, value = 2, int(l > r)
    elif op == 0x10:
        flags, value = 2, int(l < r)
    elif op == 0x11:
        flags, value = 2, int(l >= r)
    elif op == 0x12:
        flags, value = 2, int(l <= r)
    elif op == 0x13:
        flags, value = 2, s32(l * r)
    elif op == 0x14:
        flags = 2
        value = int((ltext != rtext) if ((rflags & 1) and (lflags & 1)) else (l != r))
    elif op == 0x15:
        flags, value = 2, int(l != 0 and r != 0)
    elif op == 0x16:
        flags, value = 2, s32(l >> (r & 31))
    elif op == 0x17:
        flags, value = 2, s32((l & 0xFFFFFFFF) << (r & 31))
    else:
        flags = 2
    return flags, s32(value), text


def crashes(op, left, right):
    """True when the original machine code faults (BUGS.md F8, idiv overflow)."""
    l = value_to_int(*left)
    r = value_to_int(*right)
    if op == 0x07 and r == 0:
        return True
    if op in (0x06, 0x07) and l == INT32_MIN and r == -1:
        return True
    return False


# ---------------------------------------------------------------- Unicorn original

class Image:
    def __init__(self, path):
        self.data = open(path, "rb").read()
        e_lfanew = struct.unpack_from("<I", self.data, 0x3C)[0]
        self.base = struct.unpack_from("<I", self.data, e_lfanew + 0x34)[0]
        count = struct.unpack_from("<H", self.data, e_lfanew + 6)[0]
        size_opt = struct.unpack_from("<H", self.data, e_lfanew + 20)[0]
        offset = e_lfanew + 24 + size_opt
        self.sections = []
        for _ in range(count):
            vsize, va, rawsize, raw = struct.unpack_from("<IIII", self.data, offset + 8)
            self.sections.append((va, vsize, raw, rawsize))
            offset += 40

    def bytes_at(self, address):
        rva = address - self.base
        for va, vsize, raw, rawsize in self.sections:
            if va <= rva < va + max(vsize, rawsize):
                start = raw + (rva - va)
                return self.data[start:]
        return b""

    def cstring(self, address):
        data = self.bytes_at(address)
        return data[:data.index(b"\0")] if b"\0" in data else data


class Original:
    def __init__(self, exe_path):
        self.image = Image(exe_path)
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        page = 0x1000
        for va, vsize, raw, rawsize in self.image.sections:
            start = (self.image.base + va) & ~(page - 1)
            end = (self.image.base + va + max(vsize, rawsize) + page - 1) & ~(page - 1)
            try:
                self.uc.mem_map(start, end - start)
            except unicorn.UcError:
                pass
            blob = self.image.data[raw:raw + rawsize]
            self.uc.mem_write(self.image.base + va, blob)
        self.uc.mem_map(STACK, STACK_SIZE)
        self.uc.mem_map(HEAP, HEAP_SIZE)
        self.uc.mem_map(VALUES, page)
        self.uc.mem_map(STOP, page)
        self.uc.mem_map(WORLD, page)
        self.uc.mem_write(STOP, b"\xF4")            # hlt
        # ScriptValue_ToString keeps a SEH frame in fs:[0]; Unicorn has no segment base,
        # so the three instructions that touch it are replaced (the stack stays the same:
        # mov eax,fs:[0] still pushes the 0 it reads).
        self.uc.mem_write(0x4424C7, b"\xB8\x00\x00\x00\x00\x90")
        self.uc.mem_write(0x4424CE, b"\x90" * 7)
        self.uc.mem_write(0x442542, b"\x90" * 7)
        # The game sets the empty VID and the log when it starts; the test uses its own.
        self.uc.mem_write(G_EMPTY_VID, struct.pack("<I", TEST_EMPTY_VID))
        self.uc.mem_write(G_LOG, struct.pack("<I", 0x3000))
        self.heap = HEAP + 0x1000
        self.log_errors = 0
        self.log_writes = 0
        for address, handler in ((ATOI, self._atoi), (SSCANF, self._sscanf),
                                 (STRING_APPEND, self._append), (LOG_WRITE, self._log),
                                 (LOG_ERROR, self._log_error),
                                 (ITOA, self._itoa), (STRING_ASSIGN, self._assign),
                                 (CRT_DELETE, self._delete)):
            self.uc.hook_add(UC_HOOK_CODE, handler, None, address, address)

    def _u32(self, address):
        return struct.unpack("<I", self.uc.mem_read(address, 4))[0]

    def _cstring(self, address):
        data = bytes(self.uc.mem_read(address, 4096))
        return data[:data.index(b"\0")] if b"\0" in data else data

    def _w32(self, address, value):
        self.uc.mem_write(address, struct.pack("<I", value & 0xFFFFFFFF))

    def _return(self, esp_extra):
        """Pops the return address, drops esp_extra argument bytes."""
        esp = self.uc.reg_read(UC_X86_REG_ESP)
        ret = self._u32(esp)
        self.uc.reg_write(UC_X86_REG_ESP, esp + 4 + esp_extra)
        self.uc.reg_write(UC_X86_REG_EIP, ret)

    def _atoi(self, uc, address, size, user):
        arg = self._u32(uc.reg_read(UC_X86_REG_ESP) + 4)
        text = self._cstring(arg)
        self.uc.reg_write(UC_X86_REG_EAX, _atoi(text) & 0xFFFFFFFF)
        self._return(0)                             # cdecl: the caller drops the argument

    def _sscanf(self, uc, address, size, user):
        esp = uc.reg_read(UC_X86_REG_ESP)
        text = self._cstring(self._u32(esp + 4))
        out = self._u32(esp + 0xC)
        value = _sscanf_i(text)
        if value is not None:
            self._w32(out, value)
        self.uc.reg_write(UC_X86_REG_EAX, 0 if value is None else 1)
        self._return(0)                             # cdecl: the caller drops the arguments

    def _alloc(self, blob):
        address = self.heap
        self.heap += (len(blob) + 4) & ~3
        self.uc.mem_write(address, blob + b"\0\0\0\0")
        return address

    def _append(self, uc, address, size, user):
        esp = uc.reg_read(UC_X86_REG_ESP)
        ecx = uc.reg_read(UC_X86_REG_ECX)
        other = self._u32(self._u32(esp + 4))
        old = self._u32(ecx)
        self._w32(ecx, self._alloc(self._cstring(old) + self._cstring(other)))
        self.uc.reg_write(UC_X86_REG_EAX, ecx)
        self._return(4)

    def _log(self, uc, address, size, user):
        self.log_writes += 1
        self.uc.reg_write(UC_X86_REG_EAX, 0)
        self._return(0)

    def _log_error(self, uc, address, size, user):
        self.log_errors += 1
        self.uc.reg_write(UC_X86_REG_EAX, 0)
        self._return(0)                             # cdecl: the caller drops the arguments

    def _itoa(self, uc, address, size, user):
        """The game's itoa (0x41f9e0): decimal digits of the int into a new buffer. It
        returns the output slot (EAX = the out pointer), which the caller passes on."""
        esp = uc.reg_read(UC_X86_REG_ESP)
        out = self._u32(esp + 4)
        value = s32(self._u32(esp + 8))
        self._w32(out, self._alloc(str(value).encode()))
        self.uc.reg_write(UC_X86_REG_EAX, out)
        self._return(0)                             # cdecl: the caller drops the arguments

    def _assign(self, uc, address, size, user):
        """The game's String_Assign (0x4264c0): this gets a copy of the source String."""
        esp = uc.reg_read(UC_X86_REG_ESP)
        this = uc.reg_read(UC_X86_REG_ECX)
        source = self._u32(esp + 4)
        self._w32(this, self._alloc(self._cstring(self._u32(source))))
        self.uc.reg_write(UC_X86_REG_EAX, this)
        self._return(4)                             # __thiscall: the callee drops the argument

    def _delete(self, uc, address, size, user):
        self._return(0)                             # crt_delete: the buffers are the test's

    def _put_text(self, text):
        return self._alloc(text) if text else self._alloc(b"")

    def run(self, kind, op, left, right):
        self.heap = HEAP + 0x1000
        self.log_errors = 0
        self.log_writes = 0
        lflags, lvalue, ltext = left
        rflags, rvalue, rtext = right
        ltext_ptr = self._put_text(ltext)
        rtext_ptr = self._put_text(rtext)
        # Value { uint8 flags; pad[3]; int32 value; char* text }
        self.uc.mem_write(VALUES, struct.pack("<B3xII", lflags, lvalue & 0xFFFFFFFF,
                                              ltext_ptr))
        self.uc.mem_write(VALUES + 0x10, struct.pack("<B3xII", rflags, rvalue & 0xFFFFFFFF,
                                                     rtext_ptr))
        esp = STACK + STACK_SIZE - 0x100
        self._w32(esp, STOP)
        self._w32(esp + 4, op)
        self._w32(esp + 8, VALUES + 0x10)
        if kind == "V":
            self._w32(esp + 4, self._alloc(b"for test"))   # the "for ..." text
        self.uc.reg_write(UC_X86_REG_ESP, esp)
        if kind == "S":
            # String_ToInt takes a pointer to the char* field, not the value itself.
            self._w32(VALUES + 0x20, ltext_ptr)
            self.uc.reg_write(UC_X86_REG_ECX, VALUES + 0x20)
        elif kind in ("P", "Q", "O", "V", "R"):
            # The World: one value on the script stack, at the base of VALUES.
            self._w32(WORLD + WORLD_COUNT, 1)
            self._w32(WORLD + WORLD_VALUES, VALUES)
            self._w32(WORLD + WORLD_VID_COUNT, len(TEST_VIDS))
            for index, vid in enumerate(TEST_VIDS):
                self._w32(WORLD + WORLD_VIDS + index * 4, vid)
            self.uc.reg_write(UC_X86_REG_ECX, WORLD + WORLD_MACHINE if kind == "R" else WORLD)
        else:
            self.uc.reg_write(UC_X86_REG_ECX, VALUES)
        self.uc.reg_write(UC_X86_REG_EAX, 0)
        try:
            if kind == "S":
                self.uc.emu_start(STRING_TO_INT, STOP, 0, 10000)
            elif kind == "T":
                self.uc.emu_start(TO_STRING, STOP, 0, 100000)
            elif kind == "P":
                self.uc.emu_start(POP_INT, STOP, 0, 100000)
            elif kind == "Q":
                self.uc.emu_start(POP_STRING, STOP, 0, 100000)
            elif kind == "O":
                self.uc.emu_start(POP_OBJECT, STOP, 0, 100000)
            elif kind == "V":
                self.uc.emu_start(POP_VID, STOP, 0, 100000)
            elif kind == "R":
                self.uc.emu_start(SCRIPT_POP_OBJECT, STOP, 0, 100000)
            else:
                self.uc.emu_start(BINARY_OP, STOP, 0, 100000)
        except unicorn.UcError as error:
            return ("crash", str(error))
        if kind == "S":
            return s32(self.uc.reg_read(UC_X86_REG_EAX)), 0, b""
        if kind == "P":
            return (s32(self.uc.reg_read(UC_X86_REG_EAX)), self._u32(WORLD + WORLD_COUNT),
                    b"")
        if kind == "O":
            return (s32(self.uc.reg_read(UC_X86_REG_EAX)), self._u32(WORLD + WORLD_COUNT),
                    self.log_errors)
        if kind == "R":
            return (s32(self.uc.reg_read(UC_X86_REG_EAX)),
                    self._u32(WORLD + WORLD_MACHINE + 0x04), self.log_errors)
        if kind == "V":
            return (self.uc.reg_read(UC_X86_REG_EAX), self._u32(WORLD + WORLD_COUNT),
                    self.log_writes)
        if kind == "Q":
            result = self.uc.reg_read(UC_X86_REG_EAX)
            flags = self.uc.mem_read(VALUES, 1)[0]
            return (flags, self._u32(WORLD + WORLD_COUNT),
                    self._cstring(self._u32(result)) if result else b"")
        flags = self.uc.mem_read(VALUES, 1)[0]
        value = struct.unpack("<i", self.uc.mem_read(VALUES + 4, 4))[0]
        ptr = self._u32(VALUES + 8)
        data = self.uc.mem_read(ptr, 256)
        text = data[:data.index(b"\0")] if b"\0" in data else data
        return flags, value, text


# ---------------------------------------------------------------- cases

TEXTS = [b"", b"0", b"7", b"-13", b"42", b"0x10", b"0x7fffffff", b"abc", b"hello",
         b"12abc", b" 99", b"+3", b"1x2", b"0x0", b"0x", b"0xg"]


def broken_text(text):
    """A text that takes the sscanf path but does not convert (the original leaves the
    output slot alone - BUGS.md F9 - so its result is a pointer, not a number)."""
    return bool(text) and len(text) > 1 and text[1] == ord("x") and _sscanf_i(text) is None


def original_undefined(kind, left, right):
    if kind == "S":
        return broken_text(left[2])
    if kind == "T" or kind == "Q" or kind == "V":
        return False
    if kind == "P" or kind == "O" or kind == "R":
        return bool(left[0] & 1) and broken_text(left[2])
    return ((left[0] & 1 and broken_text(left[2])) or
            (right[0] & 1 and broken_text(right[2])))


def make_cases(count):
    rng = random.Random(0x10C0)
    cases = []

    def add(kind, op=0, left=(2, 0, b""), right=(2, 0, b"")):
        if kind == "B" and crashes(op, left, right):
            return
        cases.append((kind, op, left, right))

    for text in TEXTS:
        add("S", left=(1, 0, text))
    # ScriptValue_ToString: every int, texts and the flags that are not 2.
    for value in (0, 1, -1, 7, -7, 42, 127, -128, 1000, -1000, INT32_MIN, INT32_MAX):
        add("T", left=(2, value, b""))
    add("T", left=(1, 0, b"abc"))
    add("T", left=(1, 123, b"0x10"))
    add("T", left=(0, 5, b""))
    add("T", left=(3, 5, b"12"))
    add("T", left=(3, -5, b""))
    # World_PopInt / World_PopString read the top of the script stack the same way.
    for value in (0, 1, -1, 7, -7, 42, 127, -128, INT32_MIN, INT32_MAX):
        add("P", left=(2, value, b""))
        add("Q", left=(2, value, b""))
    for text in TEXTS:
        add("P", left=(1, 0, text))
        add("Q", left=(1, 0, text))
    add("P", left=(2, 0, b"abc"))
    add("Q", left=(2, 0, b"abc"))
    add("Q", left=(1, 123, b"hello"))
    # World_PopObject and World_PopVid read the same value.
    for value in (0, 1, -1, 7, -7, 42, 127, -128, INT32_MIN, INT32_MAX):
        add("O", left=(2, value, b""))
        add("V", left=(2, value, b""))
    for text in TEXTS:
        add("O", left=(1, 0, text))
        add("V", left=(1, 0, text))
    add("O", left=(2, 5, b""))
    add("O", left=(0x10, 5, b""))          # an object reference: no report
    add("O", left=(0x11, 5, b""))          # an object reference with the text flag
    add("V", left=(2, 3, b""))             # a loaded VID
    add("V", left=(2, 4, b""))             # a number that is not loaded
    add("V", left=(2, -1, b""))
    add("R", left=(2, 5, b""))             # Script_PopObject
    add("R", left=(1, 0, b"42"))
    add("R", left=(0x10, 5, b""))
    # Hand-picked edges: every opcode, texts, zero and the sign/overflow corners.
    for op in range(0x06, 0x18):
        add("B", op, (2, 7, b""), (2, 3, b""))
        add("B", op, (2, -5, b""), (2, 0, b""))
        add("B", op, (1, 0, b"12"), (2, 4, b""))
        add("B", op, (2, 4, b""), (1, 0, b"-7"))
        add("B", op, (1, 0, b"abc"), (1, 0, b"abc"))
        add("B", op, (1, 0, b"abc"), (1, 0, b"abd"))
        add("B", op, (2, INT32_MIN, b""), (2, 2, b""))
        add("B", op, (2, INT32_MAX, b""), (2, 2, b""))
    for op in (0x06, 0x07):
        add("B", op, (2, INT32_MIN, b""), (2, -1, b""))
        add("B", op, (2, 123, b""), (2, 0, b""))
    for _ in range(count):
        roll = rng.random()
        if roll < 0.16:
            kind = "T" if roll < 0.08 else rng.choice(("P", "Q"))
            flags = rng.choice((1, 2, 1, 2, 3))
            value = rng.choice((0, 1, -1, 7, -7, 127, -128, INT32_MIN, INT32_MAX,
                                rng.randrange(-100000, 100001)))
            add(kind, left=(flags, value, rng.choice(TEXTS) if flags & 1 else b""))
            continue
        if roll < 0.24:
            kind = rng.choice(("O", "V", "R"))
            flags = rng.choice((1, 2, 0x10, 0x11, 0x12))
            value = rng.choice((0, 1, -1, 7, -7, 127, INT32_MIN, INT32_MAX,
                                rng.randrange(-100000, 100001)))
            add(kind, left=(flags, value, rng.choice(TEXTS) if flags & 1 else b""))
            continue
        op = rng.randrange(0x06, 0x19 if rng.random() < 0.02 else 0x18)
        left_flags = rng.choice((1, 2))
        right_flags = rng.choice((1, 2))
        left_value = rng.choice((0, 1, -1, 2, -2, 7, -7, 127, -128, INT32_MIN, INT32_MAX,
                                 rng.randrange(-100000, 100001)))
        right_value = rng.choice((0, 1, -1, 2, -2, 3, -3, 31, 32, 33, INT32_MIN, INT32_MAX,
                                  rng.randrange(-100000, 100001)))
        left_text = rng.choice(TEXTS) if left_flags == 1 else b""
        right_text = rng.choice(TEXTS) if right_flags == 1 else b""
        add("B", op, (left_flags, left_value, left_text),
            (right_flags, right_value, right_text))
    return cases


# ---------------------------------------------------------------- runners

def build_test_host():
    os.makedirs(OUT_DIR, exist_ok=True)
    source = os.path.join(HERE, "test_script_value.cpp")
    headers = [os.path.join(REPO, "src", "LocoFix", name)
               for name in ("script_value.h", "world_script.h")]
    if os.path.isfile(TEST_EXE) and os.path.getmtime(TEST_EXE) >= os.path.getmtime(source) \
            and all(os.path.getmtime(TEST_EXE) >= os.path.getmtime(path) for path in headers):
        return
    bat = vcvars32()
    if not bat:
        sys.exit("No MSVC compiler with the C++ tools.")
    script = os.path.join(OUT_DIR, "test_script_value.bat")
    with open(script, "w", encoding="ascii") as f:
        f.write("@echo off\r\n")
        f.write(f'call "{bat}" >nul || exit /b 1\r\n')
        f.write(f'cd /d "{OUT_DIR}"\r\n')
        f.write(f'cl /nologo /O2 /MT /EHsc /utf-8 /W4 /std:c++17 "{source}" '
                f'/Fe:test_script_value.exe\r\n')
    result = subprocess.run(["cmd", "/c", script], capture_output=True, text=True,
                            errors="replace")
    if result.returncode != 0:
        sys.exit("test host build failed:\n" + result.stdout + result.stderr)


def encode(text):
    return text.hex() if text else "-"


def decode(text):
    return b"" if text == "-" else bytes.fromhex(text)


def run_cpp(cases):
    path = os.path.join(OUT_DIR, "script_value_cases.txt")
    with open(path, "w", encoding="ascii") as f:
        for kind, op, left, right in cases:
            if kind == "S":
                f.write(f"S {encode(left[2])}\n")
            elif kind in ("T", "P", "Q", "O", "V", "R"):
                f.write(f"{kind} {left[0]} {left[1]} {encode(left[2])}\n")
            else:
                f.write(f"B {op} {left[0]} {left[1]} {encode(left[2])} "
                        f"{right[0]} {right[1]} {encode(right[2])}\n")
    result = subprocess.run([TEST_EXE, path], capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit(f"test host failed: {result.returncode}\n{result.stderr}")
    lines = result.stdout.splitlines()
    if len(lines) != len(cases):
        sys.exit(f"test host returned {len(lines)} lines for {len(cases)} cases")
    out = []
    for line in lines:
        parts = line.split()
        if parts[0] in ("O", "V", "R"):
            out.append((int(parts[1]), int(parts[2]), int(parts[3])))
        elif len(parts) == 2:                        # String_ToInt prints "value -"
            out.append((int(parts[0]), 0, b""))
        else:
            out.append((int(parts[0]), int(parts[1]), decode(parts[2])))
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("cases", nargs="?", type=int, default=2000,
                        help="number of random cases (default 2000)")
    parser.add_argument("--steam", default=STEAM, help="game folder from Steam")
    args = parser.parse_args()
    exe = os.path.join(args.steam, "Steam.exe")
    if not os.path.isfile(exe):
        sys.exit(f"No game in {args.steam}")
    build_test_host()
    cases = make_cases(args.cases)
    cpp = run_cpp(cases)
    original = Original(exe)
    bad_cpp = bad_orig = 0
    for index, (kind, op, left, right) in enumerate(cases):
        if kind == "S":
            expected = (parse_text(left[2]), 0, b"")
        elif kind == "T":
            expected = model_to_string(*left)
        elif kind == "P":
            expected = model_pop_int(*left)
        elif kind == "Q":
            expected = model_pop_string(*left)
        elif kind == "O":
            expected = model_pop_object(*left)
        elif kind == "R":
            expected = model_script_pop_object(*left)
        elif kind == "V":
            expected = model_pop_vid(*left)
        else:
            expected = model_binary(op, left, right)
        if cpp[index] != expected:
            bad_cpp += 1
            if bad_cpp <= 10:
                print(f"C++ differs, case {index}: {kind} {op:#x} {left} {right}\n"
                      f"  expected {expected}\n  got      {cpp[index]}")
        if not (crashes(op, left, right) or original_undefined(kind, left, right)):
            got = original.run(kind, op, left, right)
            if got != expected:
                bad_orig += 1
                if bad_orig <= 10:
                    print(f"original differs, case {index}: {kind} {op:#x} {left} {right}\n"
                          f"  expected {expected}\n  got      {got}")
    print(f"cases: {len(cases)}, C++ mismatches: {bad_cpp}, original mismatches: {bad_orig}")
    return 1 if bad_cpp or bad_orig else 0


if __name__ == "__main__":
    sys.exit(main())
