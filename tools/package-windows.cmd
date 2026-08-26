@echo off
rem SPDX-License-Identifier: MIT
rem
rem Runs package-windows.ps1 from cmd.exe, or from a double-click in Explorer.
rem
rem cmd.exe does not execute .ps1 files — it opens them with whatever program
rem is associated with the extension, which is why typing the script's name
rem produces an "open with" prompt instead of a build. This wrapper calls
rem PowerShell properly and passes any arguments straight through:
rem
rem     tools\package-windows.cmd
rem     tools\package-windows.cmd -QtDir C:\Qt\6.8.0\msvc2022_64
rem
rem -ExecutionPolicy Bypass applies to this one invocation only. It changes
rem nothing on the machine and needs no administrator rights; the default
rem policy blocks script *files* from running, which would otherwise stop this
rem before it started.

setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0package-windows.ps1" %*
set "PACKAGE_EXIT=%ERRORLEVEL%"

rem Keep the window open when this was double-clicked, so the result is
rem readable. When it was run from an existing prompt, the output stays on
rem screen anyway and a pause would just be in the way. Explorer launches the
rem file as `cmd /c "...\package-windows.cmd"`, so the command line mentioning
rem this file's own name is what distinguishes the two.
echo %cmdcmdline% | find /i "%~nx0" >nul
if not errorlevel 1 (
    echo.
    pause
)

exit /b %PACKAGE_EXIT%
