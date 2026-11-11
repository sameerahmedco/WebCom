# WebCom 0.6.7 validation

0.6.7 is based on the Windows-compiling WebCom lineage confirmed by the user. Startup/session-format behavior is portable and tested here; the F6/WebView2 focus path and Windows accessibility event behavior require a fresh Windows `build.cmd` and live acceptance run.

## Completed portable checks
- GCC C++20: 17/17 CTest targets passed, zero warnings.
- Clang C++20: 17/17 CTest targets passed, zero warnings.
- 721 explicit regression checks passed.
- Session v7 round-trip coverage verifies startup mode and startup-page persistence.
- Version 6 migration coverage verifies older profiles default to Continue mode with no configured startup pages.
- Validation rejects unknown startup modes, internal/non-web startup URLs, embedded credentials, duplicate URLs and more than 16 pages.
- Dedicated startup-plan tests cover Continue, New Tab, Specific Pages, empty Specific Pages fallback and recovery override.
- Existing recovery, Reader, bookmark folders, profile templates, capture, permissions, sessions, history, omnibox and layout tests remain green.

## Windows acceptance required
1. Build with `build.cmd` in a fresh extracted directory.
2. Set one profile to Continue, restart and confirm its saved tabs return.
3. Set it to New Tab, restart and confirm only a clean New Tab opens.
4. Configure two specific HTTP(S) startup pages, restart and confirm they open in configured order.
5. Confirm embedded-credential URLs and duplicate startup pages are rejected.
6. Exercise backup recovery while startup mode is New Tab/Specific Pages; choose Keep recovered tabs and confirm the recovered session wins for that run without changing the saved startup preference.
7. Press F6 and Shift+F6 repeatedly and confirm focus cycles between tabs, address bar and page; page focus should enter first/last tabbable WebView content appropriately.
8. With Narrator/NVDA, change tabs and workspaces and verify the tab-strip accessible name announces workspace, selected position/count and title.
9. Re-test v0.6.6 screenshot clipboard, recovery Start fresh and profile desktop shortcut paths.

No claim of production sign-off is made until the Windows/MSVC checks above pass.
