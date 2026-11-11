# WebCom 0.6.6 changes

## Screenshot clipboard
- Capture menu adds **Copy visible area** and **Copy selected area**.
- WebView2 CapturePreview still supplies the pixels.
- GDI+ converts the PNG stream to a bitmap; WebCom transfers the HBITMAP to the Windows clipboard only after SetClipboardData succeeds.
- Existing Save visible / Save selected / Save full page actions remain.

## Explicit recovery choice
- When `state.v1` is unreadable but `state.v1.bak` is valid, startup asks whether to keep the recovered tabs or start fresh.
- Start fresh resets only tabs/groups. Profiles, workspaces, bookmarks, browser data and preferences remain.
- Both primary and backup session generations are rewritten so discarded recovered tabs cannot return after another corrupt primary.

## Profile desktop shortcuts
- Profile menu adds **Create desktop shortcut**.
- The shortcut invokes the current WebCom executable with `--profile "<profile key>"`.
- The visible display label is used only for the `.lnk` filename/description; the immutable browser-data key stays authoritative.
- Existing shortcut files require confirmation before replacement.
