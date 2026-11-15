# How to build and run WebCom v3

This guide is for contributors and testers who want to build **WebCom v3** from source on Windows.

> Build from the current **WebCom-Windows-v3** source folder. Do not reuse an older `WebCom-Windows-v2` or `WebCom-Windows-0.8.0` extraction/build directory; those older trees can reproduce compile errors that are already guarded/fixed in v3.

WebCom is a **C++20 / Win32** application. It uses **Microsoft Edge WebView2** as the web-rendering engine and builds with the Microsoft Visual C++ toolchain.

## 1. Requirements

Use a 64-bit Windows 10 or Windows 11 computer.

Install **Visual Studio 2022 Build Tools** or Visual Studio 2022 with the following components:

- Desktop development with C++
- MSVC x64/x86 build tools
- Windows 10 or Windows 11 SDK
- CMake tools for Windows
- Ninja

WebCom also requires the **Microsoft Edge WebView2 Evergreen Runtime** on the computer where the built browser will run. Modern Windows systems often already have it installed.

The first normal build needs internet access because WebCom restores the pinned **Microsoft.Web.WebView2 1.0.4314.35** SDK from NuGet and verifies its package metadata before using it.

## 2. Get the source

You can either:

- use GitHub's **Code → Download ZIP** option and extract it, or
- clone the repository with Git.

Put the source in a normal writable folder such as:

```text
C:\Users\YourName\source\WebCom
```

Avoid building directly inside a protected Windows folder such as `Program Files`.

## 3. Build WebCom

Open the repository folder and run:

```bat
build.cmd
```

You can double-click `build.cmd`, or run it from Command Prompt/PowerShell.

The wrapper automatically:

1. finds the installed Visual Studio C++ toolchain;
2. imports the correct MSVC developer environment;
3. restores and validates the pinned WebView2 SDK;
4. configures an x64 Release build with CMake + Ninja;
5. builds and runs the MSVC source preflight;
6. compiles WebCom and the test targets;
7. runs CTest unless tests were explicitly skipped;
8. creates a portable browser folder and ZIP under `dist`.

A successful build produces:

```text
dist\WebCom\WebCom.exe
dist\WebCom\WebView2Loader.dll
dist\WebCom-v3-Windows-x64.zip
```

Keep `WebView2Loader.dll` next to `WebCom.exe`.

## 4. Run WebCom

From the repository root, run:

```bat
run.cmd
```

Or launch the executable directly:

```text
dist\WebCom\WebCom.exe
```

## 5. Useful build options

The normal build is recommended:

```bat
build.cmd
```

To build without the optional native Windows UI regression harness:

```bat
build.cmd -SkipUiTests
```

Normal native Windows builds treat WebCom-owned MSVC warnings as errors. For diagnosis only, you can temporarily disable `/WX`:

```bat
build.cmd -AllowCompilerWarnings
```

Do **not** use that switch for Release Candidate or public-release sign-off. The normal Windows acceptance path requires a warning-clean WebCom build.

To skip test execution completely while still compiling the targets:

```bat
build.cmd -SkipTests
```

For a Debug build, call the PowerShell build script directly:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Build.ps1 -Configuration Debug
```

For ARM64 source compilation:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Build.ps1 -Architecture ARM64
```

An ARM64 build produced on an x64 host cannot run its ARM64 tests on that host; validate it on ARM64 Windows before distribution.

## 6. Clean rebuild

If you change compilers, Windows SDK versions, or CMake configuration and want a completely fresh compile, delete the generated folders:

```text
build\
dist\
```

Then run:

```bat
build.cmd
```

You normally do **not** need to delete `.deps`; keeping it avoids restoring the WebView2 SDK again. Delete `.deps` only when you intentionally want to re-download the pinned SDK.

## 7. Build log and troubleshooting

Every `build.cmd` run writes:

```text
build-last.log
```

If the build fails, start with the **first compiler or linker error**, not the last PowerShell error. The PowerShell message at the bottom usually only reports that CMake/Ninja stopped because an earlier compiler step failed.

### Visual Studio tools not found

Open **Visual Studio Installer → Modify** and make sure **Desktop development with C++** and the Windows SDK/CMake components are installed.

### WebView2 SDK download or verification fails

