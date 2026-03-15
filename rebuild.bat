@echo off
setlocal

set "SERVER_DIR=%~dp0"
set "VCPKG_ROOT=C:\Users\Evan\BF\vcpkg"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

:: ── Locate VsDevCmd.bat via vswhere (VS 2017-2026+) ──────────────────────────
if not exist "%VSWHERE%" (
    echo ERROR: vswhere.exe not found. Visual Studio does not appear to be installed.
    pause & exit /b 1
)

for /f "usebackq tokens=*" %%i in (
    `"%VSWHERE%" -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`
) do set "VS_PATH=%%i"

if not defined VS_PATH (
    echo ERROR: No Visual Studio installation with C++ tools found.
    echo Install the "Desktop development with C++" workload from the VS Installer.
    pause & exit /b 1
)

set "VSDEVCMD=%VS_PATH%\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEVCMD%" (
    echo ERROR: VsDevCmd.bat not found at:
    echo   %VSDEVCMD%
    pause & exit /b 1
)

:: ── Check vcpkg ───────────────────────────────────────────────────────────────
if not exist "%VCPKG_ROOT%\vcpkg.exe" (
    echo ERROR: vcpkg not found at %VCPKG_ROOT%
    echo Run the BF installer first to set up vcpkg.
    pause & exit /b 1
)

echo ============================================================
echo  BF Server Rebuild
echo  VS:    %VS_PATH%
echo  vcpkg: %VCPKG_ROOT%
echo ============================================================
echo.

:: Ask: configure + build, or just build?
set RECONFIGURE=N
set /p RECONFIGURE="Re-run CMake configure? (y/N, default N = build only): "
if /i "%RECONFIGURE%"=="y" goto :configure

:build_only
echo [1/1] Building (Debug)...
cmd /c ""%VSDEVCMD%" && cd /d "%SERVER_DIR%" && cmake --build . --config Debug"
goto :done

:configure
echo [1/2] Configuring (debug-win64 preset)...
cmd /c ""%VSDEVCMD%" && cd /d "%SERVER_DIR%" && cmake --preset debug-win64"
if errorlevel 1 (
    echo.
    echo ERROR: CMake configure failed.
    pause & exit /b 1
)
echo.
echo [2/2] Building (Debug)...
cmd /c ""%VSDEVCMD%" && cd /d "%SERVER_DIR%" && cmake --build . --config Debug"

:done
if errorlevel 1 (
    echo.
    echo BUILD FAILED. Check the output above for errors.
) else (
    echo.
    echo Build succeeded.
)
pause
endlocal
