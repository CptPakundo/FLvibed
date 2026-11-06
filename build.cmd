@echo off
rem Builds FLvibed (32-bit, static C runtime: players need no Visual C++ redistributable).
rem Usage: build.cmd [dev|impl|release|sigtool|test|setup]
rem   dev      (default) build\dev\: FLvibed.dll (the modules' hosts: hooks, debug channels) plus the logic of every
rem            module as FLvibedLogic.dll, which hot-reloads in the running game (tools\deploy.ps1 -Impl)
rem   impl     only the logic DLL of the dev build
rem   release  build\release\FLvibed.dll: everything in one DLL (FLV_STATIC: logic linked in, no reload)
rem   sigtool  build\sigtool\sigtool.exe: Freelancer.exe signatures for core\flexe.h (tools\sigtool\sigtool.cpp)
rem   flhook   build\flhook\flvibed_server.dll: the FLHook 4.0 plugin a server installs to tell FLvibed's clients what it
rem            allows the autopilot (server\flhook\README.md)
rem   mptest   build\mptest\flmptest.dll + flmplaunch.exe: TEST-ONLY helpers for the local multiplayer setup (tools\mptest\README.md);
rem            never shipped
rem   test     build\test\fltest.exe: the host-free unit tests (tests\*.cpp + the pure core sources, compiled plain with
rem            tests\space_stub.cpp for the game), then runs them; exit code = the tests'. `build.cmd test "<install>"`
rem            names the install for the data tests (else FLVIBED_INSTALL, else Install\Freelancer HD Edition; absent =
rem            those tests are skipped)
rem Each module source is compiled through a generated wrapper that puts it into its own namespace
rem (flv_<module>_<host|impl>, core\pre.h), so the modules link into one DLL without clashing names. core\game and the
rem core sources of the logic are compiled once, at global scope (:wraplogic); the host has one instance of them too, in
rem namespace flv_host (:wraphost). /MP compiles the sources
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
rem The toolset is pinned for every target (the owner, 6 Nov 2026): VS 2026's default no longer targets Windows 7, and its headers have no Windows 7
rem level. A missing pin stops the build; so does a toolset whose yvals_core.h lost that level. tools\win7_check.ps1 refuses a binary with a newer part.
set MSVC_PIN=14.44
set "VCToolsVersion="
call "%VSPATH%\VC\Auxiliary\Build\vcvarsall.bat" x86 -vcvars_ver=%MSVC_PIN% >nul 2>nul
if not defined VCToolsVersion goto badtoolset
echo %VCToolsVersion%| findstr /b /c:"%MSVC_PIN%." >nul || goto badtoolset
findstr /c:"_STL_WIN32_WINNT_WIN7" "%VCToolsInstallDir%include\yvals_core.h" >nul || goto badtoolset

cd /d "%~dp0"
set CFLAGS=/nologo /MP /std:c++17 /O2 /MT /EHsc /W4 /Zi /D_CRT_SECURE_NO_WARNINGS /Icore
set LFLAGS=/DLL /DEBUG /OPT:REF /OPT:ICF /MAP
set LIBS=user32.lib kernel32.lib gdi32.lib gdiplus.lib ole32.lib ws2_32.lib
rem SDL3 (WORK_QUEUE Q5: the opt-in gamepad backend `pad_backend` 1, modules\controller\src\host\sdlpad.cpp): a static library in
rem the controller HOST only (its wrappers define FLV_SDL3, core\pre.h), built once per version by tools\sdl3\build_sdl3.cmd
rem into build\sdl3\<version>\ (cached; the source: tools\sdl3\fetch_sdl3.ps1). Its headers are external (no warnings).
set SDL3_VER=3.4.18
set SDL3_DIR=build\sdl3\%SDL3_VER%
set SDL3_CFLAGS=/external:I %SDL3_DIR%\include /external:W0
set SDL3_LIBS=%SDL3_DIR%\lib\SDL3-static.lib winmm.lib setupapi.lib shell32.lib advapi32.lib oleaut32.lib version.lib imm32.lib uuid.lib

if /i "%TARGET%"=="release" goto release
if /i "%TARGET%"=="sigtool" goto sigtool
if /i "%TARGET%"=="mptest" goto mptest
if /i "%TARGET%"=="flhook" goto flhook
if /i "%TARGET%"=="setup" goto setup
if /i "%TARGET%"=="test" goto test
if /i "%TARGET%"=="impl" goto impl
if /i not "%TARGET%"=="dev" (
  echo unknown target %TARGET%: dev, impl, release, sigtool, flhook, mptest, test or setup
  exit /b 1
)

rem ---------------------------------------------------------------- dev
call tools\sdl3\build_sdl3.cmd %SDL3_VER% || exit /b 1
call :clean dev
call :wraphost dev
call :link dev "" || exit /b 1
:impl
call :logicdll || exit /b 1
exit /b 0

rem ---------------------------------------------------------------- release
:release
call tools\sdl3\build_sdl3.cmd %SDL3_VER% || exit /b 1
call :clean release
call :wraphost release
call :wraplogic release wrap
call :link release "/DFLV_STATIC" || exit /b 1
exit /b 0

rem ---------------------------------------------------------------- sigtool
:flhook
if not exist build\flhook\obj mkdir build\flhook\obj
lib /nologo /def:server\flhook\FLHook.def /machine:x86 /out:build\flhook\obj\FLHook.lib >nul || exit /b 1
cl %CFLAGS% /Fo:build\flhook\obj\ /Fd:build\flhook\obj\ server\flhook\flvibed_server.cpp ^
   /link %LFLAGS% /OUT:build\flhook\flvibed_server.dll /PDB:build\flhook\flvibed_server.pdb %LIBS% build\flhook\obj\FLHook.lib || exit /b 1
exit /b 0

:mptest
if not exist build\mptest\obj mkdir build\mptest\obj
cl %CFLAGS% /Fo:build\mptest\obj\ /Fd:build\mptest\obj\ tools\mptest\flmptest.cpp ^
   /link %LFLAGS% /OUT:build\mptest\flmptest.dll /PDB:build\mptest\flmptest.pdb %LIBS% advapi32.lib || exit /b 1
cl %CFLAGS% /Fo:build\mptest\obj\ /Fd:build\mptest\obj\ tools\mptest\flmplaunch.cpp ^
   /link /DEBUG /OUT:build\mptest\flmplaunch.exe /PDB:build\mptest\flmplaunch.pdb %LIBS% || exit /b 1
exit /b 0

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
rem core\game\flhash.cpp), mission_live (its walk over a fake memory), ini, utf, avoid, route, flight, the pad presets;
rem tests\space_stub.cpp stands in for space.cpp (the game), tests\presets_test.cpp for the glyph names; the situation
rem assessor (A37 S1: situation.cpp, the job table core\game\jobs.cpp) over the recorded snapshots tests\fixtures\situation,
rem the arbiter's rules (A37 S3: situation_rules.cpp), the goal queue (A37 S6: goalqueue.cpp), the executor's pure half
rem (A37 S7: situation_exec.cpp), LOOK's pure verdict (L0: lookahead.cpp), ASH-RIDE's pure half (ash_ride.cpp).
if not exist build\test\obj mkdir build\test\obj
cl %CFLAGS% /Fo:build\test\obj\ /Fd:build\test\obj\ tests\*.cpp core\game\flhash.cpp core\game\mission.cpp core\game\mission_live.cpp core\game\ini.cpp core\game\universe_data.cpp ^
   core\settings_bus.cpp core\game\utf.cpp core\game\avoid.cpp core\game\route.cpp core\game\flight.cpp core\game\jobs.cpp core\hook.cpp core\log.cpp core\fluf.cpp ^
   core\ui\presets.cpp modules\autopilot\src\situation.cpp modules\autopilot\src\situation_rules.cpp modules\autopilot\src\goalqueue.cpp modules\autopilot\src\situation_exec.cpp modules\autopilot\src\lookahead.cpp modules\autopilot\src\ash_ride.cpp ^
   modules\subtitles\src\impl\linebreak.cpp ^
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

