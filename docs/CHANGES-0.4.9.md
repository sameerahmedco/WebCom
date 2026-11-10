# WebCom 0.4.9 changes

Version 0.4.9 is the **organization, profile-control and event-driven downloads update** built on the Windows-compiling 0.4.7.4 lineage and the 0.4.8 recovery/download-control work.

## History

- Adds a portable history model shared by the native History page.
- Search remains local and case-insensitive.
- Adds **Delete matches** for the current History filter.
- Adds **Delete last 7 days** and **Delete older than 30 days** bulk actions.
- Individual History deletion and Clear history remain available.
- Incognito activity remains excluded from persistent History.

## Bookmarks

- Folder strings can now be treated as nested paths such as `Work/Research/2027`.
- Nested paths are normalized for new folder moves and renames while existing one-level bookmark metadata remains compatible.
- Renaming a folder updates its descendant folder tree.
- Chromium/Netscape HTML import/export now preserves nested folder hierarchy.
- The bookmarks bar shows top-level folders; opening one includes bookmarks from descendant folders with their relative folder path.
- Bookmark context menus can rename the containing folder and accept `/` in new folder paths.

## Tab groups

- Leading grouped tabs now render a colored group chip instead of embedding the group name directly into the page title.
- Collapsed groups show their count beside the group chip.
- Adds **Close group** to the tab context menu.
- Existing collapse/expand, rename, color and persistence behavior remains.

## Profiles

- The native Profiles page adds **Open all profiles** and **Close other windows** actions.
- Existing profile windows keep their independent WebView2 profile storage and can continue to run simultaneously.

## Downloads

- Live download progress now uses WebView2 `BytesReceivedChanged` and `StateChanged` events when available.
- A 1-second polling timer remains only as a fallback when either event cannot be registered.
- Download state is persisted on state changes and periodically during byte progress.
- Completion and unexpected interruption produce non-blocking WebCom notifications.
- Existing Pause/Resume/Cancel/Retry controls remain.

## Validation

Portable validation adds a dedicated history-model suite and nested bookmark-folder regression coverage. Windows/MSVC acceptance is still required for the native WebView2 event wiring and updated native UI.
