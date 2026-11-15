# WebCom v3 validation

## Completed source validation

- GCC C++20 portable matrix: **40/40 CTest targets passed**.
- Clang C++20 portable matrix: **40/40 CTest targets passed**.
- Portable behavioral executables: **1151 explicit checks**, all passing.
- Bundled ad-blocker / YouTube script tests: **8/8 passed**.
- Memory-snapshot model tests cover host/WebView2 split totals, tab/sleep state, sleep-policy metadata, Never-sleep counts, partial runtime-process-list warnings, and incomplete-process warnings.
- Windows memory QA tooling now carries explicit scenario labels and optional versioned JSON output with average/median/p95/peak summaries while avoiding process command lines and browsing URLs.
- Windows/MSVC source-preflight gate: **591 explicit checks**, all passing.
- Stable/Beta version-policy tests cover stable final, stable prerelease rejection, beta prerelease acceptance, and final-on-beta allowance.
- Recovery-readiness tests cover ready, not-recommended, running-version mismatch, missing package, version mismatch, publisher mismatch, and stable log-name output.
- New Tab model tests cover host normalization, non-web rejection, recent-host deduplication, newest-entry selection, and bounded-card behavior.
- Home-preferences tests cover safe shortcut add/remove, duplicate rejection, unsafe-scheme rejection, case-insensitive hidden-host behavior, and save/load round trips.
- Ad Blocker preference tests cover DNS-host normalization/validation, URL host extraction, duplicate rejection, the 64-host bound, profile-name rejection, atomic save/load, shipped DNR-template rendering, reserved-sentinel fallback, and generated site-config output.
- Bundled blocker script tests cover player-response filtering, endpoint scoping, skip-button behavior, paused-site bypass, site-config ordering, DNR exclusion slots, and cosmetic-filter gating.

- Performance preference tests cover all five timeout modes, exact-host normalization/validation, duplicate suppression, the 64-host bound, timeout/exception preservation, atomic persistence, and invalid profile-name rejection.
- Latest-35 Windows acceptance gate clean-builds the real browser package, rejects WebCom MSVC project warnings by default, verifies version/assets, replays the packaged native UI harness, and requires a real packaged WebView2 smoke launch against an isolated QA profile.
- Latest-36 performance hardening adds portable coverage for adaptive automatic-sleep polling and source-preflight guards that keep Downloads/Site Permissions/Saved Sessions/unpacked-extension source scans off the ordinary constructor path while enforcing load-before-mutate safety.
- Latest-36 also guards the unpacked-extension picker against the duplicate `PWSTR raw` declaration that would fail the real MSVC browser compile.
- Latest-37 adds portable chrome-layout checks for changed-tab/reduced-motion selection timing and reflow gating.
- Latest-37 MSVC source-preflight guards require destination-first WebView visibility handoff, hidden startup tab/address controls, buffered final-bounds omnibox reveal, and reduced-motion-aware native dark-menu animation.
- These source/portable checks reduce known flicker paths but do **not** replace real Windows visual acceptance at multiple DPI/scaling settings.
- Latest-38 adds 42 portable Recently Closed persistence checks covering safe URL acceptance/rejection, profile validation, bounded history, pinned/keep-awake preservation, round-trip persistence, removal, and corrupt-header fallback.
- Latest-38 source-preflight guards persistent recovery wiring, restore state, actionable download/update notices, the Report a Problem privacy boundary, Ad Blocker state display, and post-success WebCom-owned browsing-data cleanup.
- Latest-40 adds 10 portable accessibility-policy checks covering profile/system reduced motion, Remote Desktop motion suppression, profile/Windows High Contrast, and composed policies.
- Latest-40 source-preflight requires effective reduced-motion gating across tab/reflow/omnibox/menu paths, live `WM_SETTINGCHANGE`/theme refresh, DPI clamping/buffer rebuild, stale-popup dismissal, and keyboard focus/activation for painted native-page actions.
- Latest-41 adds 13 portable native-accessibility model checks for virtual child-ID boundaries and forward/backward action navigation.
- Latest-41 source-preflight requires the painted-action `IAccessible` wrapper, standard-client wrapping through `CreateStdAccessibleObject`, push-button roles/names/bounds/hit testing, focus events, tested child-ID mapping/navigation, and stale-action suppression on web/loading/private-New-Tab states.
- Latest-42 adds log-driven negative guards for the old MSVC `LONG`/`int` `std::clamp` ambiguity, raw-`POINT` GDI+ `DrawLine` overload ambiguity, and brittle Windows-UI `SavedProfile` startup aggregate. The UI harness now seeds profiles through explicit field assignment while preserving namespace-qualified `webcom::StartupMode::Continue`.
- Latest-43 turns WebCom-owned native MSVC warnings into build failures by default (`/W4 /WX`), suppresses only external angle-bracket header diagnostics from that project policy, and retains a clearly labeled diagnostic-only override. Windows acceptance still parses repository warning lines after the build so the release gate remains defense-in-depth.
- Latest-44 source-preflight guards the staged-package verifier, PE architecture checks, source-manifest exactness, release-ready `/WX`/update/publisher metadata, pre/post-sign verification, signed executable identity, and installer-vs-staged executable SHA-256 binding.
- The acceptance runner can optionally collect the one-tab memory scenario and writes `windows-acceptance.json`; portable Linux validation cannot execute that Windows/WebView2 gate.

