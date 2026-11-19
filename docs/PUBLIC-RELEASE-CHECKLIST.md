# WebCom v3 public-release acceptance checklist


## Latest-50 background update acceptance

- [ ] With update checks disabled, leave WebCom open past the startup delay and confirm no release-manifest request is made.
- [ ] Enable update checks and confirm a due background check starts after the startup delay without showing modal success/failure dialogs; leave WebCom running and verify the normal cadence is six hours.
- [ ] Open two profile windows before the same due check and confirm only one background update network operation is active; restart repeatedly and confirm `update-automation.v1` prevents repeated startup polling.
- [ ] Simulate transient endpoint failures: first two failures may use bounded quick retry; third and later failures must fall back to the normal six-hour cadence.
- [ ] With automatic verified downloads **off**, publish a valid newer manifest and confirm WebCom notifies/reviews the update but does not download the installer.
- [ ] Enable automatic verified downloads and confirm this also enables checks; a valid package may download/stage automatically only after size/SHA-256/Authenticode/product-version/publisher verification.
- [ ] Tamper the package, hash, product version, signature, or publisher and confirm no verified staged transaction is created.
- [ ] Confirm a background-staged installer never launches Setup automatically; installation still requires an explicit Updates-page action and preserves per-user/per-machine scope.
- [ ] Clear or successfully finish an update transaction and confirm future background checks are scheduled again.
- [ ] Verify v2 update settings migrate with check consent intact but automatic downloads disabled until explicitly enabled.

## Latest-50 source freeze

- [x] Portable GCC matrix passes **42/42** targets.
- [x] Portable Clang matrix passes **42/42** targets.
- [x] ASan + UBSan matrix passes **42/42** targets with leak detection.
- [x] Bundled blocker/YouTube script tests pass **8/8**.
- [x] Portable behavioral suites pass **1,184** explicit checks.
- [x] Source-preflight protects the background-update, release, installer, privacy and incident pipeline (**840 checks** in this source snapshot).
- [ ] Complete the Latest-50 Windows background-update acceptance section above on the exact signed candidate.

## Latest-49 update installation scope acceptance

- [ ] From a normal per-user installation, stage and install a signed newer release; confirm WebCom uses the existing per-user registration, passes `/CURRENTUSER`, and does not request unnecessary administrator elevation.
- [ ] From a machine-wide installation, stage and install a signed newer release; confirm WebCom passes `/ALLUSERS` and Windows requests administrator approval.
- [ ] Launch an unregistered/portable copy, stage a valid update, and confirm the Updates page reports unknown/portable scope and does not offer automatic installer launch.
- [ ] From the previous version, schedule a verified update and cancel/exit the installer before replacement; reopen the old WebCom version and confirm the transaction returns from `install-scheduled` to `verified` with the same target/hash/size metadata.
- [ ] Retry that staged update and confirm WebCom re-verifies size, SHA-256, Authenticode, product/version and publisher immediately before launching it again.
- [ ] Repeat per-user/per-machine/unknown-scope behavior for the verified rollback path; automatic rollback must remain disabled.

## Latest-48 final source freeze

- [x] Portable GCC matrix passes **41/41** targets.
- [x] Portable Clang matrix passes **41/41** targets.
- [x] ASan + UBSan matrix passes **41/41** targets with leak detection.
- [x] Bundled blocker/YouTube script tests pass **8/8**.
- [x] Source-preflight protects the consolidated release/security/incident pipeline (**778 checks** in this source snapshot).
- [x] Runtime process baseline, PE hardening verification, least-privilege installer defaults, safe uninstall, signed uninstaller path, dependency lock/SBOM/provenance, and public security/privacy/support/contribution policies are present in source.
- [ ] On real Windows, build the exact signed Stable candidate with `Build-ReleaseCandidate.ps1` and archive its package/provenance/SBOM evidence.
- [ ] Run `Test-ReleaseCandidate.ps1` **without** `-SkipMemoryGate` and **with** `-RunMachineInstall`; complete upgrade QA or the explicit first-release waiver.
- [ ] Complete Narrator/UI Automation and 100/125/150/200% + mixed-monitor DPI acceptance on the exact candidate.
- [ ] Choose the real public publisher identity, product/repository/support/security/privacy/release-notes HTTPS URLs, and OpenSource vs Proprietary distribution terms.
- [ ] Run `Approve-PublicRelease.ps1`; archive `PUBLIC-RELEASE.json`, its `.sha256`, and `PUBLIC-RELEASE-SHA256SUMS.txt`. Only then mark the binary public/stable.


