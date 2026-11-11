# WebCom 0.6.5 validation

0.6.5 is based on the Windows-compiling WebCom lineage confirmed by the user. The new portable folder/preferences formats have been tested here; the updated native Bookmark Manager and Windows file-dialog paths still require a fresh `build.cmd` and live acceptance run.

## Completed portable checks

- GCC C++20 configure/build: PASS, zero compiler warnings.
- Clang C++20 configure/build: PASS, zero compiler warnings.
- 16/16 CTest targets: PASS under both toolchains.
- 699 explicit regression checks: PASS.
- Reader-default coverage verifies per-profile round-trip, range validation and v4-to-v5 migration defaults.
- Bookmark-folder coverage verifies empty/nested folder creation, case-insensitive uniqueness, atomic persistence, combined folder listing, tree rename and refusal to delete folders containing bookmarks.
- Profile-template coverage verifies safe preference round-trip, strict numeric parsing, invalid layout rejection and no destination mutation on malformed input.
- Existing session/recovery, privacy, external-protocol, capture, Reader extraction, saved sessions, History, omnibox, tab-search and layout suites remain green.
- Static Windows audit confirms cookie-manager access still goes through `ICoreWebView2_2`; new multi-variable `auto` declarations were split to avoid the earlier MSVC deduction failure pattern.

## Windows acceptance required

Run `build.cmd` in a fresh extracted source tree, then verify:

1. `WebCom.exe` and the optional UI harness compile with MSVC and the pinned WebView2 SDK.
2. Open Reader Mode, change text size/width/font, close it, reopen Reader Mode and confirm the same defaults return; restart WebCom and confirm persistence.
3. Create an empty bookmark folder and nested child, restart, then confirm both remain visible before any bookmark is added.
4. Rename a folder tree containing both bookmarks and empty descendants and confirm all paths move together.
5. Remove an empty folder tree and confirm WebCom refuses removal when any bookmark remains in that subtree.
6. Export the current profile setup, import it into another disposable profile, and confirm only preferences change—no tabs, bookmarks, cookies, history or credentials are imported.
7. Re-run the v0.6.4 high-contrast, shortcuts and safe external-link smoke tests.

No claim of production sign-off is made until the Windows/MSVC checks above pass.
