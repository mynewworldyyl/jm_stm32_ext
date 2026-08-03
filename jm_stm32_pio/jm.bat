@echo off
setlocal enabledelayedexpansion

if "%1"=="" (
    echo Usage: jm.bat [ACTION] [BOARD_NAME]
    echo Actions: compile, upload, monitor
    echo Example: jm.bat compile stm32f1_bluepill
    exit /b 1
)

if "%2"=="" (
    echo Error: Board name is required
    echo Usage: jm.bat [ACTION] [BOARD_NAME]
    echo Example: jm.bat compile stm32f1_bluepill
    exit /b 1
)

set ACTION=%1
set BOARD_NAME=%2
set ENV_NAME=

if "%BOARD_NAME%"=="stm32f1_bluepill" (
    set ENV_NAME=stm32f1_bluepill
) else if "%BOARD_NAME%"=="stm32f1_blackpill" (
    set ENV_NAME=stm32f1_blackpill
) else if "%BOARD_NAME%"=="stm32f103_nucleo64" (
    set ENV_NAME=stm32f103_nucleo64
) else if "%BOARD_NAME%"=="stm32f411_nucleo64" (
    set ENV_NAME=stm32f411_nucleo64
) else (
    echo Error: Unknown board "%BOARD_NAME%"
    echo Supported boards: stm32f1_bluepill, stm32f1_blackpill, stm32f103_nucleo64, stm32f411_nucleo64
    exit /b 1
)

set BASE_DIR=%~dp0

where pio >nul 2>&1
if %errorlevel% neq 0 (
    set "PIO_SCRIPT="
    if exist "%LOCALAPPDATA%\PlatformIO\penv\Scripts\pio.exe" (
        set "PIO_SCRIPT=%LOCALAPPDATA%\PlatformIO\penv\Scripts\pio.exe"
    ) else if exist "%USERPROFILE%\.platformio\penv\Scripts\pio.exe" (
        set "PIO_SCRIPT=%USERPROFILE%\.platformio\penv\Scripts\pio.exe"
    ) else if exist "%LOCALAPPDATA%\PlatformIO\penv\Scripts\platformio.exe" (
        set "PIO_SCRIPT=%LOCALAPPDATA%\PlatformIO\penv\Scripts\platformio.exe"
    ) else if exist "%USERPROFILE%\.platformio\penv\Scripts\platformio.exe" (
        set "PIO_SCRIPT=%USERPROFILE%\.platformio\penv\Scripts\platformio.exe"
    )
    if "!PIO_SCRIPT!"=="" (
        echo Error: pio/platformio command not found. Please install PlatformIO first.
        echo   pip install platformio
        echo Or check PATH settings.
        exit /b 1
    )
)

echo ========================================
echo Action: %ACTION%
echo Board: %BOARD_NAME%
echo Environment: %ENV_NAME%
echo Project dir: %BASE_DIR%
echo ========================================

if "%ACTION%"=="compile" (
    if defined PIO_SCRIPT (
        "!PIO_SCRIPT!" run -e %ENV_NAME%
    ) else (
        pio run -e %ENV_NAME%
    )
) else if "%ACTION%"=="upload" (
    if defined PIO_SCRIPT (
        "!PIO_SCRIPT!" run -e %ENV_NAME% -t upload
    ) else (
        pio run -e %ENV_NAME% -t upload
    )
) else if "%ACTION%"=="monitor" (
    if defined PIO_SCRIPT (
        "!PIO_SCRIPT!" run -e %ENV_NAME% -t monitor
    ) else (
        pio run -e %ENV_NAME% -t monitor
    )
) else (
    echo Error: Unknown action "%ACTION%"
    echo Supported actions: compile, upload, monitor
    exit /b 1
)

if %errorlevel% equ 0 (
    echo.
    echo ========================================
    echo Operation successful!
    echo ========================================
) else (
    echo.
    echo ========================================
    echo Operation failed with error code: %errorlevel%
    echo ========================================
)

endlocal
