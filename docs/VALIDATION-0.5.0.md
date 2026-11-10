# WebCom 0.5.0 validation

0.5.0 is based on the Windows-compiling WebCom lineage confirmed by the user. The new portable site-permission model and toolbar geometry have been tested here; the native WebView2 permission callbacks and download bubble still require a fresh Windows `build.cmd` and live acceptance run.

## Completed portable checks

- GCC C++20 configure/build: PASS, zero compiler warnings.
- Clang C++20 configure/build: PASS, zero compiler warnings.
- 10/10 CTest targets: PASS under both toolchains.
- 597 explicit regression checks: PASS.
- New permission coverage includes origin normalization, Ask/Allow/Block cycling, replacement/reset behavior and atomic persistence.
- New toolbar coverage verifies that the optional downloads control preserves a usable omnibox and hides before the address field is squeezed on narrow layouts.

## Windows acceptance required

Run `build.cmd` in a fresh extracted source tree, then verify:

1. WebCom.exe and WebCom-ui-tests.exe compile with MSVC and the pinned WebView2 SDK.
2. Open a site that requests camera, microphone or geolocation; confirm the shield menu can switch Ask/Allow/Block and that reload applies the change.
3. Confirm Incognito does not reuse persistent normal-profile permission overrides.
4. Start a large download; confirm the toolbar download button appears, shows progress/status, opens completed files and routes active items to Ctrl+J Downloads.
5. Resize below 560 DIPs and confirm the downloads button disappears before the omnibox becomes unusable.
6. Clear browsing data and confirm saved WebCom site-permission overrides return to Ask.
7. Re-run Test-UI.cmd plus normal tab/profile/download smoke tests.

No claim of production sign-off is made until the Windows/WebView2 checks above pass.
