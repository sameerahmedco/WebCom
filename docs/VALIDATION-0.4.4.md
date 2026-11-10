# WebCom 0.4.4 validation

Validated in the current build environment on 2026-11-10.

## Executed checks

- Portable CMake build: passed.
- Seven portable regression suites: passed.
- Total portable checks: **532 passed, 0 failed**.
  - 131 core
  - 97 storage
  - 43 bookmarks
  - 75 launch-request
  - 26 tab-search
  - 125 appearance
  - 35 chrome-layout
- New core coverage accepts the three new internal WebCom URLs.
- New chrome-layout coverage checks tab-reflow easing at start, midpoint, completion, and overrun.
- History continues to be profile-local and excludes private tabs by implementation.
- Native tooltip-popup creation remains disabled.

## Windows build boundary

A new Windows x64 `WebCom.exe` was **not** produced in this environment. The documented Zig 0.13.0 cross-compiler and the pinned Microsoft.Web.WebView2 1.0.4314.35 SDK are not installed locally, and the attempted external compiler download was unavailable. No older executable is relabeled as 0.4.4.

Before a shipping claim, compile the 0.4.4 source with the pinned WebView2 SDK and run `Test-UI.cmd` plus live WebView2 acceptance tests on Windows.

## Windows acceptance still required

- Rapid open/close/switch cycles to inspect the 160 ms reflow and 140/100 ms transitions for dropped frames or stale paint.
- History, Downloads, and Settings scrolling at 100%, 125%, 150%, and 200% DPI.
- Active download progress, completion, interrupted downloads, file opening, and shutdown behavior.
- Multi-profile isolation while History/Downloads/Settings tabs are open in more than one window.
- Menu animation with Windows animations enabled/disabled and Remote Desktop.
- Keyboard focus, high contrast, screen readers, minimum-size windows, mixed monitors, and accessibility naming.
- WebView2 page tools, permissions, media, login flows, renderer recovery, sleeping tabs, and memory measurements.
- Code signing, installer/updater, supported Windows/runtime policy, and final security review.
