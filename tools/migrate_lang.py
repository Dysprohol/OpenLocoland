#!/usr/bin/env python3
"""
Migration script: Move existing lang/<code>/ to new structure.

Old structure:
  lang/<code>/
    language.ini
    strings.override.ini
    maps/edithelp*.txt
    maps/menu*.txt
    maps/sandbox/*.txt
    maps/template/*.txt
    maps/l??/*.txt
    maps/script/*.lgc.patch
    maps/shiftb.txt.patch

New structure:
  lang/<code>/
    language.ini
    installer/texts.ini          (NEW - copy from _template, then translate)
    game/
      strings.override.ini       (same file)
      briefings/l01-l20/         (from maps/l??/)
      briefings/sandbox/         (from maps/sandbox/)
      briefings/template/        (from maps/template/)
      menus/menu*.txt            (from maps/menu*.txt)
      editor/edithelp*.txt       (from maps/edithelp*.txt)
      scripts/                   (from maps/script/ and maps/shiftb.txt.patch)
        *.lgc.patch
        shiftb.txt.patch
"""
import os
import shutil
import sys
from pathlib import Path

REPO = Path(__file__).parent.parent
LANG_DIR = REPO / "lang"
TEMPLATE_DIR = LANG_DIR / "_template"


def migrate_code(code):
    src = LANG_DIR / code
    if not src.exists():
        print(f"  {code}: not found, skipping")
        return False

    print(f"Migrating {code}...")

    # 1. Create new directories
    installer_dir = src / "installer"
    game_dir = src / "game"
    briefings_dir = game_dir / "briefings"
    menus_dir = game_dir / "menus"
    editor_dir = game_dir / "editor"
    scripts_dir = game_dir / "scripts"

    for d in [installer_dir, game_dir, briefings_dir, menus_dir, editor_dir, scripts_dir]:
        d.mkdir(parents=True, exist_ok=True)

    # 2. Copy language.ini (unchanged)
    # Already in place

    # 3. Create installer/texts.ini from template (will need translation)
    template_texts = TEMPLATE_DIR / "installer" / "texts.ini"
    target_texts = installer_dir / "texts.ini"
    if template_texts.exists() and not target_texts.exists():
        shutil.copy2(template_texts, target_texts)
        print(f"  Created {target_texts.relative_to(REPO)} (needs translation)")

    # 4. Move strings.override.ini to game/
    src_override = src / "strings.override.ini"
    dst_override = game_dir / "strings.override.ini"
    if src_override.exists() and not dst_override.exists():
        shutil.move(str(src_override), str(dst_override))
        print(f"  Moved strings.override.ini -> game/")

    # 5. Migrate maps/ to game/
    maps_dir = src / "maps"
    if maps_dir.exists():
        # briefings/l01-l20/
        for mission_dir in sorted(maps_dir.glob("l??")):
            if mission_dir.is_dir():
                target = briefings_dir / mission_dir.name
                if not target.exists():
                    shutil.move(str(mission_dir), str(target))
                    print(f"  Moved maps/{mission_dir.name} -> game/briefings/")

        # briefings/sandbox/
        sandbox_src = maps_dir / "sandbox"
        sandbox_dst = briefings_dir / "sandbox"
        if sandbox_src.exists() and not sandbox_dst.exists():
            shutil.move(str(sandbox_src), str(sandbox_dst))
            print(f"  Moved maps/sandbox -> game/briefings/")

        # briefings/template/
        template_src = maps_dir / "template"
        template_dst = briefings_dir / "template"
        if template_src.exists() and not template_dst.exists():
            shutil.move(str(template_src), str(template_dst))
            print(f"  Moved maps/template -> game/briefings/")

        # menus/
        for menu_file in maps_dir.glob("menu*.txt"):
            dst = menus_dir / menu_file.name
            if not dst.exists():
                shutil.move(str(menu_file), str(dst))
                print(f"  Moved maps/{menu_file.name} -> game/menus/")

        # editor/
        for edit_file in maps_dir.glob("edithelp*.txt"):
            dst = editor_dir / edit_file.name
            if not dst.exists():
                shutil.move(str(edit_file), str(dst))
                print(f"  Moved maps/{edit_file.name} -> game/editor/")

        # scripts/
        script_src = maps_dir / "script"
        if script_src.exists():
            for patch_file in script_src.glob("*.patch"):
                dst = scripts_dir / patch_file.name
                if not dst.exists():
                    shutil.move(str(patch_file), str(dst))
                    print(f"  Moved maps/script/{patch_file.name} -> game/scripts/")

        shiftb_patch = maps_dir / "shiftb.txt.patch"
        if shiftb_patch.exists():
            dst = scripts_dir / "shiftb.txt.patch"
            if not dst.exists():
                shutil.move(str(shiftb_patch), str(dst))
                print(f"  Moved maps/shiftb.txt.patch -> game/scripts/")

        # Remove empty maps/ directory
        try:
            maps_dir.rmdir()
            print(f"  Removed empty maps/ directory")
        except OSError:
            # Not empty, list remaining
            remaining = list(maps_dir.rglob("*"))
            if remaining:
                print(f"  WARNING: maps/ not empty, left in place:")
                for r in remaining:
                    print(f"    {r.relative_to(src)}")

    print(f"  {code} migration complete")
    return True


def main():
    if len(sys.argv) > 1:
        codes = sys.argv[1:]
    else:
        # Auto-detect: all lang/<code> with language.ini except _template
        codes = sorted(
            d.name for d in LANG_DIR.iterdir()
            if d.is_dir() and d.name != "_template" and (d / "language.ini").exists()
        )

    if not codes:
        print("No languages to migrate")
        return

    print(f"Migrating languages: {', '.join(codes)}")
    print()

    for code in codes:
        migrate_code(code)
        print()

    print("Migration complete!")
    print("Next steps:")
    print("  1. Translate lang/<code>/installer/texts.ini for each language")
    print("  2. Verify game/ structure looks correct")
    print("  3. Run: python tools/check_lang.py <code>")
    print("  4. Test installer: python tools/installer.py --silent --lang <code> --target test")


if __name__ == "__main__":
    main()