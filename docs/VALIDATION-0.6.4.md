# WebCom 0.6.4 validation

0.6.4 retains the Windows-compiling WebCom lineage and the corrected `ICoreWebView2_2` CookieManager path. Portable appearance/protocol/core models are validated here; Windows High Contrast detection and actual external-application handoff require a fresh MSVC/live run.

## Completed portable validation
- GCC C++20: 15/15 CTest targets passed, zero compiler warnings.
- Clang C++20: 15/15 CTest targets passed, zero compiler warnings.
- 680 explicit checks passed.
- Appearance coverage includes high-contrast palette ratios, v4 persistence and v3 migration.
- External-protocol coverage allows only mailto/tel/sms and rejects javascript/file/custom/malformed/control-character/oversized inputs.
- Core routing coverage includes the new `webcom:shortcuts` page.

## Windows acceptance required
1. Run `build.cmd` from a fresh extracted directory.
2. Toggle High contrast WebCom chrome and confirm browser chrome remains readable at 100/125/150/200% DPI.
3. Enable Windows High Contrast and confirm WebCom follows it even when the per-profile toggle is off.
4. Open Help/Settings -> Keyboard Shortcuts, type into Search shortcuts and verify filtered results/scrolling.
5. Click a `mailto:` test link, reject it, then accept it and confirm only the registered email application is invoked.
6. Repeat with `tel:` or `sms:` if Windows has a registered handler.
7. Confirm `javascript:`, `file:` and a made-up custom scheme are blocked and never passed to Windows.
8. Re-run `Test-UI.cmd` and normal workspace/capture/profile smoke tests.
