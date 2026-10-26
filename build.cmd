@echo off
rem Builds FLvibed (32-bit, static C runtime: players need no Visual C++ redistributable).
rem Usage: build.cmd [dev|impl|release]
rem   dev      (default) build\dev\: FLvibed.dll (the modules' hosts: hooks, debug channels) plus the modules'
rem            logic as FLSubtitlesImpl.dll and FLControllerImpl.dll, which hot-reload in the running game
rem            (tools\deploy.ps1 -Impl)
rem   impl     only the two logic DLLs of the dev build
rem   release  build\release\FLvibed.dll: everything in one DLL (FLV_STATIC: logic linked in, no reload)
rem   sigtool  build\sigtool\sigtool.exe: Freelancer.exe signatures for core\flexe.h (tools\sigtool\sigtool.cpp)
rem   test     build\test\fltest.exe: the host-free unit tests (tests\*.cpp + the pure core sources, compiled plain with
rem            tests\space_stub.cpp for the game), then runs them; exit code = the tests'. `build.cmd test "<install>"`
rem            names the install for the data tests (else FLVIBED_INSTALL, else Install\Freelancer HD Edition; absent =
rem            those tests are skipped)
rem Each module source is compiled through a generated wrapper that puts it into its own namespace
rem (flv_<module>_<host|impl>, core\pre.h), so the modules link into one DLL without clashing names.
rem Shared code (core\*.cpp, core\game\*.cpp) is compiled into every module part that uses it. /MP compiles the sources
rem of each cl invocation in parallel (measured 24 Oct 2026: a full dev build 69 s -> 15 s, release 13 s).
setlocal
set TARGET=%1
if "%TARGET%"=="" set TARGET=dev

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSPATH="
if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`call "%VSWHERE%" -latest -products * -property installationPath`) do set "VSPATH=%%i"
)
if not defined VSPATH (
  echo Visual Studio with C++ tools not found
  exit /b 1
)
call "%VSPATH%\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul 2>nul || exit /b 1

cd /d "%~dp0"
set CFLAGS=/nologo /MP /std:c++17 /O2 /MT /EHsc /W4 /Zi /D_CRT_SECURE_NO_WARNINGS /Icore
set LFLAGS=/DLL /DEBUG /OPT:REF /OPT:ICF
set LIBS=user32.lib kernel32.lib gdi32.lib gdiplus.lib ole32.lib ws2_32.lib

if /i "%TARGET%"=="release" goto release
if /i "%TARGET%"=="sigtool" goto sigtool
if /i "%TARGET%"=="setup" goto setup
if /i "%TARGET%"=="test" goto test
if /i "%TARGET%"=="impl" goto impl
if /i not "%TARGET%"=="dev" (
  echo unknown target %TARGET%: dev, impl or release
  exit /b 1
)

rem ---------------------------------------------------------------- dev
call :clean dev
call :wrap dev flv_subtitles_host "subtitles\src\host\*.cpp core\hook.cpp core\log.cpp core\text.cpp"
call :wrap dev flv_controller_host "controller\src\host\*.cpp core\hook.cpp core\log.cpp core\text.cpp"
call :link dev "" || exit /b 1
:impl
call :logic subtitles FLSubtitlesImpl || exit /b 1
call :logic controller FLControllerImpl || exit /b 1
exit /b 0

rem ---------------------------------------------------------------- release
:release
call :clean release
call :wrap release flv_subtitles_host "subtitles\src\host\*.cpp core\hook.cpp core\log.cpp core\text.cpp"
call :wrap release flv_subtitles_impl "subtitles\src\impl\*.cpp core\hook.cpp core\log.cpp core\text.cpp core\game\*.cpp"
call :wrap release flv_controller_host "controller\src\host\*.cpp core\hook.cpp core\log.cpp core\text.cpp"
call :wrap release flv_controller_impl "controller\src\impl\*.cpp core\hook.cpp core\log.cpp core\text.cpp core\game\*.cpp"
call :link release "/DFLV_STATIC" || exit /b 1
exit /b 0

