# fonts/ - letters the base's fonts do not have

Only for a pack whose letters are missing from the fonts of the language it is built on (Czech:
`lang/cs/fonts/`). `<font>.png` is a picture the size of the game's `fonts\<font>.bmp` with only
the marks of those letters, each in the 16 x 16 grid cell of its byte in the pack's code page:
white the letters' colour, grey 128 the shadow of font-Objectives, black the background,
transparent the base font's pixel. The build makes each such cell from the base font's own
Latin letter with these pixels painted over it, in every font whose name starts with the
sheet's (`font8x13.png` also `font8x13light.bmp`). See "Fonts" in `docs/TRANSLATING.md`;
`python tools\font_cells.py preview <code>` shows the result. Delete this folder if the pack
needs none.
