# WebCom 0.4.7.4 validation

This hotfix was derived from the supplied MSVC 19.51 build log. The log confirms WebView2 restore succeeds and compilation reaches WebCom-owned Windows code.

Fixes applied for the reported MSVC errors:

- favicon decoder local renamed from `small` to avoid the Windows/RPC `small` macro collision;
- duplicate `internal_page` declaration removed;
- Win32 `LONG` rectangle deltas are explicitly converted to `int` before `std::max`;
- history/download temp paths no longer use one `auto` declaration for different deduced types;
- remaining NameDialog and explicit-black appearance-test warnings were cleaned up.

Portable validation performed on the corrected tree:

- GCC C++20: 8/8 CTest suites passed; zero build warnings.
- Clang C++20: 8/8 CTest suites passed; zero build warnings.

The Windows executable still needs to be compiled on Windows with MSVC and WebView2. `build-last.log` remains the source of truth for any subsequent Windows-only error.
