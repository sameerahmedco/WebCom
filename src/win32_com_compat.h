#pragma once

// WebCom defines WIN32_LEAN_AND_MEAN for most portable/native code to keep the
// Windows header surface small. WebView2 and GDI+ both use COM declarations
// such as IUnknown, IStream, STDMETHOD, and MIDL_INTERFACE. Newer MSVC/Windows
// SDK combinations do not make those declarations available through the lean
// windows.h include, so include the COM ABI headers explicitly before either
// WebView2.h or gdiplus.h.
#include <windows.h>
#include <objbase.h>
#include <objidl.h>
#include <unknwn.h>
