# WebCom v2 public-beta checklist

- [ ] Fresh MSVC `build.cmd` succeeds after source preflight.
- [ ] `WebCom.exe` and UI test harness link successfully with `oleacc` and `oleaut32`.
- [ ] Narrator announces the custom tab strip as a tab list and reads each visible tab name.
- [ ] Selected/focused states update after mouse, Ctrl+Tab, Ctrl+1…Ctrl+9, and Left/Right navigation.
- [ ] Horizontal and vertical tab bounds/hit-testing are correct for accessibility clients.
- [ ] Closing, reordering, workspace switching, pinned tabs, Incognito tabs, muted/audible tabs, and sleeping tabs do not break the accessibility provider.
- [ ] Stable release manifests reject prerelease versions even if `channel=stable`.
- [ ] Beta release manifests accept a correctly signed prerelease version.
- [ ] `Test-ReleaseMatrix.ps1` passes for the intended Stable/Beta artifacts and common signer pin.
- [ ] Installer ProductName/ProductVersion match the release manifest.
- [ ] Rollback is offered only after repeated failed health windows and only when centralized recovery readiness is `ready`.
- [ ] Wrong running version, missing package, wrong previous version, invalid metadata, or publisher mismatch all block rollback.
- [ ] Recovery installer is re-verified immediately before the UAC launch.
- [ ] One-tab memory QA is sampled after warmup and remains within the release budget chosen for the beta; use 300 MiB private commit as the current target, not an unverified guarantee.
- [ ] Multi-tab, sleeping-tab, split-view, and two-profile memory measurements are recorded for regression comparison.
- [ ] The memory script does not collect command lines, page titles, URLs, credentials, or browsing data.
- [ ] 0.7.9 extension source-health / reload / forget / per-profile isolation acceptance still passes.
- [ ] Update checks/downloads/installs/rollback remain explicit; no unattended install or rollback path was introduced.
