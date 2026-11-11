# WebCom 0.6.2 changes

Version 0.6.2 is the **capture, Reader polish, workspace shortcuts and accessibility** milestone on top of the 0.6.1 reading/sessions/privacy foundation.

## Screenshot capture

- `Ctrl+Shift+S` now opens a native capture picker instead of immediately saving only the viewport.
- **Visible area** retains WebView2 `CapturePreview` and saves the current rendered viewport as PNG.
- **Full page** uses WebView2 `CallDevToolsProtocolMethod` with CDP `Page.captureScreenshot` and `captureBeyondViewport=true`.
- Full-page results are decoded by a bounded portable JSON/base64 model capped at 64 MiB before disk write.
- Unsupported or malformed CDP responses fail closed without leaving a partial output file.

## Reader Mode polish

- Reader Mode adds A− / A+ text sizing from 13 to 25 DIPs.
- Content width cycles between Narrow, Comfortable and Wide.
- Reader text can switch between Segoe UI and Georgia without altering the source page.
- **Copy text** copies only the extracted Reader text to the Windows Unicode clipboard.
- **Save text** exports the current in-memory Reader text as UTF-8 with BOM.
- Reader preferences remain temporary with the Reader tab, matching the existing privacy rule that extracted content is not restored from session storage.
- Reader controls wrap to a second row in narrow browser windows.

## Workspace ergonomics

- `Ctrl+Alt+Left` / `Ctrl+Alt+Up`: previous workspace.
- `Ctrl+Alt+Right` / `Ctrl+Alt+Down`: next workspace.
- `Ctrl+Alt+1…9`: switch directly to the corresponding workspace.
- Previous/Next Workspace actions are also available from Quick Commands.

## Profile creation

- New profiles now immediately offer an optional native color picker after the profile name is accepted.
- Automatic profile color remains the default when the picker is dismissed.
- The chosen color uses the existing Appearance store and profile badge/accent behavior; no new profile renderer or background process is introduced.

## Accessibility

- Appearance storage advances from v1 to v2 with backward-compatible loading.
- New **Reduce WebCom motion** preference disables browser-chrome tab reflow, tab-selection, hover and omnibox suggestion animations.
- Existing v1 appearance files migrate with full motion enabled.
- Reduced motion affects WebCom chrome only; websites retain their own animation/accessibility behavior.

## Compatibility

- Session metadata remains v6 from WebCom 0.6.1.
- The 0.5.2.1 `ICoreWebView2_2` CookieManager fix remains intact.
- Existing Reader, Named Sessions, Privacy Manager, Workspaces, Split View and vertical-tab behavior is preserved.
