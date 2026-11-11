# WebCom 0.7.7 changes

0.7.7 is the Chromium/WebView2 browser-extension milestone.

## Extension environment
- Browser extensions are explicitly enabled at environment creation through `ICoreWebView2EnvironmentOptions6`.
- CMake fails early if the pinned WebView2 SDK does not contain `WebView2EnvironmentOptions.h`.
- The Windows header smoke target now covers extension environment options as well.

## Native Extensions manager
- Added native `webcom:extensions`, reachable from the main menu, Settings and Quick Commands.
- Extension profile acquisition works whether the page is selected, restored from session, or typed directly in the omnibox.
- If no normal WebView exists yet, WebCom creates one ephemeral unselected `about:blank` bootstrap view solely to acquire the normal profile, then closes it immediately.
- Installed extensions are enumerated through `ICoreWebView2Profile7::GetBrowserExtensions`.
- Rows show extension name, ID and enabled state.
- Enable/Disable uses `ICoreWebView2BrowserExtension::Enable`.
- Remove uses `ICoreWebView2BrowserExtension::Remove`.

## Load unpacked
- Native folder picker selects the top-level unpacked extension directory.
- A bounded 2 MiB preview checks for `manifest.json`, readable name/version/manifest version, and common toolbar/popup declarations before the WebView2 install call.
- Actual validation/install is delegated to `ICoreWebView2Profile7::AddBrowserExtension`.
- The user is told that WebView2 references the extension folder directly and that it must remain available.

## Compatibility boundary
- Direct Chrome Web Store installation is not implemented.
- WebView2 does not expose normal Chrome browser toolbar entry points for extension icons, badges or action popups.
- WebCom warns when a selected manifest declares common action/popup UI.
- Extension installation/state remains per profile and is persisted by WebView2.
- Profile deletion releases extension/profile COM references before WebView2 profile deletion.
