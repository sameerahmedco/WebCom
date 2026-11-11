# WebCom 0.5.2.1 changes

This hotfix corrects the Win32 WebView2 cookie-manager API usage introduced in 0.5.1 and inherited by 0.5.2.

- `ICoreWebView2::get_CookieManager` was invalid for the pinned SDK.
- WebCom now queries `ICoreWebView2_2` and calls `ICoreWebView2_2::get_CookieManager`, matching the WebView2 Win32 API.
- Both cookie enumeration and per-site cookie clearing use the corrected helper.
- All 0.5.2 credential/autofill/TLS diagnostics remain intact.
