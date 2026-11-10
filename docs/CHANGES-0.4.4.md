# WebCom 0.4.4 native pages and tab-motion update

- Added first-class native `webcom:history`, `webcom:downloads`, and `webcom:settings` pages.
- Ctrl+H opens full per-profile History; Incognito activity remains excluded.
- Ctrl+J opens Downloads with active byte progress, completed-file opening, an Open Downloads folder action, and clear-completed without cancelling active transfers.
- Main Settings now opens a native Settings page with Appearance, Performance, password prompts, bookmarks-bar, profile-color, and browsing-data actions.
- Internal pages reuse the buffered Win32 renderer; they do not create an extra WebView controller.
- Added scroll and pointer hit testing for long native pages.
- Added 160 ms cubic ease-out tab reflow for open/close geometry changes.
- Existing 100 ms hover and 140 ms selection transitions remain separate.
- Menu opening honors Windows animation preferences and uses an 85 ms native blend when animations are enabled.
- Native tooltip popup creation stays disabled to prevent the reported top-left/yellow flash.
- Google remains the default search engine and the approved square WebCom W remains canonical branding.
