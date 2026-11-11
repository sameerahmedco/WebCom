# WebCom 0.6.0 validation

0.6.0 is based on the Windows/MSVC-compatible WebCom lineage, including the 0.4.7.4 compiler fixes and the 0.5.2.1 `ICoreWebView2_2` cookie-manager hotfix.

## Portable validation completed

The exact release tree was built and tested with two independent C++20 toolchains on Linux:

- GCC C++20: 10/10 CTest targets passed.
- Clang C++20: 10/10 CTest targets passed.
- 621 explicit model/layout checks passed in each toolchain run.
- No compiler warnings were found in the GCC or Clang build logs.

Coverage includes session format v5 migration/validation, Workspaces, per-tab workspace persistence, existing storage/bookmark/history/permission models, and vertical-tab layout geometry including selected-tab visibility during overflow.

## Windows/MSVC source review completed

The new native paths were checked for the MSVC issues encountered in earlier releases: mixed-type `auto` declarator lists, ambiguous Win32 `LONG`/`int` math, Windows macro-sensitive identifiers, stale `std::set::contains` usage, and direct base-interface CookieManager access. The current cookie-manager helper continues to query `ICoreWebView2_2` first.

`ICoreWebView2::CapturePreview` is used for the new visible-page PNG capture; the feature is intentionally not described as full-page capture.

## Windows acceptance still required

This environment cannot execute the real MSVC + Windows SDK + WebView2 build. Before public release, build 0.6.0 from a fresh extracted directory with `build.cmd` and exercise:

1. horizontal -> vertical -> horizontal tab layout, including overflow and groups;
2. create/rename/color/delete Workspaces and restart to verify persistence;
3. move tabs between Workspaces and jump across them via tab search;
4. open/close split view, swap/select either visible side and switch Workspaces while split;
5. Quick Commands (`Ctrl+Shift+P`) and its keyboard navigation;
6. visible-page PNG capture (`Ctrl+Shift+S`);
7. existing cookie/site-info, profile deletion, password/autofill, download and session-recovery paths;
8. mixed-DPI and multi-monitor behavior plus a one-tab memory measurement.

If MSVC fails, `build-last.log` is the authoritative diagnostic.
