@echo off
rem Starts the subtitle review tool: http://127.0.0.1:8765/ (the next free port if that one is busy).
rem Works from any folder and by double-click. Arguments go to review_server.py:
rem   review.cmd            start the server and open the page in the browser (owner, 26 Oct 2026; the page plays
rem                         each clip as it is shown); Ctrl+C stops it
rem   review.cmd --no-open  start the server without opening the browser
rem   review.cmd --check    check the prerequisites and load the data, then exit (0 = ready)
rem   review.cmd --help     every option
rem   review.cmd test       run the review tool's unit tests (seconds; no server, no audio)
rem The tool needs only Python's standard library (3.10+). Python is taken from the review folder's venv
rem (FLSubtitles\asr\.venv: "uv sync" in FLSubtitles\asr), else FLvibed's tool venv
rem (research\tools\ocr\.venv), else the Python launcher "py -3", else "python" on PATH.
setlocal
set "HERE=%~dp0"
set "NOARGS="
if "%~1"=="" set "NOARGS=1"
set "PYEXE="
set "PYARG="
if exist "%HERE%FLSubtitles\asr\.venv\Scripts\python.exe" set "PYEXE=%HERE%FLSubtitles\asr\.venv\Scripts\python.exe"
if not defined PYEXE if exist "%HERE%..\..\research\tools\ocr\.venv\Scripts\python.exe" set "PYEXE=%HERE%..\..\research\tools\ocr\.venv\Scripts\python.exe"
if not defined PYEXE (
    where py >nul 2>nul && (set "PYEXE=py" & set "PYARG=-3")
)
if not defined PYEXE (
    where python >nul 2>nul && set "PYEXE=python"
)
if not defined PYEXE (
    echo No Python found. Create the review venv: install uv, then run "uv sync" in "%HERE%FLSubtitles\asr".
    set "RC=9"
    goto :end
)
rem the browser opens by default; --no-open (taken out here), --open, --check and --help leave it to the arguments
set "OPEN=--open"
set "ARGS="
for %%a in (%*) do (
    if /i "%%~a"=="--no-open" (set "OPEN=") else (
        if /i "%%~a"=="--open" set "OPEN="
        if /i "%%~a"=="--check" set "OPEN="
        if /i "%%~a"=="--help" set "OPEN="
        if /i "%%~a"=="-h" set "OPEN="
        call set "ARGS=%%ARGS%% %%a"
    )
)
if /i "%~1"=="test" (
    "%PYEXE%" %PYARG% -I "%HERE%FLSubtitles\asr\tests\run_tests.py"
) else (
    "%PYEXE%" %PYARG% -I "%HERE%FLSubtitles\asr\review_server.py" %OPEN% %ARGS%
)
set "RC=%ERRORLEVEL%"
:end
rem double-clicked (no arguments, a console of its own): keep the window open on an error to read the message
if not "%RC%"=="0" if defined NOARGS (echo %CMDCMDLINE% | find /i "%~nx0" >nul && pause)
exit /b %RC%
