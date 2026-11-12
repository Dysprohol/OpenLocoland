@echo off
rem Look into the test rig's running sandbox: a window on your desktop, minimised to the taskbar.
rem "peek.cmd -Front" shows it at once. Closing the window closes only the view, not the sandbox.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0rig.ps1" -Peek %*