Check that Windows can reach NuGet over HTTPS and that a proxy/firewall is not rewriting or blocking NuGet metadata/package downloads. Then run `build.cmd` again.

### Browser builds but does not open web pages

Install or repair the **Microsoft Edge WebView2 Evergreen Runtime**. `WebView2Loader.dll` is only the loader; the Evergreen Runtime is the actual rendering runtime.

### A C++ compile fails

Attach or paste `build-last.log` when reporting the problem. Include the first error block and the WebCom version/commit you built.

### Only the native UI tests fail

To verify whether the browser itself can compile while the test harness is being investigated, run:

```bat
build.cmd -SkipUiTests
```

Do not use this as a substitute for Windows acceptance testing before a public release.

## 8. Run the native UI regression harness

A normal successful build creates the UI test executable when `-SkipUiTests` is not used. You can run the helper:

```bat
Test-UI.cmd
```

The harness uses isolated temporary data and is designed not to open normal website browsing profiles.

## 9. Continuous integration

The repository includes GitHub Actions under `.github/workflows/windows.yml`:

- **Windows job:** builds WebCom with MSVC, runs portable/native tests, and uploads a packaged Windows artifact.
- **Ubuntu job:** builds and tests the portable non-Windows models.

A green portable job is useful, but it does not replace the Windows/MSVC job because Win32, COM, WebView2, GDI+, resource compilation, and accessibility code have Windows-only compile/runtime behavior.

## 10. Before distributing a build

Do not treat a successful compile as the only release check. At minimum, test:

- normal and private browsing;
- multiple profiles at the same time;
- history/bookmarks/session recovery;
- password/autofill behavior;
- cookies and site permissions;
- connection-security UI;
- downloads and local files;
- workspaces, vertical tabs, split view, reader mode, and capture tools;
- unpacked extensions used by your test matrix;
- Narrator/accessibility behavior;
- update/rollback verification for signed release builds;
- memory usage with the included measurement script.

See `docs/PUBLIC-BETA-CHECKLIST.md`, `docs/VALIDATION.md`, and `docs/MEMORY-ACCEPTANCE.md` for the current release boundary and repeatable memory scenarios.

## 11. Release and installer tooling

Normal developer builds intentionally leave release-update endpoints and the publisher pin blank.

The repository also contains Windows-only scripts for signed installers, release manifests, package verification, update-channel testing, and fresh-install QA. These are intended for release maintainers rather than first-time contributors. Start with `BUILDING.md` after you have a normal local build working.

## Final Windows acceptance gate

For a release-candidate build, do not stop at `build.cmd`. From the fresh v3 source folder run:

```powershell
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File .\scripts\Test-WindowsAcceptance.ps1
```

To test the one-tab 300 MiB working-set target at the same time:

```powershell
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File .\scripts\Test-WindowsAcceptance.ps1 -MeasureMemory -WorkingSetBudgetMiB 300
```

The acceptance runner uses an isolated temporary WebCom profile and leaves normal browser data untouched. Full details are in `docs/WINDOWS-ACCEPTANCE.md`.

## Release Candidate sign-off

Building successfully is not final release approval. After creating the signed installer and matching release manifest, run the Windows Release Candidate gate from the source root:

```powershell
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File .\scripts\Test-ReleaseCandidate.ps1 `
  -CurrentInstaller .\release\WebCom-Setup-3.0.0-x64.exe `
  -StableManifest .\release\stable.v2.txt `
  -ExpectedPublisherSha256 <64-hex-publisher-fingerprint> `
  -PreviousInstaller <previous-signed-installer>
```

If no previous signed WebCom build has ever existed, use `-NoPreviousSignedBuild` instead of silently skipping upgrade QA. See `docs/RELEASE-CANDIDATE.md` for the complete gate and generated reports.
## Package integrity gate

A normal `build.cmd` now verifies the staged `dist\WebCom` package before creating the portable ZIP. For signed releases, `Build-SignedInstaller.ps1` performs the same integrity check before signing and again afterward, producing `signed-dist-verification.json`. Do not move or discard that report before `Test-ReleaseCandidate.ps1`; the RC gate uses it to prove the installer contains the exact signed `WebCom.exe` that passed staged-package verification.

See `docs/BUILD-PACKAGE-INTEGRITY.md`.

