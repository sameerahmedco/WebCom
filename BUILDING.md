# Running and building WebCom v2 for Windows

> New to the project? Start with **[HOW_TO_BUILD.md](HOW_TO_BUILD.md)** for the step-by-step GitHub/contributor guide. This file keeps the lower-level release/build details.


> **Current release:** Stable/Beta policy enforcement, centralized rollback readiness, a custom-tab MSAA accessibility bridge, stricter release-package verification, and memory regression budgets.
> **MSVC follow-up:** v2 also normalizes screenshot-annotation `LONG`/`int` coordinates before `std::clamp`/GDI+ overload selection and qualifies `webcom::StartupMode` in the native Windows UI test harness. The source preflight guards these Windows-only regression classes.
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

For v2, Windows acceptance must additionally exercise the custom tab accessibility provider with Narrator/UI Automation inspection, Stable/Beta signed release policy, centralized rollback readiness, and multi-sample memory budgets. Re-run the complete unpacked-extension migration, change-detection, reload, forget, and per-profile isolation matrix as a regression set. See `docs/VALIDATION.md` and `docs/PUBLIC-BETA-CHECKLIST.md`.


## Signed installer packaging (Windows only)

After `build.cmd` produces `dist\WebCom`, install Inno Setup 6 and use a code-signing certificate available to SignTool. Then run `scripts\Build-SignedInstaller.ps1` with the distribution folder, version, certificate SHA-1 selector, pinned publisher certificate SHA-256, and an HTTPS RFC3161 timestamp URL. The script compiles the installer, Authenticode-signs it, verifies the signature, and rejects a publisher fingerprint mismatch.

Use `scripts\Test-FreshInstall.ps1` on the signed installer to perform a silent isolated install/file-layout check/uninstall pass. These Windows-only tools were not executed in the Linux validation environment.


## Release-configured update endpoints

Ordinary `build.cmd` builds leave update endpoints and the publisher pin blank. A distributable update-enabled build must provide the trust roots explicitly, for example through `scripts\Build.ps1 -StableManifestUrl <https-url> -BetaManifestUrl <https-url> -PublisherSha256 <64-hex-cert-fingerprint>`. The browser will not accept an update manifest unless the package publisher in that manifest matches the compiled pin. `scripts\Build-PublicBeta.ps1` wraps the browser build, signing/installer build, v2 manifest generation and final package verification.
