@echo off
rem Double-click to copy the corrected boat.ini files into ..\bin\Models.
rem Drag a Models folder onto this file to use that folder instead.
if "%~1"=="" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Apply-ModelFixes.ps1"
) else (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Apply-ModelFixes.ps1" -ModelsPath "%~1"
)
pause