:wraphost <config>
rem The host: its core once (namespace flv_host: core\host, core\hook, log and text), then a part per module in the module's own
rem namespace, seeing the core through core\host\all.h. The logic has its own instance of hook, log and text.
for %%f in (core\host\*.cpp core\hook.cpp core\log.cpp core\text.cpp) do (
  > "build\%1\wrap\flv_host_%%~nf.cpp" (
    echo #include "../../../core/pre.h"
    echo namespace flv_host {
    echo #include "%%~ff"
    echo }
  )
)
call :wrappart %1 flv_subtitles_host "modules\subtitles\src\host\*.cpp"
call :wrappart %1 flv_controller_host "modules\controller\src\host\*.cpp" FLV_SDL3
call :wrappart %1 flv_display_host "modules\display\src\*.cpp"
call :wrappart %1 flv_movies_host "modules\movies\src\host\*.cpp"
call :wrappart %1 flv_autopilot_host "modules\autopilot\src\host\*.cpp"
exit /b 0

:wrappart <config> <namespace> <sources> [<macro defined before pre.h: FLV_SDL3 = the SDL3 headers>]
for %%f in (%~3) do (
  > "build\%1\wrap\%2_%%~nf.cpp" (
    if not "%~4"=="" echo #define %~4 1
    echo #include "../../../core/pre.h"
    echo namespace flv_host {
    echo #include "host/all.h"
    echo }
    echo namespace %2 {
    echo using namespace flv_host;
    echo #include "host/part_log.h"
    echo #include "%%~ff"
    echo }
  )
)
exit /b 0

