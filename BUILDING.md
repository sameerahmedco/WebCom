# Running and building WebCom v3 for Windows

> New to the project? Start with **[HOW_TO_BUILD.md](HOW_TO_BUILD.md)** for the step-by-step GitHub/contributor guide. This file keeps the lower-level release/build details.


> **Current release:** Stable/Beta policy enforcement, centralized rollback readiness, a custom-tab MSAA accessibility bridge, stricter release-package verification, and memory regression budgets.
> **MSVC follow-up:** v3 also normalizes screenshot-annotation `LONG`/`int` coordinates before `std::clamp`/GDI+ overload selection and qualifies `webcom::StartupMode` in the native Windows UI test harness. The source preflight guards these Windows-only regression classes.

### Windows compile regressions already guarded in v3

- `annotation_editor.cpp`: Win32 `POINT`/`RECT` members are `LONG`; clamps use an explicit `std::clamp<LONG>` and image-space math is normalized to `int`.
- GDI+ annotation lines cast coordinates to `INT` so MSVC does not see both integer and `REAL` `DrawLine` overloads as viable.
- `windows_ui_tests.cpp` includes `storage.h` and uses `webcom::StartupMode::Continue` outside namespace `webcom`.
- Workspace rename/delete commands use `RenameWorkspaceCmd` / `DeleteWorkspaceCmd` so command IDs cannot collide with `BrowserApp::Impl` member-function names.
- `webcom_source_preflight` runs before the browser target and fails early if any of these patterns regress.
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

For v3, Windows acceptance must additionally exercise the custom tab accessibility provider with Narrator/UI Automation inspection, Stable/Beta signed release policy, centralized rollback readiness, and multi-sample memory budgets. Re-run the complete unpacked-extension migration, change-detection, reload, forget, and per-profile isolation matrix as a regression set. See `docs/VALIDATION.md` and `docs/PUBLIC-RELEASE-CHECKLIST.md`.


## Signed installer packaging (Windows only)

After `build.cmd` produces `dist\WebCom`, install Inno Setup 7 (preferred; 6 is also supported) and use a code-signing certificate available to SignTool. Then run `scripts\Build-SignedInstaller.ps1` with the distribution folder, version, certificate SHA-1 selector, pinned publisher certificate SHA-256, and an HTTPS RFC3161 timestamp URL. The script compiles the installer, Authenticode-signs it, verifies the signature, and rejects a publisher fingerprint mismatch.

Use `scripts\Test-FreshInstall.ps1` on the signed installer to perform a silent isolated install/file-layout check/uninstall pass. These Windows-only tools were not executed in the Linux validation environment.


## Release-configured update endpoints

Ordinary `build.cmd` builds leave update endpoints and the publisher pin blank. A distributable update-enabled build must provide the trust roots explicitly, for example through `scripts\Build.ps1 -StableManifestUrl <https-url> -BetaManifestUrl <https-url> -PublisherSha256 <64-hex-cert-fingerprint>`. The browser will not accept an update manifest unless the package publisher in that manifest matches the compiled pin. `scripts\Build-ReleaseCandidate.ps1` wraps the browser build, signing/installer build, signed manifest generation and final package verification.

## Windows release acceptance

After a normal portable validation pass, run the real Windows release gate from a **freshly extracted source tree**:

```powershell
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File .\scripts\Test-WindowsAcceptance.ps1
```

It performs a clean `build.cmd` release build, fails WebCom MSVC project warnings by default, validates packaged version/assets, replays the native UI harness, and smoke-launches the real packaged browser with an isolated `%TEMP%\WebCom-Acceptance-*` data root. See `docs/WINDOWS-ACCEPTANCE.md`.

## Signed Release Candidate gate

Normal `build.cmd` native targets compile WebCom-owned MSVC warnings as errors (`/W4 /WX`). `-AllowCompilerWarnings` is a diagnostic-only bypass and must not be used for release sign-off.

A successful `build.cmd` proves the source build, not the signed release artifact. Before publication, run `scripts/Test-ReleaseCandidate.ps1` against the exact signed installer and matching manifest. The gate combines clean Windows/MSVC/WebView2 acceptance, the one-tab memory target, release-manifest/package verification, installed-browser runtime/uninstall QA, and previous-to-current upgrade/profile-state compatibility when a prior signed build exists. See `docs/RELEASE-CANDIDATE.md`.
## Build-package integrity

Every staged Windows package now runs `scripts\Verify-BuildPackage.ps1` before ZIP creation. The verifier checks PE architecture, version metadata, the bundled blocker payload, compiler/debug-artifact leakage, and exact `docs/source-hashes.sha256` coverage. Release-configured builds also require passed tests, the UI harness, `/WX`, an HTTPS Stable manifest endpoint, and the compiled publisher pin.

`Build-SignedInstaller.ps1` runs that gate before signing and again after signing `WebCom.exe`; the post-sign result is saved as `signed-dist-verification.json`. The Release Candidate gate compares that signed executable SHA-256 to the `WebCom.exe` actually installed by the signed installer. See `docs/BUILD-PACKAGE-INTEGRITY.md`.

The `dist\WebCom-v3-Windows-*.zip` file is a development/test artifact. Public release approval applies to the signed installer, not that portable ZIP.


## Locked dependency verification for signed releases

Latest-45 tracks the WebView2 SDK in `dependencies.lock.json`. Normal developer builds retain the validated fallback/offline SDK path, but `Build-ReleaseCandidate.ps1` requires official NuGet catalog SHA-512 verification and refuses an arbitrary `-SdkDir` for signed/public artifacts. The release output also includes `release-provenance.json` and `webcom.spdx.json`. See `docs/SUPPLY-CHAIN.md`.
PUBLIC RELEASE OPERATIONS (LATEST-47):
Use scripts\Invoke-PublicRelease.ps1 from elevated Windows PowerShell to run build -> mandatory RC -> approval -> evidence in one fail-closed flow. It does not upload anything. After manually publishing the approved installer/Stable manifest, run scripts\Test-PublishedRelease.ps1 and archive its checksummed health report. See docs\POST-RELEASE.md.

