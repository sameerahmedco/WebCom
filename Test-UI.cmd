@echo off
setlocal
set "test_binary=%~dp0WebCom-ui-tests.exe"
if not exist "%test_binary%" set "test_binary=%~dp0dist\WebCom\WebCom-ui-tests.exe"
if not exist "%test_binary%" (
    echo WebCom-ui-tests.exe was not found. Use the portable preview or run build.cmd first.
    pause
    exit /b 1
)
echo Running isolated native Windows UI regression checks...
echo Temporary test profile windows will open and close. Your normal WebCom data is not used.
echo These checks do not test live websites or WebView2 rendering.
"%test_binary%"
set "webcom_test_exit=%ERRORLEVEL%"
echo.
if "%webcom_test_exit%"=="0" (echo Native UI checks passed.) else (echo Native UI checks failed. See the output above.)
pause
exit /b %webcom_test_exit%
