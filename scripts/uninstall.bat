@echo off
:: ExfilShield Service Uninstallation Script
:: Run this script as Administrator

echo ============================================
echo   ExfilShield Service Uninstaller
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

:: Check if service exists
sc query ExfilShield >nul 2>&1
if %errorLevel% neq 0 (
    echo [INFO] ExfilShield service is not installed.
    pause
    exit /b 0
)

:: Stop the service
echo [*] Stopping ExfilShield service...
sc stop ExfilShield >nul 2>&1
timeout /t 3 /nobreak >nul

:: Delete the service
echo [*] Removing ExfilShield service...
sc delete ExfilShield
if %errorLevel% neq 0 (
    echo [ERROR] Failed to remove service. It may be in use.
    echo         Try restarting the computer and running this script again.
    pause
    exit /b 1
)

echo.
echo ============================================
echo   Uninstallation Complete
echo ============================================
echo.
echo   The service has been removed.
echo.
echo   Note: Configuration and log files were NOT deleted.
echo   To remove them manually, delete:
echo     C:\ProgramData\ExfilShield\
echo.
pause
