@echo off
rem Starts a map of this game without the intro and the menu (LocoFix's test map).
rem Double-click starts the sample map; from a console: run_map maps\l01\l01.map
rem The launcher comes first, as at every start: OK starts the map with the settings chosen there.
setlocal
set "MAP=%~1"
if "%MAP%"=="" set "MAP=maps\sandbox\sandbox.map"
rem /D: the game works in its own folder (its files and logs\), wherever this is run from
start "" /D "%~dp0" "%~dp0Locoland.exe" --test-map "%MAP%"