## Windows acceptance required

1. Fresh-extract WebCom v3 and run `build.cmd` with Visual Studio 2022 Build Tools, a current Windows SDK, and WebView2 SDK restore.
2. Run `Test-UI.cmd` / the native UI regression harness.
3. Inspect horizontal and vertical tabs with Narrator and a UI Automation/MSAA inspection tool.
   - Repeat at 100%, 125%, 150%, and 200% display scaling.
   - Toggle Windows High Contrast while WebCom is open and confirm browser chrome updates without restart.
   - Disable Windows client animations and confirm tab/reflow/omnibox/menu motion stops even when the profile Reduce Motion toggle is off.
   - Use F6 to enter native internal-page actions; navigate by keyboard and confirm visible focus plus Enter/Space activation.
   - With Narrator and an MSAA/UI Automation inspection tool, verify visible painted actions appear as named push buttons with correct screen bounds, hit testing, focused state, and default activation while native edit/button children remain discoverable.
4. Exercise tab close/reorder, workspace switching, vertical-tab collapse, private browsing, audio state, and sleeping tabs while accessibility tooling is attached.
5. Build signed Stable/Beta installers/manifests and run `scripts/Test-ReleaseMatrix.ps1` before publication.
6. Verify Stable rejects prerelease manifests and Beta accepts correctly signed prerelease builds.
7. Exercise failed update-health flows and confirm rollback remains fail-closed until every recovery-readiness condition is satisfied.
8. Run `scripts/Measure-WebComMemory.ps1` with named scenarios, warmup and repeated samples; retain CSV/JSON artifacts for one-tab, multi-tab, sleeping-tabs, split-view and multi-profile acceptance.
9. Re-test unpacked-extension load, reload, source-health diagnostics, removal, and per-profile isolation.
10. Complete navigation, downloads, permissions, password/autofill, local-file, DPI, and profile-deletion acceptance.

Portable/source validation is necessary but does not replace native Windows/WebView2 runtime acceptance.

## Windows/MSVC regression classes

The source preflight explicitly guards the compile failures previously seen in Windows build logs: Win32 `LONG`/`int` `std::clamp` ambiguity, GDI+ `DrawLine` overload ambiguity, unqualified `StartupMode::Continue` in the native UI test harness, and workspace command-ID/member-function collisions. These checks run before the main browser target.

- History host tests cover `www.` normalization, host aggregation, visit-count ordering, case-insensitive whole-site cleanup, and preservation of unrelated sites.

- Tab-batch model tests cover safe duplicate selection, workspace scoping, private/normal separation, pinned/download protection, and missing-target fail-closed behavior.

- Performance preference tests cover all five timeout choices, missing-file fallback, atomic round trips, path traversal rejection, and Windows device-name rejection. Core tests cover each delay plus Never fail-closed behavior.

- Download-history model tests cover 30-day retention boundaries, fail-safe future timestamps, preservation of old failed records, bounded retry candidate selection, paused exclusion, and hard retry caps.

- Extension view model tests cover source-health attention classification, built-in-extension handling, All/Enabled/Disabled/Needs-attention filters, case-insensitive search, and live counts.
- Onboarding model tests cover empty, partial, and complete four-step setup progress.

## Latest-39 Release Candidate validation tooling

- `Test-FreshInstall.ps1` now verifies signed installed version metadata, Default Apps registration, isolated installed-browser/WebView2 startup, silent uninstall, and registration cleanup; it emits `webcom-fresh-install-v2`.
- `Test-UpgradeInstall.ps1` requires distinct signed previous/current installers, smoke-launches the previous build, records isolated persisted `state.v1`, verifies the current installer does not mutate that profile before launch, smoke-launches the upgraded build against the same profile, and verifies uninstall registration cleanup; it emits `webcom-upgrade-install-v1`.
- `Test-ReleaseMatrix.ps1` emits `webcom-release-matrix-v2` so signed channel/package verification is archived with the candidate.
- `Test-ReleaseCandidate.ps1` aggregates clean Windows acceptance, the default 300 MiB one-tab memory target, release-matrix verification, fresh-install runtime/uninstall QA, and optional upgrade compatibility into `webcom-release-candidate-v1` with hashes of every component report.
- Portable source-preflight verifies that these gates and the explicit "do not publish until RC passes" policy remain wired, but only a real Windows run can satisfy the Release Candidate gate.
