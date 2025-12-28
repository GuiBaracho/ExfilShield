@echo off
:: ExfilShield Service Installation Script
:: Run this script as Administrator

setlocal EnableDelayedExpansion

echo ============================================
echo   ExfilShield Service Installer
echo ============================================
echo.

:: Check for admin privileges
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [ERROR] This script requires Administrator privileges.
    echo         Right-click and select "Run as administrator"
    pause
    exit /b 1
)

:: Set default paths
set "EXE_PATH=%~dp0..\x64\Release\ExfilShield.exe"
set "CONFIG_DIR=C:\ProgramData\ExfilShield"
set "CONFIG_FILE=%CONFIG_DIR%\config.json"
set "EXAMPLE_CONFIG=%~dp0..\examples\config.example.json"

:: Check if executable exists
if not exist "%EXE_PATH%" (
    echo [ERROR] ExfilShield.exe not found at:
    echo         %EXE_PATH%
    echo.
    echo         Please build the project first using Visual Studio.
    pause
    exit /b 1
)

:: Create config directory
echo [*] Creating configuration directory...
if not exist "%CONFIG_DIR%" (
    mkdir "%CONFIG_DIR%"
    if !errorLevel! neq 0 (
        echo [ERROR] Failed to create configuration directory.
        pause
        exit /b 1
    )
)

:: Copy example config if no config exists
if not exist "%CONFIG_FILE%" (
    echo [*] Copying example configuration...
    if exist "%EXAMPLE_CONFIG%" (
        copy "%EXAMPLE_CONFIG%" "%CONFIG_FILE%" >nul
        echo [OK] Configuration file created at %CONFIG_FILE%
    ) else (
        echo [WARN] Example config not found. Please create config.json manually.
    )
) else (
    echo [OK] Configuration file already exists.
)

:: Stop existing service if running
sc query ExfilShield >nul 2>&1
if %errorLevel% equ 0 (
    echo [*] Stopping existing service...
    sc stop ExfilShield >nul 2>&1
    timeout /t 2 /nobreak >nul

    echo [*] Removing existing service...
    sc delete ExfilShield >nul 2>&1
    timeout /t 2 /nobreak >nul
)

:: Install the service
echo [*] Installing ExfilShield service...
sc create ExfilShield binPath= "%EXE_PATH%" start= auto DisplayName= "ExfilShield Device Monitor"
if %errorLevel% neq 0 (
    echo [ERROR] Failed to create service.
    pause
    exit /b 1
)

:: Set service description
sc description ExfilShield "Monitors and controls external device connections to prevent unauthorized data exfiltration."

:: Start the service
echo [*] Starting service...
sc start ExfilShield
if %errorLevel% neq 0 (
    echo [WARN] Service created but failed to start. Check Event Viewer for details.
) else (
    echo [OK] Service started successfully.
)

echo.
echo ============================================
echo   Installation Complete
echo ============================================
echo.
echo   Service Name:    ExfilShield
echo   Config File:     %CONFIG_FILE%
echo   Log Directory:   %CONFIG_DIR%\Logs\
echo.
echo   Commands:
echo     sc start ExfilShield    - Start service
echo     sc stop ExfilShield     - Stop service
echo     sc query ExfilShield    - Check status
echo.
pause
