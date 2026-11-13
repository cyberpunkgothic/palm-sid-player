@echo off
cd /d "%~dp0.."
set /p VS=<build\pc\vs_path.txt
set "PATH=%PATH%;%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
call "%VS%\VC\Auxiliary\Build\vcvars32.bat" > nul
cl /nologo /O1 /W1 /Isrc\blob tests\settings_check.c /Fobuild\pc\ /Febuild\pc\settings_check.exe > build\pc\settings.log 2>&1 || type build\pc\settings.log
