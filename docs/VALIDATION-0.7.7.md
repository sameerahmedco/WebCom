# WebCom 0.7.7 validation

0.7.7 adds per-profile Chromium/WebView2 extension management while retaining the existing secure updater, recovery and Windows build-preflight layers.

## Completed portable validation
- GCC C++20: 28/28 CTest targets pass with zero build warnings.
- Clang C++20: 28/28 CTest targets pass with zero build warnings.
- 881 portable regression checks pass.
- MSVC/WebView2 source preflight: 125/125 checks pass.
- 1,006 explicit checks total.
- Extension model tests cover missing folder, missing manifest, valid Manifest V3 preview, name/version parsing, toolbar/popup detection, content-only manifests and missing manifest version.
- Source preflight requires the versioned `ICoreWebView2Profile7` manager APIs, extension enable/remove callbacks, environment-options v6, and the explicit `AreBrowserExtensionsEnabled(TRUE)` setting.
- The existing single `get_CookieManager` access still remains behind `ICoreWebView2_2`.

## Windows acceptance required
1. Fresh-extract 0.7.7 and run `build.cmd`; confirm the MSVC/WebView2 source preflight succeeds before native compilation.
2. Open Extensions from the main menu/Settings and confirm the manager initializes even before any website has been opened.
3. Load a simple Manifest V3 unpacked content-script extension and verify it modifies a controlled test page.
4. Restart WebCom and confirm the extension remains installed/enabled for that profile.
5. Disable it and confirm its behavior stops; enable it and confirm behavior resumes.
6. Remove it and confirm it disappears and no longer executes.
7. Load an extension only in one WebCom profile and confirm another profile does not inherit it.
8. Load a manifest declaring an action/default popup and confirm WebCom shows the toolbar-UI compatibility warning.
9. Try a folder without a valid `manifest.json` and confirm WebView2/WebCom rejects it safely.
10. Delete a profile that has extensions and confirm profile deletion completes without a retained extension-profile COM reference.
11. Force/recover a WebView2 browser-process failure and confirm Extensions can refresh after the environment is recreated.

The actual WebView2 extension engine and Windows COM/UI paths cannot be executed in this Linux validation host; Windows acceptance remains decisive.
