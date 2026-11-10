# Running and building WebCom 0.5.1 for Windows

WebCom is a native C++20 / Win32 project using Microsoft Edge WebView2 for web rendering. The source package includes a build wrapper that restores the pinned WebView2 SDK and builds the browser with MSVC.

## Requirements

Install Visual Studio Build Tools with:

- Desktop development with C++
- Current MSVC x64/x86 build tools
- Windows 10 or Windows 11 SDK
- CMake tools for Windows / Ninja

The Microsoft Edge WebView2 Evergreen Runtime is required to browse web pages at runtime.

## Build

Extract the ZIP to a fresh writable folder, open that folder, and run:

```bat
build.cmd
```

The build script:

1. locates the Visual Studio Build Tools developer environment;
2. restores and validates `Microsoft.Web.WebView2` **1.0.4314.35** from NuGet when it is not already cached;
3. configures CMake for an x64 Release build;
4. builds the browser and native UI regression harness;
5. packages `WebCom.exe`, `WebView2Loader.dll` and the runtime files into `dist\WebCom`.

If only the optional native UI test target is failing, use:

```bat
build.cmd -SkipUiTests
```

The real browser is still built.

## Run

After a successful build:

```bat
run.cmd
```

or open:

```text
dist\WebCom\WebCom.exe
```

Keep `WebView2Loader.dll` beside `WebCom.exe`.

## Build diagnostics

Every build writes `build-last.log` beside `build.cmd`. If MSVC fails, send that file; it contains the exact compiler or linker error.

For v0.5.1, Windows acceptance should specifically exercise the site-info connection state, cookie count/details/clear action, HTTPS/HTTP/certificate-error presentation, and destructive profile deletion in addition to the existing site-permission and download-bubble paths. See `docs/VALIDATION-0.5.1.md`.
