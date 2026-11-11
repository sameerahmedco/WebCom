# WebCom 0.7.3 changes

0.7.3 is an MSVC/Windows build-hardening milestone.

## Compiled source preflight
- Added `tools/source_preflight.cpp`.
- Built on all platforms and executed as a CTest target.
- Windows `Build.ps1` builds and runs it immediately after CMake configure and before compiling WebCom.
- The native app and native UI harness also depend on the preflight gate for IDE/direct CMake builds.

The preflight rejects:
- CookieManager access that is not routed through `ICoreWebView2_2`.
- More than one CookieManager call site.
- GDI+ or WebView2 includes that appear before `win32_com_compat.h`.
- Invalid `auto` + built-in type combinations.
- Multi-declarator `auto` statements that can make MSVC deduce conflicting types.
- Reintroduced duplicate `internal_page` declarations.
- Win32 RECT geometry/DPI `std::min`/`std::max` expressions lacking an explicit common type.

## Header smoke compilation
- Added `src/windows_header_smoke.cpp` to the production and Windows UI-test source sets.
- Deliberately compiles `win32_com_compat.h`, WRL, WebView2, GDI+, and the favicon decoder together.
- Keeps SDK/include-order regressions small and local.

## Native geometry hardening
- Normalized several Welcome/Diagnostics/Updates page `std::min` expressions so `RECT` members are cast to `int` before DPI arithmetic.
