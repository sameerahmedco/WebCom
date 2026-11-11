# WebCom 0.6.3 validation

0.6.3 is based on the Windows-compiling WebCom lineage and retains the 0.5.2.1 `ICoreWebView2_2` CookieManager fix plus the later MSVC hardening. Portable geometry/persistence/models have been validated here; the new native selection overlay and drag/drop UI still require a fresh Windows/MSVC acceptance build.

## Completed portable checks

- GCC C++20 configure/build: PASS, zero compiler warnings.
- Clang C++20 configure/build: PASS, zero compiler warnings.
- 14/14 CTest targets: PASS under both toolchains.
- 661 explicit regression checks: PASS.
- New selection-capture coverage validates reverse drag, viewport clipping, minimum-size rejection, invalid viewport handling and logical-to-bitmap scaling.
- Appearance coverage validates display-label round-trip, migration, invalid UTF-8/control rejection and reset/persistence behavior.
- Existing storage, Reader, capture decoder, named sessions, site permissions, History, bookmarks, omnibox, tab search and layout coverage remains green.

## Windows acceptance required

Run `build.cmd` in a fresh extracted 0.6.3 directory, then verify:

1. `WebCom.exe` and the optional UI-test target compile with MSVC and the pinned WebView2 SDK.
2. `Ctrl+Shift+S` → **Select area** → drag a rectangle; verify the PNG crop and dimensions.
3. Cancel selection with Escape and right-click; verify no file or stuck disabled window remains.
4. Drag a tab to another workspace target in horizontal tabs.
5. Repeat tab-to-workspace drag in expanded and collapsed vertical tabs.
6. Confirm cross-profile tab drag still wins when dropping onto another WebCom profile window.
7. Drag a bookmark onto a folder heading and then back onto the root/bookmarks-bar target.
8. Confirm same-folder bookmark reordering still behaves normally.
9. Rename the current profile display label, restart WebCom and confirm the label persists.
10. Confirm cookies/passwords/history remain attached to the original immutable WebView2 profile key after a display-label rename.
11. Reset Appearance and confirm the profile display label is preserved.
12. Delete the renamed profile and confirm the correct immutable profile key/data is deleted.
13. Re-run v0.6.2 full-page capture, Reader, workspace shortcuts and reduced-motion smoke tests.

No claim of production sign-off is made until the Windows/WebView2 checks above pass.
