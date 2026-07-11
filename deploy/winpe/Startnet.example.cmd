@echo off
setlocal
wpeinit

rem Customize these three paths before adding this file to boot.wim as Startnet.cmd.
set "WIMFORGE_SERVER=https://provisioning.example.test/"
set "WIMFORGE_SETUP=\\deployment.example.test\WindowsMedia\setup.exe"
set "WIMFORGE_TOKEN=X:\WimForge\provisioning-token.txt"

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "X:\WimForge\Invoke-WimForgeProvisioning.ps1" -Server "%WIMFORGE_SERVER%" -SetupPath "%WIMFORGE_SETUP%" -TokenFile "%WIMFORGE_TOKEN%"
if errorlevel 1 (
    echo WimForge provisioning failed. Windows Setup was not started.
    echo Review X:\WimForge\Provisioning.log or the configured persistent log path.
    exit /b 1
)
endlocal
