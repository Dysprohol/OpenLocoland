# Script Patches Translation

## Structure

```
scripts/
├── constants.lgc.patch
├── debug_functions.lgc.patch
├── menuend.lgc.patch
├── trainstactfunctions.lgc.patch
└── shiftb.txt.patch
```

## What These Are

The game compiles scripts (`.lgc` files) at runtime. Our patches modify the **source** of these scripts before compilation. The patches are in unified diff format.

| File | Purpose |
|------|---------|
| `constants.lgc.patch` | Defines constants, cheat keys (`KEYS_ENABLE`), mission limits |
| `debug_functions.lgc.patch` | Debug/developer functions |
| `menuend.lgc.patch` | End-of-mission menu logic |
| `trainstactfunctions.lgc.patch` | Train tactics AI functions |
| `shiftb.txt.patch` | The developer mode's help (Shift+B), `maps\shiftb.txt` |

## Guidelines

- **Only translate texts and comments** (the developer mode's messages, its help in `shiftb.txt`)
- Do NOT change code logic, variable names, or structure
- The patch must apply cleanly to the Steam original script
- Make the patch with `python tools\patch_text.py diff <original> <edited> <patch>`; `build.py` stops
  when it does not apply
- Encoding: the pack's code page (`codepage=` in `language.ini`), CRLF line ends - not UTF-8

## Example

```diff
-    "Cheats enabled"
+    "Czyty włączone"
```

## Testing

Run `python tools\build.py --target <test folder>`: it applies every patch and says which one fails.