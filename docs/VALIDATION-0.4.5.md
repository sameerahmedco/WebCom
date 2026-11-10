# WebCom 0.4.5 validation

## Executed in this environment

Portable CMake build with `WEBCOM_BUILD_APP=OFF` and `BUILD_TESTING=ON` completed.
All eight portable suites passed:

- core: 131 checks, 0 failures
- storage: 97 checks, 0 failures
- bookmarks: 43 checks, 0 failures
- launch requests: 75 checks, 0 failures
- tab search: 26 checks, 0 failures
- appearance: 125 checks, 0 failures
- omnibox model: 7 checks, 0 failures
- chrome layout: 35 checks, 0 failures

Total: **539 checks, 0 failures**.

## Windows build boundary

A new Windows x64 `WebCom.exe` was not produced in this environment. The pinned
Windows cross-compiler and Microsoft.Web.WebView2 1.0.4314.35 SDK are not installed
locally. No older executable is relabeled as 0.4.5.

The source includes Windows-native regression cases for the new autocomplete panel
and Ctrl+Shift+O Bookmark Manager flow, but those tests still require compilation and
execution on Windows (or the documented pinned cross-build environment).

## Required acceptance before shipping

1. Build with the pinned WebView2 SDK and run `Test-UI.cmd` on Windows 11 x64.
2. Verify the suggestion panel overlays WebView2 content correctly at 100/150/200% DPI.
3. Exercise rapid typing, Up/Down/Enter/Escape, mouse selection, tab switching and menu
   opening without popup/focus flicker.
4. Verify Bookmark Manager open/remove actions and History date groups in multiple profiles.
5. Repeat profile isolation, Incognito exclusion, download, sleep/unload and session restore tests.
6. Run accessibility/keyboard checks and real-site memory comparisons before a shipping claim.
