# WebCom MSVC source preflight

WebCom historically encountered several failures that did not appear in the portable GCC/Clang model tests. 0.7.3 turns those failure classes into an automated build gate.

`webcom_source_preflight` is a small C++20 executable with no WebView2 dependency. `Build.ps1` compiles/runs it before the native app. It also runs through CTest on GCC and Clang.

Current rules:
1. Every direct `<WebView2.h>` or `<gdiplus.h>` include must be preceded by `win32_com_compat.h`.
2. Exactly one `get_CookieManager` call is permitted and it must occur through `ComPtr<ICoreWebView2_2> web2`.
3. Invalid `auto` + built-in type-specifier combinations are rejected.
4. One `auto` declaration cannot declare multiple variables with separate initializers.
5. The known `internal_page` native-layout declaration may not be duplicated.
6. `std::min`/`std::max` calls combining RECT geometry with DPI `int` math require an explicit common cast/type.

This preflight complements rather than replaces a real MSVC build. Windows SDK, WebView2 ABI and linker errors still require Windows acceptance.
