# WebCom 0.6.1 validation

0.6.1 is based on WebCom's Windows-compiling lineage and the 0.5.2.1 CookieManager hotfix. Portable feature models and persistence have been tested here; a fresh Windows/MSVC/WebView2 build is still required for native acceptance.

## Completed portable checks

- GCC C++20 configure/build: PASS, zero compiler warnings.
- Clang C++20 configure/build: PASS, zero compiler warnings.
- 12/12 CTest targets: PASS under both toolchains.
- 637 explicit regression checks: PASS.
- New Reader JSON model: 7 checks including escaping, surrogate pairs, malformed input and bounded output.
- New Named Sessions model: 8 checks covering atomic persistence, update/delete behavior and invalid input rejection.
- Storage suite verifies session format v6, collapsed vertical-rail round trip, and migration of v5 vertical tabs to an expanded rail.
- Existing profile deletion, workspaces, site permissions, History, bookmarks, omnibox, tab search, appearance and layout coverage remains green.
- Static Windows-risk scan performed for prior failure patterns: direct base-interface CookieManager calls, mixed-type/multi-variable `auto` declarations, duplicate native-page locals and ambiguous Win32 `LONG`/`int` `std::max` sites.

## Windows acceptance required

Run `build.cmd` from a fresh extracted source tree, then verify:

1. `WebCom.exe` and `webcom_windows_ui_tests.exe` compile with MSVC and WebView2 SDK 1.0.4314.35.
2. Press F9 on an article-like page; confirm readable text appears in a native Reader tab, Open original works, and closing Reader does not add it to Ctrl+Shift+T.
3. Save a named session from a workspace, restart WebCom, reopen it, then overwrite/delete it.
4. Open Privacy Manager after normal browsing; verify cookie domains/counts load, individual domain clearing works, permission-origin reset works, and cookie values are never rendered.
5. Test Clear all site data with disposable logins and confirm passwords/history/downloaded files are not removed by that specific action.
6. Enable vertical tabs, collapse to the 64-DIP rail, restart and confirm the preference persists; expand again and confirm titles/close buttons return.
7. Re-test Workspaces, split view, Quick Commands, viewport capture, cookies/site info, profile deletion and download controls.
8. Run `Test-UI.cmd` and send `build-last.log` if MSVC exposes any Windows-only issue.

No production sign-off is claimed until those Windows/WebView2 checks pass.
