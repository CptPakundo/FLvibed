@echo off
rem SDL3 for FLvibed's controller host (WORK_QUEUE Q5; the opt-in gamepad backend `pad_backend` 1,
rem controller\src\host\sdlpad.cpp). Usage: build_sdl3.cmd <version>   - called by build.cmd (dev, release) with the
rem Visual Studio x86 environment already set (VSPATH, vcvarsall x86); never needed by hand.
rem Builds the official release source (pinned: tools\sdl3\fetch_sdl3.ps1 puts it into research\repos\SDL3-<version>\)
rem ONCE per version and recipe into build\sdl3\<version>\: lib\SDL3-static.lib + include\SDL3\ (the public headers),
rem stamp.txt = what it was built with; a matching stamp = cached, nothing runs. Nothing is installed anywhere.
rem The options are the 29 Oct 2026 measurement's (research\evidence\sdl3\build_results.md, build_sdl_size.cmd):
rem MinSizeRel (/O1), x86, the static C runtime /MT, no SDL DLL, audio / video / gpu / render / camera / power / sensor /
rem dialog / tray / offscreen off, joystick + HIDAPI on, haptic off. A change of options bumps RECIPE (a rebuild).
rem The configure / build output goes to build\sdl3\<version>\sdl3_build.log (only a summary line is printed).
setlocal
set "VER=%~1"
if "%VER%"=="" (
  echo build_sdl3.cmd: no version given
  exit /b 1
)
set RECIPE=1
set "ROOT=%~dp0..\.."
set "SRC=%ROOT%\research\repos\SDL3-%VER%\SDL3-%VER%"
set "OUT=%ROOT%\build\sdl3\%VER%"
set "STAMP=SDL3 %VER% recipe %RECIPE%: MinSizeRel x86 MT static, gamepad only"
if exist "%OUT%\lib\SDL3-static.lib" if exist "%OUT%\include\SDL3\SDL_gamepad.h" (
  findstr /x /c:"%STAMP%" "%OUT%\stamp.txt" >nul 2>nul && exit /b 0
)

if not exist "%SRC%\include\SDL3\SDL_version.h" (
  echo SDL3 %VER%: no source at research\repos\SDL3-%VER%\SDL3-%VER% - run tools\sdl3\fetch_sdl3.ps1 in PowerShell: the pinned official release, its SHA-256 checked
  exit /b 1
)
rem (the header has LF line ends, where findstr's $ never matches: the number, and no further digit after it)
set "VH=%SRC%\include\SDL3\SDL_version.h"
for /f "tokens=1-3 delims=." %%a in ("%VER%") do (
  findstr /r /c:"define SDL_MAJOR_VERSION  *%%a" "%VH%" >nul || goto badversion
  findstr /r /c:"define SDL_MAJOR_VERSION  *%%a[0-9]" "%VH%" >nul && goto badversion
  findstr /r /c:"define SDL_MINOR_VERSION  *%%b" "%VH%" >nul || goto badversion
  findstr /r /c:"define SDL_MINOR_VERSION  *%%b[0-9]" "%VH%" >nul && goto badversion
  findstr /r /c:"define SDL_MICRO_VERSION  *%%c" "%VH%" >nul || goto badversion
  findstr /r /c:"define SDL_MICRO_VERSION  *%%c[0-9]" "%VH%" >nul && goto badversion
)

set "CM=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "NJ=%VSPATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
if not exist "%CM%" (
  echo SDL3 %VER%: Visual Studio's CMake not found - install the "C++ CMake tools for Windows" component
  exit /b 1
)
if not exist "%NJ%" (
  echo SDL3 %VER%: Visual Studio's Ninja not found - install the "C++ CMake tools for Windows" component
  exit /b 1
)

if exist "%OUT%" rmdir /s /q "%OUT%"
mkdir "%OUT%\lib" || exit /b 1
set "LOG=%OUT%\sdl3_build.log"
set "T0=%TIME%"
"%CM%" -S "%SRC%" -B "%OUT%\cmake" -G Ninja "-DCMAKE_MAKE_PROGRAM=%NJ%" -DCMAKE_BUILD_TYPE=MinSizeRel ^
 -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded ^
 -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_INSTALL=OFF ^
 -DSDL_AUDIO=OFF -DSDL_VIDEO=OFF -DSDL_GPU=OFF -DSDL_RENDER=OFF -DSDL_CAMERA=OFF ^
 -DSDL_JOYSTICK=ON -DSDL_HIDAPI=ON -DSDL_HIDAPI_JOYSTICK=ON -DSDL_POWER=OFF -DSDL_SENSOR=OFF -DSDL_DIALOG=OFF ^
 -DSDL_TRAY=OFF -DSDL_OFFSCREEN=OFF -DSDL_HAPTIC=OFF >"%LOG%" 2>&1 || goto failed
"%CM%" --build "%OUT%\cmake" >>"%LOG%" 2>&1 || goto failed
copy /y "%OUT%\cmake\SDL3-static.lib" "%OUT%\lib\" >nul || goto failed
xcopy /e /i /q /y "%SRC%\include\SDL3" "%OUT%\include\SDL3" >nul || goto failed
> "%OUT%\stamp.txt" echo %STAMP%
for %%f in ("%OUT%\lib\SDL3-static.lib") do echo SDL3 %VER% built into build\sdl3\%VER%\lib\SDL3-static.lib: %%~zf bytes ^(started %T0%, done %TIME%^)
exit /b 0

:badversion
echo SDL3 %VER%: research\repos\SDL3-%VER%\SDL3-%VER% is not version %VER% (SDL_version.h)
exit /b 1

:failed
echo SDL3 %VER%: the build failed - see build\sdl3\%VER%\sdl3_build.log
exit /b 1
