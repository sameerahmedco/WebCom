# WebCom 0.6.6 validation

0.6.6 is based on the Windows-compiling WebCom lineage confirmed by the user. Recovery-reset state logic is portable/tested here; the clipboard and Windows ShellLink paths require a fresh Windows `build.cmd` and live acceptance run.

## Completed portable checks
- GCC C++20: 16/16 CTest targets passed, zero warnings.
- Clang C++20: 16/16 CTest targets passed, zero warnings.
- 704 explicit regression checks passed.
- Recovery reset tests verify tabs/groups are discarded while workspaces and profile preferences remain.
- Recovery reset persistence tests verify `state.v1.bak` cannot resurrect discarded tabs.
- Existing Reader, bookmark-folder, profile-template, capture, permissions, sessions, history, omnibox and layout tests remain green.
- Static Windows audit confirms CookieManager still goes through `ICoreWebView2_2`; the new native code does not introduce mixed-type multi-variable `auto` declarations.

## Windows acceptance required
1. Build with `build.cmd` in a fresh extracted directory.
2. Capture > Copy visible area and paste into Paint or another image editor.
3. Capture > Copy selected area, drag a region, and verify only that crop is pasted.
4. Simulate/trigger session backup recovery and test both Keep recovered tabs and Start fresh. Confirm Start fresh preserves profile/browser data and that a second recovery cannot restore discarded tabs.
5. Create a desktop shortcut for a profile, rename the visible profile label, then confirm the shortcut still opens the correct immutable profile key.
6. Re-test v0.6.5 Reader defaults, empty bookmark folders and profile setup import/export.

No claim of production sign-off is made until the Windows/MSVC checks above pass.
