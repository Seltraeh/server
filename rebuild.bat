@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\Build-Dev.ps1" %*
exit /b %ERRORLEVEL%
