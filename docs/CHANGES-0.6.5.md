# WebCom 0.6.5 changes

Version 0.6.5 is the **Reader defaults, first-class bookmark folders and safe profile setup exchange** milestone.

## Persistent Reader defaults

- Reader text scale, content width and serif/sans choice persist per profile.
- A Reader control change immediately becomes the default for the next Reader tab in that profile.
- Appearance storage advances to v5 and remains backward compatible with v1-v4.
- Older appearance files migrate to text scale 0, comfortable width and sans-serif.
- Reader text itself remains ephemeral and excluded from normal session persistence.

## First-class bookmark folders

- Explicit empty folders are stored separately from bookmark rows.
- Nested empty folders can exist before a bookmark is moved inside them.
- Bookmark Manager exposes **New folder** and safe removal for empty folder trees.
- Folder-tree rename updates both contained bookmark paths and explicit empty descendants.
- Explicit folder names are canonicalized, bounded and unique ignoring case.
- Existing bookmark v1/v2 files and Netscape/Chromium HTML import/export remain compatible.
- Deleting a WebCom profile also removes its explicit bookmark-folder metadata.

## Safe profile setup exchange

- Export/import `.webcom-profile` files containing only user preferences.
- Included: theme/accent, bookmark-bar visibility, reduced motion, high contrast, display label, Reader defaults, password/autofill prompt preferences, and vertical-tab layout.
- Excluded by design: immutable profile key, tabs/URLs, bookmarks, cookies, history, downloads and password values.
- Import uses bounded strict parsing and asks before applying changes to the current profile.
