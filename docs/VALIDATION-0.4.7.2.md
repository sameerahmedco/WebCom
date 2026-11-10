# WebCom 0.4.7.2 validation

This hotfix addresses the Windows SDK-restore failure reported for 0.4.7.1.

Validated in the available Linux build environment:

- Portable C++ configuration/build succeeded with GCC.
- All 8 portable CTest suites passed.
- Portable C++ configuration/build succeeded with Clang.
- All 8 portable CTest suites passed under Clang.
- `Restore-WebView2.ps1` no longer requests the invalid flat-container `*.nupkg.sha512` URL.
- The SDK restore validates the downloaded archive, exact NuGet package ID/version and required x64/ARM64 SDK layout; NuGet catalog SHA-512 verification is best-effort when its metadata endpoints are reachable.

Windows/MSVC/WebView2 compilation must still be run on Windows. `build.cmd` writes the complete output to `build-last.log`.
