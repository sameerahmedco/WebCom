# WebCom 0.7.0 validation

Portable validation covers update consent migration, signed-manifest rules, rollback-safe update transaction transitions and all prior portable browser models.

## Completed here
- GCC C++20: 20/20 test suites passed.
- Clang C++20: 20/20 test suites passed.
- 783 explicit checks, zero failures.
- Zero GCC/Clang compiler warnings.
- Existing `ICoreWebView2_2` CookieManager compatibility fix preserved.

## Windows acceptance required
- Native Updates page compile/render/actions under MSVC.
- Inno Setup installer compilation.
- Authenticode signing/timestamping/verification.
- Silent fresh install and uninstall QA.
- Windows UI tests and normal browser smoke tests.

Automatic in-browser download/install is not enabled by this release.
