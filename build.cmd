@echo off
REM Builds CSB on Windows. Runs build.ps1 with the execution policy bypassed
REM for this one process, so it works even where PowerShell scripts are
REM disabled by default. Nothing is changed on the system.
REM
REM Usage:  build.cmd            build
REM         build.cmd -Clean     rebuild from scratch
REM         build.cmd -Run       build, then start in Dungeons\Dungeonmaster

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*
exit /b %ERRORLEVEL%
