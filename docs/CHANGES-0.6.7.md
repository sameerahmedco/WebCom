# WebCom 0.6.7 changes

Version 0.6.7 is the **startup behavior and keyboard accessibility** milestone.

## Startup behavior
- New per-profile startup modes: Continue, New Tab, or Specific Pages.
- Specific Pages supports up to 16 complete HTTP(S) URLs in stable order.
- The native startup menu can add the current page, add an entered URL, remove individual pages, or clear the list.
- Embedded URL credentials, malformed/non-web schemes and case-insensitive duplicates are rejected.
- Session format advances to v7; v1-v6 remain readable and migrate to Continue mode.
- Recovery is authoritative for a recovered run: choosing Keep recovered tabs overrides the configured startup mode without changing the saved preference.

## Keyboard and accessibility
- F6 / Shift+F6 cycles focus through tab strip, omnibox and page.
- WebView2 page entry uses `COREWEBVIEW2_MOVE_FOCUS_REASON_NEXT/PREVIOUS`.
- Tab-strip accessible text includes the active workspace, selected tab ordinal/count, title and keyboard hints.
- Windows `EVENT_OBJECT_NAMECHANGE` is emitted when that accessible tab-strip description changes.
- The searchable Keyboard Shortcuts page documents the F6 behavior.
