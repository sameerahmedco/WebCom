# WebCom v2 release notes

Current machine-readable release version: `2.0.0`.

## Highlights

- Native Chrome-like Windows browser shell.
- Multiple profiles and workspaces.
- Vertical/horizontal tabs and tab search.
- History, bookmarks, passwords, cookies, and site controls.
- SSL/security details and permission management.
- Unpacked extension management and diagnostics.
- Reader mode, split view, capture, and annotation tools.
- Session recovery and reliability diagnostics.
- Signed update verification and rollback readiness checks.
- Stable/Beta channel policy.
- Accessibility support for the custom tab strip.
- Repeatable memory regression measurement.

## Recent Windows/MSVC hardening

- Separated workspace menu command IDs from member-function names.
- Fixed Win32 `LONG` / C++ `int` ambiguity in screenshot annotation coordinates.
- Forced GDI+ line drawing to an unambiguous integer overload.
- Qualified startup-mode references in the native Windows UI test harness.
- Expanded source-preflight coverage to catch these Windows-only compile regressions before the main browser build.
- Added contributor-focused build documentation and a GitHub-ready project README.
