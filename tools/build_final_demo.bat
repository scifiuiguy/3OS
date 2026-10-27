@echo off
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_final_demo.ps1"
cd /d "%~dp0"
pause
