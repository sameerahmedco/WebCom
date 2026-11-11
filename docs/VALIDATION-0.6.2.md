# WebCom 0.6.2 validation

0.6.2 is based on the Windows-compiling WebCom lineage and retains the 0.5.2.1 CookieManager compatibility fix. Portable models and persistence have been validated here; the native WebView2 full-page screenshot path and updated Reader/profile UI still require a fresh Windows/MSVC acceptance build.

## Completed portable checks

- GCC C++20 configure/build: PASS, zero compiler warnings.
- Clang C++20 configure/build: PASS, zero compiler warnings.
- 13/13 CTest targets: PASS under both toolchains.
- 646 explicit regression checks: PASS.
- New capture-model coverage verifies valid base64 screenshot decoding, malformed JSON/base64 rejection and decoded-size limits.
- Appearance v2 coverage verifies reduced-motion roundtrip and v1 migration to full motion.
- Existing session v1-v6 migration, Reader extraction, Named Sessions, profile deletion/recovery, site permissions, History, bookmarks, omnibox, tab search and layout coverage remains green.

## Windows-source audit

- No new mixed-type multi-variable `auto` declarator lists were introduced.
- New `std::min`/`std::max` calls use matching integer types or explicit casts.
- The only direct `get_CookieManager` call remains inside the `ICoreWebView2_2` compatibility helper.
- `favicon_image.h` retains the macro-safe variable names and COM/GDI+ include ordering from the previously fixed Windows build lineage.
- Full-page capture calls the base `ICoreWebView2::CallDevToolsProtocolMethod`; viewport capture retains `ICoreWebView2::CapturePreview`.

## Windows acceptance required

Run `build.cmd` in a fresh extracted 0.6.2 directory, then verify:

1. `WebCom.exe` and `WebCom-ui-tests.exe` compile with MSVC and the pinned WebView2 SDK.
2. Press `Ctrl+Shift+S`, save **Visible area**, and verify the PNG matches the current viewport.
3. Save **Full page** on a long page and verify content below the viewport is included. Also test a very large page and confirm failure leaves no corrupt partial file.
4. Open Reader Mode with F9 and test A−/A+, width cycling, serif/sans, Copy text and Save text.
5. Create at least three workspaces and verify `Ctrl+Alt+Arrow` cycling plus `Ctrl+Alt+1…9` direct switching.
6. Create a temporary profile and choose a color; restart and verify its profile badge/accent remains correct.
7. Turn on **Reduce WebCom motion** from Appearance and verify WebCom tab/hover/omnibox chrome transitions become immediate, then restart and confirm the preference persists.
8. Re-test v0.6.1 Reader privacy, Saved Sessions, Privacy Manager and collapsible vertical tabs.
9. Re-run `Test-UI.cmd` plus normal split-view/download/site-permission smoke tests.

No production sign-off is claimed until the Windows/WebView2 checks above pass.
