@echo off
rem Starts a map of this game in editor mode (LocoFix --editor): decals as objects, the ground
rem heights as authored, the editor's bar and Menu panel. No intro and no menu, as run_map.cmd.
rem Double-click opens the sample map; from a console: edit_map maps\l01\l01.map
rem (maps\template\template.map, as the desktop shortcut gives it, opens a new empty map).
setlocal
set "MAP=%~1"
if "%MAP%"=="" set "MAP=maps\sandbox\sandbox.map"
rem /D: the game works in its own folder (its files and logs\), wherever this is run from
start "" /D "%~dp0" "%~dp0Locoland.exe" --editor --test-map "%MAP%"
