@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Check-Package.ps1"
if errorlevel 1 goto failed
echo Starting Brave Frontier server on http://127.0.0.1:9960
echo Leave this window open while playing. Close it before copying your save.
"%~dp0gimuserverw.exe" "%~dp0config.json"
if errorlevel 1 goto failed
exit /b 0
:failed
echo.
echo Server did not start or exited with an error. Read the message above.
pause
exit /b 1
