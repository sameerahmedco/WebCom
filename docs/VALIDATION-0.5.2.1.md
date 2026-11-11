# WebCom 0.5.2.1 validation

This hotfix is based on the user's Windows MSVC build log. The WebView2 SDK restore and CMake configuration succeed; compilation fails because `get_CookieManager` was called on base `ICoreWebView2`. In the Win32 WebView2 API that method belongs to `ICoreWebView2_2`.

## Fix

- Both cookie enumeration and per-site cookie clearing now use `TryGetCookieManager`.
- `TryGetCookieManager` queries `ICoreWebView2_2` with `ComPtr::As` before calling `get_CookieManager`.
- No direct base-interface `get_CookieManager` calls remain.
- All 0.5.2 password/autofill and TLS diagnostics are retained.

## Portable validation

- GCC C++20: 10/10 CTest targets passed.
- Clang C++20: 10/10 CTest targets passed.
- No GCC/Clang compiler warnings were found in the build logs.

## Windows acceptance still required

Run `build.cmd` from a fresh extracted 0.5.2.1 directory. This Linux host cannot run the MSVC/WebView2 build, so the Windows build remains the decisive check for this interface-specific hotfix.
