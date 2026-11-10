# WebCom 0.4.5 omnibox and bookmark-management update

- Added a pure C++ omnibox suggestion model with local matching/ranking for open tabs,
  bookmarks and recent history, plus Google Search.
- Added a buffered native suggestion panel below the address bar with mouse and
  Up/Down/Enter/Escape keyboard interaction.
- Open-tab suggestions switch to the existing live tab.
- Added a native `webcom:bookmarks` manager and Ctrl+Shift+O shortcut.
- Bookmark Manager opens saved pages and removes individual entries in place.
- Settings links directly to Bookmark Manager and reports the current bookmark count.
- History is grouped by local date headings while preserving profile isolation and
  Incognito exclusion.
- Pinning moves a tab into the left-side pinned set; unpinning moves it immediately
  after the remaining pinned tabs.
- Suggestion UI hides on navigation, tab selection, focus loss and window deactivation.
- Kept Google as default search and retained the approved WebCom application icon.
- Added portable omnibox regression tests and Windows UI test cases for autocomplete
  visibility and the Bookmark Manager shortcut.
