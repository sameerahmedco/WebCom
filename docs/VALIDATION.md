# WebCom v2 validation

2.0.0 hardens public-beta release policy, rollback readiness, custom-tab accessibility source paths, and memory regression measurement while retaining the 0.7.9 extension source-health layer.

## Completed portable validation
- GCC C++20 portable matrix: **29/29 CTest targets passed**.
- Clang C++20 portable matrix: **29/29 CTest targets passed**.
- The portable behavioral executables contain **943 explicit checks**, all passing.
- The Windows/MSVC source-preflight gate contains **183 explicit checks**, all passing.
- Release validation therefore covers **1,126 distinct explicit source/behavior checks** per matrix definition (the same checks are not double-counted merely because GCC and Clang both ran them).
- Stable/Beta version-policy tests cover stable final, stable prerelease rejection, beta prerelease acceptance, and final-on-beta allowance.
- Recovery-readiness tests cover ready, not-recommended, running-version mismatch, missing package, version mismatch, publisher mismatch, and stable log-name output.
- Source preflight requires the custom tab accessibility bridge, release-policy gates, rollback readiness policy, package ProductVersion verification, release-matrix QA, and memory regression budget controls.

## Windows acceptance required
1. Fresh-extract WebCom v2 (2.0.0) and run `build.cmd` with current Visual Studio 2022 Build Tools, Windows SDK, and WebView2 SDK restore.
2. Run `Test-UI.cmd` / the native UI harness.
3. Inspect horizontal and vertical tabs with Narrator and a UI Automation/MSAA inspection tool. Verify names, roles, selected/focused state, bounds, hit-testing, Ctrl+1…Ctrl+9 shortcuts, and default selection action.
4. Close/reorder tabs, switch workspaces, toggle vertical-tab collapse, open Incognito, mute/audio, and sleep tabs while an accessibility client is attached; confirm no crash or stale provider state.
5. Build signed Stable/Beta installers/manifests and run `scripts/Test-ReleaseMatrix.ps1` against them before publication.
6. Confirm a Stable manifest containing a prerelease version is rejected by generation/verification/runtime policy.
7. Confirm Beta can accept a correctly signed prerelease and then a final version.
8. Produce a failed update-health transaction, verify rollback is blocked for every non-ready recovery condition, then verify a matching known-good recovery package is re-verified and can be explicitly launched.
9. Run `scripts/Measure-WebComMemory.ps1` with warmup and multiple samples. Record the one-tab private-commit result against the current 300 MiB target and record multi-tab/profile baselines.
10. Re-run all 0.7.9 unpacked-extension acceptance scenarios.

The Linux host cannot compile or execute the native Win32/WebView2/MSAA runtime path. Passing portable GCC/Clang and source-preflight gates is necessary but not sufficient for a public binary release.