:link <config> <defines>
rem the version resource (core\flvibed.rc from core\version.h) goes into FLvibed.dll
rc /nologo /Icore /fo build\%1\obj\flvibed\flvibed.res core\flvibed.rc || exit /b 1
cl %CFLAGS% %SDL3_CFLAGS% %~2 /Fo:build\%1\obj\flvibed\ /Fd:build\%1\obj\flvibed\ core\flvibed.cpp core\loader.cpp core\settings_bus.cpp core\hud_bus.cpp core\fluf.cpp build\%1\wrap\*.cpp ^
   build\%1\obj\flvibed\flvibed.res /link %LFLAGS% /OUT:build\%1\FLvibed.dll /PDB:build\%1\FLvibed.pdb %LIBS% %SDL3_LIBS% || exit /b 1
exit /b 0

:wraplogic <config> <folder>
rem The logic, the same in dev and release: the core sources once, at global scope; each module in its namespace, with every
rem core header included at global scope first (core_all.h, generated: a header left out would land inside the namespace,
rem and an inline variable in it would be a second, silent copy: tools\core_once.py) and its own log channel.
> build\%1\%2\core_all.h (
  for %%f in (core\*.h core\game\*.h core\ui\*.h) do echo #include "%%~ff"
)
call :wrapmodule %1 %2 flv_subtitles_impl "modules\subtitles\src\impl\*.cpp"
call :wrapmodule %1 %2 flv_controller_impl "modules\controller\src\impl\*.cpp"
call :wrapmodule %1 %2 flv_npc_impl "modules\npc\src\*.cpp"
call :wrapmodule %1 %2 flv_speedrun_impl "modules\speedrun\src\*.cpp"
call :wrapmodule %1 %2 flv_autopilot_impl "modules\autopilot\src\*.cpp"
call :wrapmodule %1 %2 flv_hud_impl "modules\hud\src\*.cpp"
call :wrapmodule %1 %2 flv_movies_impl "modules\movies\src\impl\*.cpp"
for %%f in (core\hook.cpp core\log.cpp core\text.cpp core\game\*.cpp core\ui\*.cpp core\logic\*.cpp) do (
  > "build\%1\%2\flv_core_%%~nf.cpp" (
    echo #include "../../../core/pre.h"
    echo #include "%%~ff"
  )
)
exit /b 0

:wrapmodule <config> <folder> <namespace> <sources>
for %%f in (%~4) do (
  > "build\%1\%2\%3_%%~nf.cpp" (
    echo #include "../../../core/pre.h"
    echo #include "core_all.h"
    echo namespace %3 {
    echo #include "logic/module_log.h"
    echo #include "%%~ff"
    echo }
  )
)
exit /b 0

:logicdll
if exist build\dev\wraplogic rmdir /s /q build\dev\wraplogic
mkdir build\dev\wraplogic
if not exist build\dev\obj\FLvibedLogic mkdir build\dev\obj\FLvibedLogic
call :wraplogic dev wraplogic
cl %CFLAGS% /Fo:build\dev\obj\FLvibedLogic\ /Fd:build\dev\obj\FLvibedLogic\ build\dev\wraplogic\*.cpp ^
   /link %LFLAGS% /OUT:build\dev\FLvibedLogic.dll /PDB:build\dev\FLvibedLogic.pdb %LIBS% || exit /b 1
exit /b 0

:badtoolset
echo.
echo BUILD REFUSED: MSVC %MSVC_PIN% is missing, or the toolset found no longer targets Windows 7 (found: "%VCToolsVersion%").
echo FLvibed is built with MSVC %MSVC_PIN% so that it keeps loading on Windows 7. In the Visual Studio Installer: Modify, Individual components,
echo "MSVC v143 - VS 2022 C++ x64/x86 build tools (v%MSVC_PIN%)". It installs beside the default toolset and changes nothing else.
exit /b 1
