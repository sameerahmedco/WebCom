@echo off
setlocal
if exist "%~dp0WebCom.exe" (
    start "" "%~dp0WebCom.exe" %*
    exit /b 0
)
if exist "%~dp0dist\WebCom\WebCom.exe" (
    start "" "%~dp0dist\WebCom\WebCom.exe" %*
    exit /b 0
)
echo WebCom.exe has not been built here yet.
echo Run build.cmd first. See BUILDING.md for the required Windows C++ tools.
exit /b 1
