# WebCom 0.6.3 changes

Version 0.6.3 is the **direct manipulation + profile lifecycle** milestone on top of the 0.6.2 capture/Reader/accessibility foundation.

## Selection capture

- `Ctrl+Shift+S` now offers **Select area** in addition to Visible area and Full page.
- Selection uses a temporary layered native Win32 overlay positioned over the active WebView controller.
- Escape or right-click cancels without saving.
- The selected logical rectangle is clamped and scaled against the actual `CapturePreview` bitmap dimensions by a separately tested portable geometry model.
- Cropping is performed with GDI+ and partial files are removed on failure.
- No extra WebView or persistent renderer is created.

## Direct workspace drag targets

- While dragging a tab, WebCom paints live drop targets for every other workspace.
- Targets work in horizontal tabs, expanded vertical tabs and the collapsed vertical-tab rail.
- Dropping directly changes the tab's workspace through the existing persistent workspace model.
- Cross-profile drag remains higher priority when the pointer is over another WebCom profile window.

## Bookmark drag-to-folder

- Bookmark Manager folder headings become live drop targets during a bookmark drag.
- A dedicated root target moves a bookmark back to the bookmarks bar.
- Same-folder drag reordering remains intact when no folder target is active.
- Nested folder paths use the existing validated bookmark-folder model.

## Safe profile display-name rename

- Users can rename the visible profile label without changing `SavedProfile::name`, the immutable WebView2 profile key, or its browser-data directory.
- Display labels are stored in per-profile Appearance metadata format v3.
- Appearance v1/v2 files remain readable; an empty display label falls back to the immutable profile key.
- Reset appearance deliberately preserves the profile display label.
- Profile deletion continues to target the immutable internal key, preventing label changes from orphaning WebView2 data.
