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
| `shiftb.txt.patch` | Shift+B cheat text (developer mode) |

## Guidelines

- **Only modify translatable strings** in the patch (text in quotes)
- Do NOT change code logic, variable names, or structure
- The patch must apply cleanly to the Steam original script
- Test by running `tools/map_tool.py check` after changes
- Encoding: UTF-8

## Example

```diff
-    "Cheats enabled"
+    "Czyty włączone"
```

## Testing

Run `python tools/map_tool.py check` to verify patches apply correctly.