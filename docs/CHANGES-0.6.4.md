# WebCom 0.6.4 changes

## High-contrast browser chrome
- Adds a per-profile **High contrast WebCom chrome** appearance option.
- Automatically honors Windows High Contrast while it is active.
- Uses strong black/white/yellow chrome colors with enhanced text/focus contrast.
- Does not inject CSS or alter website content.
- Appearance persistence advances to v4; v1-v3 migrate to normal contrast.

## Keyboard Shortcuts page
- Adds `webcom:shortcuts`, a native searchable shortcut reference.
- Covers tabs, workspaces, Reader Mode, capture, History, Downloads, bookmarks and performance.
- Linked from Settings and Help.

## Safe external protocols
- Allows only `mailto:`, `tel:` and `sms:` links initiated by web content.
- Validates the URI with a portable allow-list before any OS handoff.
- Cancels WebView2 navigation first and asks for explicit user confirmation.
- Blocks custom schemes, `file:`, `javascript:` and malformed/control-character payloads.
- The omnibox remains HTTP(S)-only.
