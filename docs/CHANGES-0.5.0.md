# WebCom 0.5.0 changes

Version 0.5.0 is the **site controls + browser chrome maturity** milestone built on the Windows-compiling 0.4.7.4 lineage and the 0.4.8/0.4.9 recovery, download and organization work.

## Site information and permissions

- A native shield button now occupies the reserved identity slot on the left side of the omnibox for HTTP(S) pages.
- Per-profile WebCom permission overrides support Camera, Microphone, Location and Notifications.
- Each permission cycles through Ask → Allow → Block from the site-info menu.
- Permission rules are stored in a bounded, validated, atomic per-profile file under the WebCom data root.
- Supported WebView2 permission requests are intercepted and WebCom asks the runtime not to persist a second independent copy of the event decision.
- The WebView2 profile permission API is also updated when a user changes a setting from the shield menu, keeping the engine and WebCom's local rule in sync.
- Incognito tabs do not inherit persistent normal-profile permission overrides; they remain Ask-each-time.
- Clear browsing data also clears WebCom's saved site-permission overrides.

## Recent downloads bubble

- A compact download button appears between the omnibox and profile control when a profile has download history.
- The button disappears first on narrow windows so the address field keeps usable width.
- The menu shows up to five recent downloads with current state/progress and links to the full native Downloads page.
- Completed entries open directly; active/paused/interrupted entries route to the full Downloads manager.
- Active downloads display a small accent activity dot without creating another WebView or background renderer.

## Layout and accessibility

- Toolbar geometry now explicitly reserves the optional download control and is covered by portable layout regression tests.
- Site-info and download controls are native tab-stop buttons with accessible names.
- Existing buffered drawing, tooltip-flash removal, omnibox suggestion crossfades and tab reflow animations remain intact.

## Compatibility

- Session, bookmark, History and appearance file formats are unchanged.
- WebView2 SDK remains pinned to Microsoft.Web.WebView2 1.0.4314.35.
- The 0.4.7.4 MSVC/COM/header compatibility fixes remain in the build lineage.