rem ---------------------------------------------------------------- sigtool
:sigtool
if not exist build\sigtool\obj mkdir build\sigtool\obj
cl %CFLAGS% /Fo:build\sigtool\obj\ /Fd:build\sigtool\obj\ tools\sigtool\sigtool.cpp core\hook.cpp core\log.cpp ^
   /link /DEBUG /OUT:build\sigtool\sigtool.exe /PDB:build\sigtool\sigtool.pdb %LIBS% || exit /b 1
exit /b 0

rem ---------------------------------------------------------------- setup
:setup
rem The players' installer / uninstaller (tools\setup): carries build\release\FLvibed.dll and the shipped data as
rem resources, so it needs a release build first (tools\package.ps1 runs both).
if not exist build\release\FLvibed.dll (
  echo setup needs build\release\FLvibed.dll: build.cmd release first
  exit /b 1
)
if not exist build\setup\obj mkdir build\setup\obj
rc /nologo /Icore /fo build\setup\obj\setup.res tools\setup\setup.rc || exit /b 1
cl %CFLAGS% /DUNICODE /D_UNICODE /Fo:build\setup\obj\ /Fd:build\setup\obj\ tools\setup\setup.cpp build\setup\obj\setup.res ^
   /link /SUBSYSTEM:WINDOWS /MANIFEST:NO /DEBUG /OUT:build\setup\FLvibed-Setup.exe /PDB:build\setup\FLvibed-Setup.pdb ^
   %LIBS% comctl32.lib shell32.lib advapi32.lib version.lib || exit /b 1
exit /b 0

rem ---------------------------------------------------------------- test
:test
rem The tests and the core sources they exercise, plain (no namespace wrapper, no pre.h): mission (its CreateID is in-tree,
rem core\game\flhash.cpp), ini, utf, avoid, route, flight; tests\space_stub.cpp stands in for space.cpp (the game).
if not exist build\test\obj mkdir build\test\obj
cl %CFLAGS% /Fo:build\test\obj\ /Fd:build\test\obj\ tests\*.cpp core\game\flhash.cpp core\game\mission.cpp core\game\ini.cpp ^
   core\game\utf.cpp core\game\avoid.cpp core\game\route.cpp core\game\flight.cpp core\hook.cpp core\log.cpp core\fluf.cpp ^
   /link /DEBUG /OUT:build\test\fltest.exe /PDB:build\test\fltest.pdb %LIBS% || exit /b 1
build\test\fltest.exe %2
exit /b %ERRORLEVEL%

rem ---------------------------------------------------------------- helpers
:clean <config>
if exist build\%1\wrap rmdir /s /q build\%1\wrap
if exist build\%1\obj\flvibed rmdir /s /q build\%1\obj\flvibed
mkdir build\%1\wrap
mkdir build\%1\obj\flvibed
exit /b 0

:wrap <config> <namespace> <sources>
for %%f in (%~3) do (
  > "build\%1\wrap\%2_%%~nf.cpp" (
    echo #include "../../../core/pre.h"
    echo namespace %2 {
    echo #include "%%~ff"
    echo }
  )
)
exit /b 0

:link <config> <defines>
rem the version resource (core\flvibed.rc from core\version.h) goes into FLvibed.dll
rc /nologo /Icore /fo build\%1\obj\flvibed\flvibed.res core\flvibed.rc || exit /b 1
cl %CFLAGS% %~2 /Fo:build\%1\obj\flvibed\ /Fd:build\%1\obj\flvibed\ core\flvibed.cpp core\settings_bus.cpp core\hud_bus.cpp core\fluf.cpp build\%1\wrap\*.cpp ^
   build\%1\obj\flvibed\flvibed.res /link %LFLAGS% /OUT:build\%1\FLvibed.dll /PDB:build\%1\FLvibed.pdb %LIBS% || exit /b 1
exit /b 0

:logic <module> <dll name>
if not exist build\dev\obj\%2 mkdir build\dev\obj\%2
cl %CFLAGS% /Fo:build\dev\obj\%2\ /Fd:build\dev\obj\%2\ %1\src\impl\*.cpp core\hook.cpp core\log.cpp core\text.cpp core\game\*.cpp ^
   /link %LFLAGS% /OUT:build\dev\%2.dll /PDB:build\dev\%2.pdb %LIBS% || exit /b 1
exit /b 0
