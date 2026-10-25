# Mission Briefings Translation

## Structure

```
briefings/
├── l01/          # Mission 1
│   ├── brief.txt      # Main briefing text
│   ├── brief2.txt     # Additional briefing
│   ├── brief3.txt     # (if exists)
│   ├── brief.lst      # Briefing list (Czech only, optional)
│   ├── loose.txt      # "Loose" text
│   ├── objectives.txt # Mission objectives
│   ├── script1.txt    # Script text 1
│   ├── script2.txt    # Script text 2
│   ├── script3.txt    # Script text 3
│   ├── script4.txt    # Script text 4
│   └── win.txt        # Victory text
├── l02/ ... l20/    # Missions 2-20 (same files)
├── sandbox/          # Sandbox mode
│   ├── brief.txt
│   ├── brief2.txt
│   ├── loose.txt
│   ├── objectives.txt
│   └── win.txt
└── template/         # Template mission (editor)
    ├── brief.txt
    ├── loose.txt
    ├── objectives.txt
    └── win.txt
```

## File Descriptions

| File | Purpose | Typical Length |
|------|---------|----------------|
| `brief.txt` | Main mission briefing shown at start | ~200-500 chars |
| `brief2.txt` | Second briefing page (if any) | ~200-500 chars |
| `brief3.txt` | Third briefing page (rare) | ~100-300 chars |
| `brief.lst` | List of briefings (Czech only) | ~50-100 chars |
| `loose.txt` | "Loose" text - context unclear | ~50-150 chars |
| `objectives.txt` | Mission objectives list | ~50-100 chars |
| `script1-4.txt` | In-mission scripted texts | ~50-150 chars each |
| `win.txt` | Victory message | ~50-150 chars |

## Guidelines

1. **Translate meaning, not word-for-word** - keep the game's tone
2. **Preserve formatting** - line breaks, punctuation
3. **Character limits** - the game UI has fixed widths; test in-game
4. **Encoding** - UTF-8 (save as UTF-8 without BOM)
5. **Only translate what exists** - if a file doesn't exist in Steam for a mission, don't create it

## Testing

Run the game with your language pack, start the mission, verify all texts display correctly.