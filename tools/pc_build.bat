@echo off
rem PC builds (MSVC, VS 2022 Build Tools, 32-bit like SIDPLAY/w):
rem   build\pc\sid2wav_ref.exe    unchanged libsidplay 1.36.60 from vendor\ (the reference)
rem   build\pc\sid2wav.exe        our working copy in src\core
rem   build\pc\sid2wav_fixed.exe  src\core with the fixed-point filter (as on the Palm)
rem   build\pc\sidcore.dll        the Palm core (src\blob) for the Palm OS Simulator, which runs
rem                               "native" code as x86 DLLs (copied next to the TX simulator)
rem Usage: pc_build.bat [ref|core|fixed|dll|all]   (default all)
setlocal
cd /d "%~dp0.."
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist build\pc mkdir build\pc
"%VSWHERE%" -products * -latest -property installationPath > build\pc\vs_path.txt
set VS=
set /p VS=<build\pc\vs_path.txt
if not defined VS (echo No Visual Studio C++ tools found & exit /b 1)
set "PATH=%PATH%;%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
call "%VS%\VC\Auxiliary\Build\vcvars32.bat" > nul || exit /b 1
set WHAT=%1
if "%WHAT%"=="" set WHAT=all
if "%WHAT%"=="ref"   call :build vendor\src\libsidplay-1.36.60\src ref sid2wav_ref "" || exit /b 1
if "%WHAT%"=="core"  call :build src\core core sid2wav "" || exit /b 1
if "%WHAT%"=="fixed" if not "%2"=="" call :build src\core fixed%2 sid2wav_fixed%2 "/DSID_FIXED_FILTER /DFILTER_BITS=%2" || exit /b 1
if "%WHAT%"=="fixed" if "%2"=="" call :build src\core fixed sid2wav_fixed "/DSID_FIXED_FILTER" || exit /b 1
if "%WHAT%"=="dll"   call :dll || exit /b 1
if "%WHAT%"=="all" (
  call :build vendor\src\libsidplay-1.36.60\src ref sid2wav_ref "" || exit /b 1
  call :build src\core core sid2wav "" || exit /b 1
  call :build src\core fixed sid2wav_fixed "/DSID_FIXED_FILTER" || exit /b 1
  call :dll || exit /b 1
)
exit /b 0

rem :dll -- the core as the Palm will run it (memory loading only), as a DLL for the simulator
:dll
set SRC=src\core
if not exist build\pc\dll mkdir build\pc\dll
cl /nologo /O2 /EHsc /W1 /wd4838 /LD /DSIDCORE_DLL /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_WARNINGS ^
   /Isrc\blob\config /I%SRC% /Isrc\blob ^
   src\blob\sidcore.cpp %SRC%\6510_.cpp %SRC%\6581_.cpp %SRC%\eeconfig.cpp %SRC%\envelope.cpp ^
   %SRC%\fformat_.cpp %SRC%\mixing.cpp %SRC%\mus_.cpp %SRC%\player.cpp %SRC%\psid_.cpp ^
   %SRC%\samples.cpp %SRC%\sidtune.cpp ^
   /Fobuild\pc\dll\ /Febuild\pc\sidcore.dll > build\pc\sidcore.log 2>&1
if errorlevel 1 (type build\pc\sidcore.log & exit /b 1)
set "SIM=%LOCALAPPDATA%\Programs\PalmDev\simulators\Palm TX"
if exist "%SIM%" copy /y build\pc\sidcore.dll "%SIM%\" > nul
exit /b 0

rem :build <source dir> <object dir> <exe name> <extra defines>
rem (the source variable is not called LIB: the linker uses that one)
:build
set SRC=%1
if not exist build\pc\%2 mkdir build\pc\%2
cl /nologo /O2 /EHsc /W1 /wd4838 /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_WARNINGS %~4 %EXTRA% ^
   /Isrc\pc\config /I%SRC% ^
   src\pc\sid2wav.cpp %SRC%\6510_.cpp %SRC%\6581_.cpp %SRC%\eeconfig.cpp %SRC%\envelope.cpp ^
   %SRC%\fformat_.cpp %SRC%\info_.cpp %SRC%\mixing.cpp %SRC%\mus_.cpp %SRC%\player.cpp ^
   %SRC%\pp_.cpp %SRC%\psid_.cpp %SRC%\samples.cpp %SRC%\sid_.cpp %SRC%\sidtune.cpp ^
   /Fobuild\pc\%2\ /Febuild\pc\%3.exe > build\pc\%3.log 2>&1
if errorlevel 1 (type build\pc\%3.log & exit /b 1)
exit /b 0