- [ ] Fresh MSVC `build.cmd` succeeds after source preflight.
- [ ] `WebCom.exe` and the native UI test harness link successfully.
- [ ] Normal and private browsing work across common sites.
- [ ] Failed navigations show the native recovery card with the correct failed address/error; Try again retries the tab, Edit address focuses the omnibox, New Tab escapes cleanly, and no failed page content can replace those browser-owned controls.
- [ ] Multiple profiles remain isolated and can run simultaneously.
- [ ] History, bookmarks, sessions, and crash recovery behave correctly.
- [ ] New Tab quick actions open the expected native pages; recent-site cards deduplicate hosts and navigate the current tab; workspace cards show correct tab counts and switch/create workspaces without spawning extra WebViews.
- [ ] Password/autofill prompts and saved credentials behave correctly.
- [ ] The native Passwords & Autofill page toggles only the active profile settings, private tabs keep autosave/autofill disabled, clearing remains confirmation-gated, and the UI never claims to enumerate/decrypt/display saved credential values.
- [ ] Cookie and site-permission controls work per profile.
- [ ] Privacy Manager search filters cookie domains and permission origins together, live cookie/permission totals match the loaded profile data, no-match states remain usable, and per-domain Clear / per-origin Reset still affect only the selected target.
- [ ] SSL/security UI reports expected connection state.
- [ ] Downloads, local files, printing, find, and Developer Tools work.
- [ ] Workspaces, vertical tabs, split view, Reader mode, and capture tools work.
- [ ] Unpacked extension load/reload/remove and per-profile isolation work.
- [ ] Extensions v2 search matches extension name/ID/source path; All/Enabled/Disabled/Needs-attention counts and filters stay correct; changed/missing/unscannable sources surface as attention without marking the built-in blocker as missing-source.
- [ ] Ad Blocker v2 Site Information pause/resume changes the real network + cosmetic + YouTube-helper behavior for the selected host after extension/page reload; the saved list survives restart, caps at 64, Extensions reports the correct count, **Clear sites** restores blocking, global Turn off/Turn on remains independent, and Incognito clearly reports no bundled blocker.
- [ ] Delete a profile with Ad Blocker site exceptions and confirm both the copied bundled-extension directory and the profile's `AdBlocker/<profile>.v1` metadata are removed.
- [ ] Welcome setup progress reflects the real default-browser/bookmark/workspace/update-check state; each setup action opens the correct native/Windows surface and WebCom never silently changes defaults or imports data.
- [ ] Narrator announces the custom tab strip and selected/focused states correctly.
- [ ] Horizontal/vertical tab bounds and hit-testing are correct for accessibility clients.
- [ ] Stable manifests reject prerelease versions.
- [ ] Beta manifests accept correctly signed prerelease versions.
- [ ] Installer ProductName/ProductVersion match the release manifest.
- [ ] Rollback is offered only when centralized recovery readiness is `ready`.
- [ ] Recovery packages are re-verified immediately before launch.
- [ ] About WebCom shows the correct WebCom version, installed WebView2 Runtime, update channel/default-browser/profile state, and its Updates/Default Apps/Performance/Diagnostics/Data-folder actions open the expected surfaces.
- [ ] One-tab memory is measured after warmup and compared with the current release budget using `scripts/Measure-WebComMemory.ps1 -Scenario one-tab`; retain both CSV and JSON artifacts.
- [ ] Multi-tab, sleeping-tab, split-view, and multi-profile memory scenarios are recorded with the same Windows/WebView2/site/extension conditions and reviewed using average/median/p95/peak rather than a single sample.
- [ ] Shift+Esc Performance snapshot includes WebCom-host and WebView2-group memory separately plus current tabs/sleep/workspaces/sleep policy, and clearly labels partial/incomplete process reads.
- [ ] Update endpoint contact and verified package download occur only according to the saved opt-in settings; installer launch and rollback remain explicit and user-controlled.

- [ ] New Tab custom shortcuts persist per profile, reject unsafe/non-web URLs, enforce the eight-shortcut limit, and remove without affecting bookmarks/history.
- [ ] Recent-site **Pin** creates a custom shortcut and **Hide** removes the host from New Tab without deleting History; hidden hosts remain hidden after restart.

