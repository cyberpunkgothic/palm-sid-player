@echo off
rem Launch the Palm TX simulator (with the simulator copy of the sound core, sidcore.dll, beside it).
rem Install with right-click > Install > Database...: SIDPlayer.prc (standard interface)
rem and/or SIDPlayerDX.prc (C64 style), both in the project folder.  They are separate apps: both can be installed.
rem Installing again replaces the old copy (installed apps stay between runs).
rem Test tunes go in the simulated SD card: <simulator folder>\Card1\SID
set "SIM=%LOCALAPPDATA%\Programs\PalmDev\simulators\Palm TX"
if not exist "%SIM%\PalmSim.exe" (echo Palm TX simulator not found in "%SIM%" & pause & exit /b 1)
if exist "%~dp0build\pc\sidcore.dll" copy /y "%~dp0build\pc\sidcore.dll" "%SIM%\" > nul
start "" /d "%SIM%" "%SIM%\PalmSim.exe"
