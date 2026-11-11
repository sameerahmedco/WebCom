# WebCom 0.6.0 changes

## Productivity and organization

- Added persistent per-profile Workspaces with up to 32 named, color-coded spaces.
- Added per-tab workspace membership and active-workspace persistence in session format v5.
- Added a native vertical-tab layout that keeps the active workspace's selected tab visible.
- Added two-tab split view using existing WebView2 controllers rather than hidden browser instances.
- Added a searchable Quick Commands palette on `Ctrl+Shift+P`.
- Added visible-WebView PNG capture on `Ctrl+Shift+S` using `ICoreWebView2::CapturePreview`.

## Interaction details

- Tab search can jump to tabs in other workspaces and switches the workspace automatically.
- Numeric tab shortcuts and Ctrl+Tab/Ctrl+Shift+Tab operate on the current workspace.
- Context menus can move tabs between workspaces and open a second website tab in split view.
- Settings exposes Workspaces and the horizontal/vertical tab-layout preference.
- Profile cards report saved workspace counts.
- Native home/private/internal pages are laid out inside the reduced content area when vertical tabs are enabled.

## Persistence

- Session metadata is now version 5.
- State v1-v4 remains readable and migrates into a `Main` workspace with horizontal tabs.
- Workspace names, colors, current workspace, per-tab membership and vertical-tab preference are validated before restore.
- Cookies, passwords, form data and private tabs remain outside the session file.

## Stability notes

- Preserves the Windows/MSVC fixes from the 0.4.7.4 line and the corrected `ICoreWebView2_2` cookie-manager access from 0.5.2.1.
- Portable tests cover v5 state migration/validation plus vertical-tab geometry; Windows-native WebView2 paths still require MSVC/live acceptance on Windows.