- [ ] History Top sites show correct visit counts, filter the History search box to the chosen host, and whole-site cleanup removes only that host after confirmation.
- [ ] Downloads show stable Today / Yesterday / date section headers without changing item order or action behavior.

- [ ] **Close duplicate tabs** affects only the current workspace and never closes the active, pinned, or download-owning copy; private/normal tabs remain distinct.
- [ ] **Sleep other tabs** stays workspace-scoped and skips all tabs rejected by the normal sleep-safety path.

- [ ] Performance timeout choices persist independently per profile; 1 minute uses Memory Saver, 5/15/30/Never use Balanced targets, Never disables automatic sleeping without disabling manual sleep.
- [ ] Idle performance polling follows the configured policy (10s/30s/60s/60s/off) and switching to Never removes the automatic `kTick` timer instead of leaving a fixed 10-second wakeup behind.
- [ ] A cold normal constructor does not read Downloads history, Site Permissions, Saved Sessions, or unpacked-extension source metadata/health; first-use UI/runtime paths load each store before mutation and preserve existing persisted records.
- [ ] Rapidly switch between already-created website tabs and confirm the destination WebView appears before the outgoing controller is hidden: no blank content frame or desktop-colored flash between tabs.
- [ ] Cold-launch WebCom at 100%, 125%, 150%, and 200% DPI and confirm the tab strip/address field never flash as a 1×1 child at the top-left before their first layout.
- [ ] Open/close omnibox suggestions repeatedly and confirm the owner-drawn panel appears at final bounds without an intermediate light/default background flash.
- [ ] Enable **Reduce motion** and confirm tab selection, tab reflow, omnibox hover, and dark-menu blend animations are suppressed; with Reduce motion off, Windows' client-animation setting still controls menu blending.
- [ ] **Never sleep this site** stores only the current normalized exact HTTP(S) host, survives restart, preserves the selected timeout, wakes an already-suspended tab when enabled, and blocks automatic sleep without blocking explicit manual sleep; clearing the list does not reset the timeout.

- [ ] Downloads **Source** never opens non-HTTP(S) or rejected addresses; **Retry failed** handles at most five candidates, keeps paused entries out, and auto-cleanup removes only completed history older than 30 days while leaving downloaded files untouched.

- [ ] Close normal tabs, restart WebCom, and confirm **Recently Closed** survives with workspace/group/pinned/keep-awake state; private/ephemeral tabs never appear, restore removes the recovered entry, and **Clear recovered** persists the empty list.
- [ ] Complete a download and confirm the native notice opens the finished file when available (otherwise Downloads); simulate an available update and confirm the notice opens Updates without installing automatically.
- [ ] Report a Problem copies a template that contains version/runtime/profile context but no browsing URLs, page titles, cookies, passwords, or form contents; diagnostics/recovery exports remain explicit and are never uploaded automatically.
- [ ] After a successful full browsing-data clear, confirm WebCom History, Recently Closed recovery, and saved site permissions are cleared while downloaded files and bookmarks remain; deleting a profile also removes its Home/Performance/Recently Closed metadata.

- [ ] Run `scripts/Test-WindowsAcceptance.ps1` from a fresh source extraction; require zero WebCom MSVC project warnings, packaged native UI replay success, and a successful isolated real-browser WebView2 smoke launch.
- [ ] Archive `windows-acceptance.json` and `build-last.log` for the release candidate; if using the 300 MiB one-tab target, archive the matching memory JSON/CSV too.


## Latest-40 accessibility and DPI acceptance

- [ ] At 100%, 125%, 150%, and 200% scaling, verify tabs, omnibox, menus, internal pages, focus rings, and WebView bounds remain aligned with no clipped hit targets.
- [ ] Move the window between monitors with different DPI values and confirm fonts/native buffers rebuild without stale-sized chrome or blank WebView gaps.
- [ ] Toggle Windows High Contrast while WebCom is open; browser chrome must switch live and return cleanly when High Contrast is disabled.
- [ ] Turn Windows client animations off with the WebCom profile Reduce Motion setting off; tab selection/reflow, omnibox hover, and native menu motion must still be suppressed.
- [ ] In a Remote Desktop session, confirm browser-chrome animation is suppressed without changing the saved profile preference.
- [ ] Use F6 to enter Settings/Privacy/Extensions/Recovery native actions, move with Tab/Shift+Tab or arrow keys, activate with Enter/Space, exit with Escape/F6, and verify a visible focus ring.
- [ ] Re-run Narrator/UI Automation/MSAA inspection of horizontal and vertical tabs and painted native-page actions. Latest-41 now exposes visible painted actions through an MSAA client wrapper, but real Windows assistive-technology testing is still required before claiming complete screen-reader compatibility.


