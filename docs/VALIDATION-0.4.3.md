# WebCom 0.4.3 validation

Validated in the current build environment on 2026-11-10.

- Portable CMake build: passed.
- Seven portable regression suites: passed.
- Total portable checks: 525 passed, 0 failed.
- Added deterministic hover interpolation tests at start, midpoint, completion and overrun.
- History remains profile-local and private tabs remain excluded by implementation.
- Native tooltip popup creation remains disabled.
- Windows x64 executable was not rebuilt in this environment because the documented Zig 0.13.0 cross-compiler was not installed and outbound compiler download was unavailable. No old executable is relabeled as 0.4.3.

Windows release validation still requires compiling with the pinned Microsoft.Web.WebView2 1.0.4314.35 SDK and running Test-UI.cmd on Windows.
