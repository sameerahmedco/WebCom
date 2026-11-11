# WebCom 0.6.8 validation

0.6.8 keeps the Windows-compiling WebCom lineage and the corrected `ICoreWebView2_2` CookieManager path. Portable feature/model code was validated here; the native diagnostics/clipboard/file-dialog surfaces still require a fresh Windows MSVC/WebView2 acceptance run.

## Completed portable checks
- GCC C++20: 19/19 CTest targets passed, zero compiler warnings.
- Clang C++20: 19/19 CTest targets passed, zero compiler warnings.
- 744 explicit regression checks passed.
- Reader export tests verify HTML escaping, Reader formatting preferences and Markdown title/source output.
- Update-model tests verify Stable/Beta parsing, semantic-version comparison, strict HTTPS package URLs, SHA-256 validation, unknown-field rejection and persisted channel round-trip.
- Diagnostics-model tests verify runtime/profile/channel/tab-state output without requiring browsing data.

## Windows acceptance required
1. Run `build.cmd` from a fresh extracted 0.6.8 folder.
2. Open Settings > Support diagnostics and confirm WebView2 Runtime/version/profile/workspace counts render.
3. Copy diagnostics and confirm no current page URL, cookie value or password value is present.
4. Change Stable/Beta, restart WebCom and confirm the channel persists.
5. In Reader Mode export TXT, HTML and Markdown; open HTML and print-preview it in a browser to confirm readable formatting.
6. Re-test site cookies, profile deletion, startup modes, F6 focus and screenshot capture from prior milestones.

No production sign-off is claimed until the Windows checks above pass.
