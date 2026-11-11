# WebCom v2 changes

WebCom v2 is the current public generation. The machine-readable release version is `2.0.0`.

## Small feature list
- Chrome-like native UI.
- Profiles and workspaces.
- Vertical/horizontal tabs.
- History and bookmarks.
- Password manager.
- Cookie/site controls.
- SSL/security UI.
- Unpacked extensions.
- Reader and split view.
- Capture tools.
- Recovery diagnostics.
- Safe updater/rollback.
- Stable/Beta channels.
- Accessibility support.
- Memory regression checks.

## Windows/MSVC build-log fix
- Renamed workspace menu command IDs to `RenameWorkspaceCmd` and `DeleteWorkspaceCmd`.
- Kept the member methods named `RenameWorkspace()` and `DeleteWorkspace()`.
- Updated menu construction and dispatch cases to use the distinct command IDs.
- Added source-preflight checks that reject the old colliding command names.
- Cleaned the warning sites reported in the same Windows build log.

## Latest Windows/MSVC follow-up
- Fixed `std::clamp` ambiguity between Win32 `LONG` coordinates and C++ `int` bounds in the screenshot annotation editor.
- Forced GDI+ `DrawLine` to the integer overload with explicit `INT` coordinates.
- Qualified `webcom::StartupMode::Continue` in the native Windows UI test harness.
- Expanded the source preflight from 176 to 183 checks so these Windows-only compile regressions are caught earlier.
- Added a contributor-friendly `HOW_TO_BUILD.md` and refreshed the GitHub README around WebCom's goal, features, build flow, and beta status.

## Version lineage
- Historical internal builds 0.5.x through 0.8.x are grouped as the WebCom v1 generation.
- Historical filenames are retained for exact build/log traceability.
