@echo off
setlocal EnableExtensions
set "WEBCOM_LOG=%~dp0build-last.log"
echo WebCom build started %DATE% %TIME% > "%WEBCOM_LOG%"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\Build.ps1" %* >> "%WEBCOM_LOG%" 2>&1
set "WEBCOM_BUILD_EXIT=%ERRORLEVEL%"
type "%WEBCOM_LOG%"
if not "%WEBCOM_BUILD_EXIT%"=="0" (
  echo.
  echo WebCom build failed. Full log: "%WEBCOM_LOG%"
  echo The first compiler error in build-last.log identifies the remaining failure.
)
exit /b %WEBCOM_BUILD_EXIT%
