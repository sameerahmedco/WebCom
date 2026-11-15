# WebCom v3 Windows acceptance

`Latest-35` adds a single Windows release gate that exercises the real packaged browser rather than stopping at portable C++ tests.

## Run it

Open **Windows PowerShell** in a freshly extracted WebCom v3 source folder:

```powershell
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File .\scripts\Test-WindowsAcceptance.ps1
```

The default run:

1. removes the target Release build/package folders,
2. runs `build.cmd` with the Windows MSVC/WebView2 toolchain,
3. requires the MSVC source preflight to pass,
4. fails if WebCom project sources emit MSVC `warning C...` diagnostics,
5. verifies packaged `WebCom.exe` FileVersion/ProductVersion are `3.0.0`,
6. verifies required WebView2 loader and bundled Ad Blocker MV3 assets,
7. replays the packaged native Windows UI harness,
8. starts the **real packaged WebCom.exe** against a temporary QA-only profile,
9. opens a local HTML page and requires both a WebCom window and descendant `msedgewebview2.exe` process,
10. closes the QA browser and deletes the temporary profile,
11. writes `windows-acceptance.json` plus the exact build log into `acceptance/`.

The QA launch is intentionally inaccessible during normal browser use. It requires `WEBCOM_QA_MODE=1`, accepts only a direct `%TEMP%\WebCom-Acceptance-*` data root, and uses a separate mutex from ordinary WebCom sessions.

## Add the one-tab memory gate

```powershell
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File .\scripts\Test-WindowsAcceptance.ps1 `
  -MeasureMemory `
  -MemorySamples 10 `
  -MemoryWarmupSeconds 5 `
  -WorkingSetBudgetMiB 300
```

That records the existing `one-tab` memory scenario as CSV + JSON while the isolated local smoke page is running. The 300 MiB working-set value is a **release target**, not a claimed result; only the generated Windows report establishes whether the tested build meets it.

## Useful switches

- `-SkipBuild` — reuse an already-built `dist\WebCom` package.
- `-SkipUiReplay` — skip the second packaged UI-harness replay (the clean build still runs CTest unless separately changed).
- `-SkipRuntimeSmoke` — skip the real packaged WebView2 launch.
- `-AllowCompilerWarnings` — record rather than fail project MSVC warnings. Do not use this for a public release candidate.
- `-OutputDirectory <path>` — choose where acceptance artifacts are written.

ARM64 cross-builds can compile on x64 Windows, but runtime smoke/UI execution still needs a matching ARM64 Windows host.

## Release Candidate aggregation

For final signed-build sign-off, use `scripts/Test-ReleaseCandidate.ps1` rather than treating this source/runtime gate alone as release approval. The Release Candidate workflow adds signed manifest/package verification, installed-browser runtime smoke, uninstall cleanup, and optional previous-to-current upgrade/profile-state compatibility. See `docs/RELEASE-CANDIDATE.md`.
