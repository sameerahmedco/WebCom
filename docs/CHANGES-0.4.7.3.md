# WebCom 0.4.7.3 Windows COM-header build hotfix

This hotfix addresses the MSVC 19.51 / Windows SDK 10.0.26100 build failure
reported after the WebView2 SDK restored successfully.

## Root cause

The native WebCom target defined `WIN32_LEAN_AND_MEAN`. With that reduced
Windows header surface, newer MSVC/SDK combinations did not provide COM ABI
symbols (`IUnknown`, `STDMETHOD`, `MIDL_INTERFACE`, `IStream`) before
`WebView2.h` and `gdiplus.h` were parsed. This caused the first errors to appear
inside SDK headers even though the SDK itself was valid.

## Fix

- Native WebCom and Windows UI-test targets no longer define
  `WIN32_LEAN_AND_MEAN`.
- `src/win32_com_compat.h` explicitly includes the COM ABI headers used by
  WebView2/GDI+.
- The compatibility header is included before WebView2/GDI+ in all relevant
  translation units.
- `wrl.h` is included before `WebView2.h`, matching Microsoft's Win32 sample
  include pattern.
- MSVC warnings observed in the submitted build log were cleaned up in the
  omnibox model, launch-request tests, appearance tests, and tab-search control
  ID casts.

The WebView2 version remains pinned to 1.0.4314.35.
