# WebCom 0.4.6 validation

## Completed on this development host

The portable CMake build completed successfully after the 0.4.6 storage, bookmark,
omnibox and tab-layout changes.

`ctest --output-on-failure` result:

- 8 / 8 suites passed
- 545 checks total
- 0 failures

Per-suite results:

| Suite | Checks |
| --- | ---: |
| core | 131 |
| storage | 96 |
| bookmarks | 47 |
| launch requests | 75 |
| tab search | 26 |
| appearance | 125 |
| omnibox | 7 |
| chrome layout | 38 |

The 0.4.6 tests specifically cover session-v2 group round trips, legacy session reading,
native-page restore, bookmark-v2 folders, bookmark HTML export/import, and compact pinned
slot planning in addition to the existing regression coverage.

## Windows build boundary

A new 0.4.6 `WebCom.exe` was **not** produced on this host. `clang-cl` is present, but the
Windows SDK headers (including `windows.h`) and the pinned Windows/WebView2 build toolchain
are not available. An older executable must not be relabeled as 0.4.6.

## Required Windows acceptance before shipping

Run the native UI harness and live browser on the exact shipping Windows 11 x64 build and
verify at minimum:

- compact pinned tabs at 100%, 150% and 200% DPI, including audio/loading/private states;
- group creation/removal, persisted group names/colors and grouped tab ordering;
- History and Bookmark Manager keyboard filtering, scrolling and focus behavior;
- bookmark folder buttons, folder menus, move-to-folder/root operations, import/export
  dialogs and interoperability with a Chromium-exported bookmarks HTML file;
- drag a normal and private tab between two simultaneous profile windows and verify profile
  cookie/storage isolation; verify active downloads block the move;
- omnibox suggestion hover/keyboard animation without top-left tooltip artifacts;
- tab close/open/reflow smoothness under rapid input and narrow-window tab overflow;
- accessibility/screen-reader labels and full keyboard navigation;
- live WebView2 navigation, downloads, print/find/devtools and engine recovery;
- full WebCom process-tree memory under identical workloads versus the comparison browser.

Production signing, installer/update delivery and default-browser registration remain
separate release-engineering gates.
