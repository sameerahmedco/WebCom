# WebCom 0.4.9 validation

0.4.9 is based on the Windows-compiling 0.4.7.4 lineage. The user confirmed that baseline builds successfully with MSVC on Windows. The 0.4.9 native changes must still be compiled and exercised on Windows before release sign-off.

## Completed portable validation

- GCC C++20: 9/9 suites passed, 578 checks, 0 failures, 0 compiler warnings.
- Clang C++20: 9/9 suites passed, 578 checks, 0 failures, 0 compiler warnings.
- New coverage includes nested bookmark path normalization, folder-tree rename, nested Chromium/Netscape HTML round trips, invalid nested-folder rejection, History filtering, filtered bulk deletion, older-than deletion and inclusive date-range deletion.
- Existing session, recovery, omnibox, tab-search, appearance, layout and launch-request suites remain green.

## Windows acceptance required

Run `build.cmd` from a fresh extracted directory and verify:

1. `WebCom.exe` and the optional UI-test target compile with MSVC.
2. Download progress updates without a 250 ms permanent polling loop on WebView2 1.0.4314.35.
3. Pause/Resume/Cancel/Retry still work after event-driven progress registration.
4. Download completion/interruption toasts appear once and do not block navigation.
5. History bulk buttons delete only the intended rows and remain usable at 100%, 125%, 150% and 200% DPI.
6. Nested bookmark HTML imports from Chrome/Edge preserve parent/child paths and export back correctly.
7. Top-level bookmark-bar folders expose descendant bookmarks with usable labels.
8. Group chips, collapsed group counts and Close group behave correctly during tab reflow.
9. Open all profiles and Close other windows do not close the current profile or lose saved tabs.
10. Repeat the tab-switch/flicker checks and full browser memory measurement.