## Latest-42 MSVC build-regression acceptance

- [ ] Fresh-extract Latest-42 and run `build.cmd` with the same/current Visual Studio Build Tools + Windows SDK; do not reuse an old v0.8/v2 build tree.
- [ ] Confirm `annotation_editor.cpp` compiles without C2672 `std::clamp` ambiguity and without C2668 `Gdiplus::Graphics::DrawLine` ambiguity.
- [ ] Confirm `WebCom-ui-tests.exe` compiles with the field-explicit `SeedProfile` helper and no `StartupMode`/`Continue` identifier errors.
- [ ] Require zero WebCom-project MSVC warnings in the Release Candidate acceptance report; warnings such as old hidden `origin`/`color_index`, unused `dip_width`, or unused `old_id` should not return.
- [ ] If a new MSVC failure occurs, capture the first compiler error block from `build-last.log`; do not treat the final PowerShell/CMake exit-code wrapper as the root cause.

## Latest-41 painted-action accessibility acceptance

- [ ] Open Settings, Privacy, Extensions, Recovery, History, Downloads, New Tab and the native failed-page recovery card; confirm visible painted actions appear to Narrator/MSAA inspection as `ROLE_SYSTEM_PUSHBUTTON` with meaningful names rather than anonymous client-area pixels.
- [ ] Confirm the address edit, native filter edit, toolbar buttons and other standard Win32 children remain present in the accessibility tree; the Latest-41 wrapper must append virtual actions instead of replacing the standard client provider.
- [ ] Move action focus with F6 + Tab/arrow keys and verify accessibility focus follows the visible focus ring and emits a focus change for the corresponding virtual child.
- [ ] Invoke painted actions through the accessibility default action and confirm they use the same command path as mouse/keyboard activation.
- [ ] Verify action screen bounds/hit testing track the actual painted rectangles at 100/125/150/200% DPI and after internal-page scrolling.
- [ ] Switch from an internal page to a web page, loading page, or private New Tab and confirm stale internal-page action children disappear from the accessibility tree.

## Latest-39 Release Candidate gate

- [ ] Run `scripts/Test-ReleaseCandidate.ps1` against the exact **signed** current installer and matching stable manifest intended for publication.
- [ ] Require the clean Windows acceptance component to report zero WebCom MSVC project warnings, successful packaged UI replay, a real WebCom window, and an observed descendant WebView2 process.
- [ ] Unless explicitly diagnosing memory, keep the default one-tab Release Candidate memory gate enabled with the **300 MiB working-set target** and archive its CSV/JSON report.
- [ ] Confirm `webcom-release-matrix-v2` verifies package hash/size, Authenticode status, publisher pin, channel policy, and installer ProductVersion against the matching manifest.
- [ ] Confirm `webcom-fresh-install-v2` launches the **installed signed browser**, observes WebView2, verifies version/Default Apps registration, then silently uninstalls and removes WebCom ProgIDs/RegisteredApplications entries.
- [ ] When a previous signed package exists, pass it as `-PreviousInstaller`; require the upgrade gate to preserve the isolated `state.v1` across installer replacement, launch both previous/current browsers against that profile, and clean registrations after uninstall. If no prior signed build has ever existed, pass `-NoPreviousSignedBuild` explicitly; the gate must never skip upgrade QA silently.
- [ ] Archive `release-candidate.json` plus every referenced component report and verify their SHA-256 values before approving the signed build.
- [ ] Do not publish merely because `Build-ReleaseCandidate.ps1` produced signed artifacts; publication requires the Release Candidate gate to pass on real Windows.


## Latest-45 supply-chain acceptance

