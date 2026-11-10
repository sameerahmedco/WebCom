# WebCom 0.4.6 tab organization and library-management update

## Compact pinned tabs

Pinned tabs now occupy a fixed compact region at the left side of the tab strip and
render favicon/audio/private/loading state without a title or close button. Normal tabs
use the remaining width. Reordering continues to preserve the pinned/unpinned boundary.
The selected normal tab is kept visible when the strip is crowded.

## Persistent tab groups

Tabs can be assigned to a named group from the tab context menu, moved between existing
groups, or removed from a group. Group names persist in the WebCom session format v2.
Same-group tabs are kept adjacent when the pinned-tab boundary permits it. Groups use a
stable color marker, and the first normal tab in a contiguous group displays the group
name with the page title.

## Searchable History and Bookmark Manager

The native History and Bookmark Manager pages now expose a native filter field. Filtering
matches page title and URL; Bookmark Manager also matches folder names. History keeps its
Today/Yesterday/date grouping after filtering. Ctrl+F focuses these filters on the native
pages rather than trying to invoke WebView2 Find.

## Bookmark folders and import/export

Bookmarks now use a backward-compatible v2 storage format with an optional folder field.
Root bookmarks remain direct buttons on the bookmarks bar. Each non-empty folder appears
once as a folder button and opens a dark menu containing that folder's bookmarks.
Bookmarks can be moved between folders or back to the root bar from the bookmark context
menu.

Import and export use the Netscape bookmark HTML convention understood by Chromium-family
browsers. Import validates URLs, ignores unsupported/unsafe entries, de-duplicates by URL,
and retains the local 100-bookmark profile cap. Export is written atomically.

## Move tabs between profile windows

A tab can be moved to another open WebCom profile from its context menu. A dragged tab can
also be dropped on another visible WebCom profile window. The destination recreates the
tab in that profile's WebView2 context and preserves URL, title, private-mode state,
pinned state, keep-awake state, and group metadata. The page reloads in the destination
profile; WebView2 navigation/back-forward page state is not transferred. Tabs with active
downloads are intentionally not movable.

## Omnibox interaction polish

Suggestion-row hover states now interpolate over the same short native animation cadence
used by the rest of the browser chrome. No hidden WebView is created for autocomplete or
animation.

## Storage compatibility

Session state v2 adds the optional tab-group field and explicitly permits WebCom native
History, Downloads, Settings and Bookmarks pages to restore. The loader still accepts
state v1. Bookmark storage v2 adds folders and still accepts v1 files.

## Validation boundary

The portable C++ model/tests were built and executed in the development host. The native
Windows application was not rebuilt here because this host lacks Windows headers/SDK and
the pinned Windows/WebView2 build toolchain. Windows-only drag/drop, common-file dialogs,
painting, mixed-DPI behavior and live WebView2 operation remain acceptance items.
