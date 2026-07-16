@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

set "PYTHON_EXE="
for /f "delims=" %%P in ('where python.exe 2^>nul') do (
    if not defined PYTHON_EXE set "PYTHON_EXE=%%P"
)

if not defined PYTHON_EXE (
    for /d %%D in ("%LocalAppData%\Programs\Python\Python3*") do (
        if not defined PYTHON_EXE if exist "%%~fD\python.exe" set "PYTHON_EXE=%%~fD\python.exe"
    )
)

if not defined PYTHON_EXE (
    for /d %%D in ("%ProgramFiles%\Python3*") do (
        if not defined PYTHON_EXE if exist "%%~fD\python.exe" set "PYTHON_EXE=%%~fD\python.exe"
    )
)

if not defined PYTHON_EXE (
    echo [ERROR] Python 3 was not found.
    echo Install Python 3 or add python.exe to PATH.
    goto :failed
)

echo [ENV] Python candidate: !PYTHON_EXE!
"!PYTHON_EXE!" -c "import sys; raise SystemExit(sys.version_info[0] - 3)" >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Python 3 is required.
    goto :failed
)

echo [START] Loading config.json and starting serial capture...
"!PYTHON_EXE!" "serial_logger.py" --config "config.json" %*
if errorlevel 1 goto :failed

endlocal
exit /b 0

:failed
echo.
echo [ERROR] Serial logger setup or execution failed.
echo Review the message above. Press any key to close this window.
pause >nul
endlocal
exit /b 1