- [ ] `dependencies.lock.json` is covered by the source hash manifest and pins exactly `Microsoft.Web.WebView2` `1.0.4314.35` to the official NuGet HTTPS source.
- [ ] Public-beta generation runs with `-RequireVerifiedDependencies`; official NuGet catalog SHA-512 retrieval succeeds and the downloaded package hash matches exactly.
- [ ] Staged package verification reports `officialHashVerified=true` and the packaged dependency lock matches the source lock SHA-256.
- [ ] `release-provenance.json` and `webcom.spdx.json` are produced beside the signed installer and release manifest.
- [ ] The RC gate passes the provenance source-manifest, dependency-lock, signed-dist, installer, release-manifest, and SBOM hash comparisons.
- [ ] Archive the SPDX SBOM and provenance report with the other candidate evidence; do not treat generation alone as release approval.

## Latest-44 package-integrity acceptance

- [ ] `build.cmd` completes staged-package verification and the portable package contains `docs/PACKAGE-VERIFICATION.json`.
- [ ] For a release-configured build, package verification confirms Release configuration, passed tests, UI harness presence, `/WX`, an HTTPS Stable manifest endpoint, and the exact compiled publisher SHA-256 pin.
- [ ] `Build-SignedInstaller.ps1` produces `signed-dist-verification.json` after signing `WebCom.exe`; require Authenticode status `Valid` and the expected publisher fingerprint.
- [ ] `Test-FreshInstall.ps1` records `installedExecutableSha256`, and `Test-ReleaseCandidate.ps1` confirms it exactly matches the signed staged `WebCom.exe` SHA-256.
- [ ] Intentionally add an unmanifested source file or alter a manifested source file and confirm package verification fails; restore the clean source before release.
- [ ] Intentionally place a PDB/OBJ/LIB/ILK artifact in the staged package and confirm verification fails.
- [ ] Do not publish `dist\WebCom-v3-Windows-*.zip` as the public release artifact; use the signed installer that passes the RC gate.

## Latest-48 incident withdrawal and recovery

- [ ] Preserve `PUBLIC-RELEASE.json`, the exact signed installer/Stable manifest, release evidence, RC reports, and published-health evidence before changing public hosting.
- [ ] For a last-known-good restore, run `scripts/Test-RollbackCandidate.ps1`; require the target to be an older approved Stable release, hash-identical to its prior approval, Authenticode-valid, and signed by the same pinned publisher certificate.
- [ ] Use `scripts/Prepare-ReleaseWithdrawal.ps1 -Mode WithdrawOnly` to prepare evidence/manual instructions when the affected release should simply be withdrawn.
- [ ] Use `-Mode RestoreLastKnownGood` only after rollback-candidate validation; publish the staged previous manifest/installer manually and then rerun the published-release health check against the previous approval.
- [ ] Do not expect an older Stable manifest to downgrade clients already running the affected newer release; use WebCom's verified user-triggered rollback where available or issue a higher-version hotfix through the full RC → approval → publication-health chain.
- [ ] Archive `RELEASE-WITHDRAWAL.json`, `INCIDENT-EVIDENCE.json`, their checksums, and the incident ZIP. Confirm the tooling reports `publicationPerformed=false`.
- [ ] For signing-key/dependency compromise, withdraw the endpoint, preserve evidence, follow `SECURITY.md`, rotate/revoke credentials as appropriate, and never weaken publisher/version verification to force recovery.

## Latest-47 release ceremony and live publication

- [ ] Prefer the elevated `scripts/Invoke-PublicRelease.ps1` workflow so build, real-Windows RC, machine-install acceptance, memory budget, upgrade/first-release policy, promotion, and evidence archive all refer to one exact candidate.
- [ ] Archive `PUBLIC-RELEASE-RUN.json`, `PUBLIC-RELEASE.json`, their SHA-256 files, and `WebCom-<version>-public-release-evidence.zip`.
- [ ] Publish artifacts manually; the release scripts must never upload on their own.
- [ ] Immediately run `scripts/Test-PublishedRelease.ps1` against the live Stable manifest.
- [ ] Require the live manifest and installer to hash-match the local approval, keep valid Authenticode/publisher identity, and remain on HTTPS without downgrade redirects.
- [ ] Require product, repository, support, security, privacy, and release-notes URLs to be reachable; archive `published-release-health.json` and its `.sha256`.
- [ ] If live verification fails, restore/withdraw the approved artifact rather than silently replacing it; any new binary must restart the RC → approval → publication-health chain.

