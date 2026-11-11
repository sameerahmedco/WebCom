# WebCom v2 validation

## Completed source validation

- GCC C++20 portable matrix: **29/29 CTest targets passed**.
- Clang C++20 portable matrix: **29/29 CTest targets passed**.
- Portable behavioral executables: **943 explicit checks**, all passing.
- Windows/MSVC source-preflight gate: **183 explicit checks**, all passing.
- Stable/Beta version-policy tests cover stable final, stable prerelease rejection, beta prerelease acceptance, and final-on-beta allowance.
- Recovery-readiness tests cover ready, not-recommended, running-version mismatch, missing package, version mismatch, publisher mismatch, and stable log-name output.

## Windows acceptance required

1. Fresh-extract WebCom v2 and run `build.cmd` with Visual Studio 2022 Build Tools, a current Windows SDK, and WebView2 SDK restore.
2. Run `Test-UI.cmd` / the native UI regression harness.
3. Inspect horizontal and vertical tabs with Narrator and a UI Automation/MSAA inspection tool.
4. Exercise tab close/reorder, workspace switching, vertical-tab collapse, private browsing, audio state, and sleeping tabs while accessibility tooling is attached.
5. Build signed Stable/Beta installers/manifests and run `scripts/Test-ReleaseMatrix.ps1` before publication.
6. Verify Stable rejects prerelease manifests and Beta accepts correctly signed prerelease builds.
7. Exercise failed update-health flows and confirm rollback remains fail-closed until every recovery-readiness condition is satisfied.
8. Run `scripts/Measure-WebComMemory.ps1` with warmup and repeated samples, then record one-tab and multi-tab/profile baselines.
9. Re-test unpacked-extension load, reload, source-health diagnostics, removal, and per-profile isolation.
10. Complete navigation, downloads, permissions, password/autofill, local-file, DPI, and profile-deletion acceptance.

Portable/source validation is necessary but does not replace native Windows/WebView2 runtime acceptance.
