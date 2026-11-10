# WebCom 0.4.7 changes

- Added persistent tab-group metadata: name, one of eight colors, and collapsed state.
- Session persistence moves to format v3 while retaining v1/v2 read compatibility.
- Added tab-group collapse/expand, rename and color actions to the tab context menu.
- Collapsed groups render one representative tab; expansion/collapse participates in the native 160 ms reflow animation.
- Added per-entry History deletion; Incognito history exclusion remains unchanged.
- Added Bookmark Manager drag reordering within a folder.
- Moving bookmarks between folders preserves manual order instead of alphabetically re-sorting all bookmarks.
- Fixed nested native action hit-testing so remove/delete controls win over their enclosing row action.
- Expanded the native Profiles management page with profile identity/running state, saved-tab/group counts and open/focus actions.
- Retained screen-coordinate cross-window tab drops so the existing profile-move path can operate across monitors.
- Preserved compact pinned tabs, local omnibox suggestions, Google default search, native internal pages and the approved WebCom application icon.
