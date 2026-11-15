# WebCom v3 release notes

Current machine-readable release version: `3.0.0`.

## Current v3 feature work

- Latest-44 adds `Verify-BuildPackage.ps1`, a Windows-only staged-package gate for PE machine architecture, version resources, UI-harness architecture, blocker payload integrity, debug/compiler artifact leakage, exact source-manifest coverage, and release-build trust metadata.
- `Build.ps1` now verifies the staged package before portable ZIP creation and records `PACKAGE-VERIFICATION.json`. Release-configured builds record warning policy, Stable/Beta manifest endpoints, and the compiled publisher SHA-256 pin in `BUILD-INFO.txt`.
- Signed-installer creation verifies the staged package before signing and again after signing `WebCom.exe`; the post-sign pass requires valid Authenticode/publisher identity and writes `signed-dist-verification.json`.
- Fresh-install QA now records the installed `WebCom.exe` SHA-256 before uninstall. The Release Candidate gate requires the signed-dist report and rejects the installer if its installed executable differs from the signed staged executable.

- Latest-43 makes native WebCom and the Windows UI harness **MSVC warning-clean by construction**: `/W4 /WX` is enabled by default for project C++ sources, while angle-bracket third-party/system headers are marked external at warning level 0 so SDK noise does not masquerade as a WebCom regression.
- Added a diagnostic-only `build.cmd -AllowCompilerWarnings` path that disables `/WX` explicitly. Windows acceptance forwards that switch only when requested and retains its repository-source warning parser, so Release Candidate sign-off still fails project warnings by default.
- Source-preflight now protects the warning-policy wiring across CMake, `Build.ps1`, and `Test-WindowsAcceptance.ps1`, in addition to the concrete MSVC compiler regressions guarded in Latest-42.

- Latest-42 is a **log-driven MSVC regression hardening** pass based on the real Windows build failures from the older v0.8/v2 source line. The current annotation editor retains explicit Win32 `LONG` clamps and GDI+ `INT` line coordinates, while source-preflight now also rejects the exact ambiguous `std::clamp(point.x/point.y)` and raw `POINT` `DrawLine` patterns that previously slipped past preflight.
- The native Windows UI harness no longer seeds `SavedProfile` with a long positional aggregate. `SeedProfile` / `SeedNewTab` assign the required fields explicitly, including namespace-qualified `webcom::StartupMode::Continue`, so future `SavedProfile` field additions cannot silently shift the test initializer.
- Added `docs/MSVC-BUILD-REGRESSION.md` and expanded preflight protection around the old annotation, StartupMode/aggregate, and workspace-command collision classes. This does not replace a real Windows build; it makes known regressions fail earlier.

- Latest-41 extends accessibility semantics to WebCom's painted native-page actions without replacing the existing Win32 control tree. The main client accessibility object wraps the standard Win32 `IAccessible` provider and appends virtual action children with `ROLE_SYSTEM_PUSHBUTTON`, meaningful names, real screen bounds/hit-testing, focus state, `EVENT_OBJECT_FOCUS` notifications, and default-action dispatch through the normal browser command path.
- Added a portable native-accessibility model for bounded virtual child-ID mapping/navigation; the Windows provider uses that model directly. Web/loading/private-New-Tab states fail closed so stale painted actions are never exposed after switching away from an internal page.
- New Tab, failed-page recovery, Settings and many dynamic native-page actions now retain explicit accessibility labels alongside their command/bounds. Real Narrator/UI Automation/MSAA acceptance on Windows remains required before claiming complete assistive-technology compatibility.

- Latest-40 hardens Windows accessibility and DPI behavior before the signed Release Candidate run. Browser-chrome motion now follows the **effective** policy: the profile Reduce Motion preference, Windows client-animation policy, or Remote Desktop can each suppress tab/reflow/omnibox/menu motion. Windows High Contrast is composed with the per-profile high-contrast preference.
- Windows accessibility/theme changes now refresh live through `WM_SETTINGCHANGE` / `WM_THEMECHANGED` / `WM_SYSCOLORCHANGE`; custom menus/search popups dismiss on those transitions rather than keeping stale metrics/colors. Per-monitor `WM_DPICHANGED` clamps incoming DPI, rebuilds fonts/native buffers, relayouts, and explicitly redraws children.
- Painted internal pages now participate in keyboard navigation: F6 can enter visible native-page actions, Tab/Shift+Tab and arrow keys move the focus target, Enter/Space activates it, Escape returns to chrome, and a native focus ring identifies the active action.
- Added a portable accessibility-policy model/test suite plus expanded source-preflight guards for effective reduced motion, live High Contrast refresh, DPI rebuilding, popup dismissal, and native-page keyboard focus. Real Narrator/UI Automation and multi-DPI Windows acceptance is still required.

- Latest-39 adds a single **Release Candidate gate** that aggregates the exact signed build's Windows/MSVC/WebView2 acceptance, signed Stable/Beta manifest/package verification, signed fresh-install runtime/uninstall QA, default 300 MiB one-tab memory target, and optional previous-to-current installer upgrade compatibility into `webcom-release-candidate-v1`.
- `Test-FreshInstall.ps1` now verifies installed ProductVersion/FileVersion, launches the **installed signed browser** against an isolated QA profile, requires an observed WebView2 process, and emits `webcom-fresh-install-v2` before checking uninstall/Default Apps cleanup.
- Added `Test-UpgradeInstall.ps1`: it installs a previous signed package, launches it against an isolated profile, captures persisted `state.v1`, upgrades the same installation to the current signed package, verifies the installer did not mutate profile state before launch, then smoke-tests the upgraded browser against that same profile and checks uninstall registration cleanup.
- `Test-ReleaseMatrix.ps1` now emits `webcom-release-matrix-v2`; `Build-PublicBeta.ps1` explicitly warns that signed artifacts must not be published until the Release Candidate gate passes.

- Latest-38 adds profile-scoped **Recently Closed** persistence across restarts (up to 20 entries), restoring workspace/group/pinned/keep-awake context while excluding private/ephemeral tabs. History exposes direct restore and clear-recovered actions, and Ctrl+Shift+T uses the same recovery path.
- Download completion and update availability now use actionable native notices: completed downloads can open the finished file when still available, while update notices open the native Updates page.
- Added a native **Report a Problem** page with a privacy-safe copyable template, diagnostics/recovery export actions, data-folder access, and an explicit no-automatic-upload boundary.
- Privacy Manager now shows honest Ad Blocker profile state and paused-site count without claiming blocked-request telemetry that the host cannot reliably observe.
- Successful full browsing-data cleanup now also clears WebCom History, Recently Closed recovery, and saved site permissions while preserving downloaded files/bookmarks; profile deletion also removes Home, Performance, and Recently Closed metadata.
- Added a portable Recently Closed persistence suite plus MSVC source-preflight guards for recovery, actionable notices, privacy-safe support routing, and the extended cleanup paths.

- Latest-37 smooths native tab switching by making controller visibility a strict two-phase handoff: destination/split WebViews are revealed before outgoing controllers are hidden, preventing a tab-order-dependent blank content frame.
- The tab strip and address edit are now created hidden and only revealed after `Layout()` assigns their final bounds, eliminating the transient 1×1 startup child at the client origin that could appear as a top-left flash.
- Omnibox suggestions are positioned/revealed with redraw suppressed, then synchronously painted through the existing native buffer at their final bounds so USER32 does not expose an intermediate native background.
- Tab-selection and reflow animations now skip same-tab/reduced-motion cases before starting timers, and native dark-menu blend animation now honors both WebCom's Reduce motion preference and the Windows client-animation policy.
- Added portable chrome-layout checks for transition/reflow gating plus MSVC source-preflight guards for destination-first handoff, hidden startup controls, buffered suggestion reveal, and reduced-motion-safe menus.

- Latest-36 reduces idle CPU wakeups by replacing the fixed 10-second tab-sleep timer with an adaptive **10s / 30s / 60s / 60s / off** cadence for the 1m / 5m / 15m / 30m / Never policies. Changing the sleep policy immediately reschedules the timer.
- Persisted Downloads history, Site Permissions, Saved Sessions, and unpacked-extension source metadata/source-health scans are no longer loaded by the ordinary browser constructor; each bounded store now loads on the first UI/runtime path that needs it.
- Lazy stores are load-before-mutate safe: new downloads load existing persisted history before append, permission requests load saved rules before answering, saved-session edits load the current store before update, and unpacked-extension installs load existing source mappings before mutation.
- Fixed a Windows/MSVC compile regression in `LoadUnpackedExtension()` where `PWSTR raw` was declared twice; source-preflight now guards that exact failure pattern.

- Added a **Windows release acceptance gate** that clean-builds through `build.cmd`, treats WebCom MSVC project warnings as release failures by default, verifies packaged 3.0.0 version resources/blocker assets, replays the packaged native UI harness, and smoke-launches the real packaged WebView2 browser.
- Added a QA-only isolated data-root launch path guarded by `WEBCOM_QA_MODE=1`; it accepts only direct `%TEMP%\WebCom-Acceptance-*` roots, uses a separate acceptance mutex, avoids modal startup-error dialogs, and leaves ordinary WebCom profile data untouched.
- Windows acceptance can optionally integrate the one-tab memory sampler and emits a versioned `windows-acceptance.json` artifact plus the exact `build-last.log`.

- Replaced the legacy About message box with a first-class native **About WebCom** page showing WebCom/WebView2 version state, release channel, default-browser status, active profile, automatic-sleep policy, Never-sleep exception count, and direct links to updates/default apps/performance/support/data folder.
- Reworked **Shift+Esc / Performance snapshot** through a tested portable memory model. The live Windows snapshot now separates WebCom host memory from the WebView2 process group and includes tab/sleep/workspace state plus the active sleep policy.
- Expanded `Measure-WebComMemory.ps1` with named release scenarios (`one-tab`, `multi-tab`, `sleeping-tabs`, `split-view`, `multi-profile`), machine-readable JSON output, and average/median/p95/peak summaries while retaining explicit regression budgets.
- Added `docs/MEMORY-ACCEPTANCE.md` so memory targets are measured as matched Windows scenarios instead of inferred from single-process Task Manager screenshots.

- Added **Ad Blocker v2** site controls through the native site-information shield: normal-profile HTTP(S) tabs can pause/resume the bundled blocker for the current validated host, while Incognito remains explicitly outside bundled-blocker coverage.
- Site exceptions are bounded to 64 per profile and persisted atomically in a versioned `WEBCOM_AD_BLOCKER` store. Profile deletion removes that store.
- Site exceptions are applied to the real blocker, not only the UI: WebCom renders profile-specific `excludedInitiatorDomains` into every bundled DNR rule and generates the matching MAIN-world cosmetic/YouTube bypass config before extension registration.
- Changing the site list removes/re-adds the bundled extension through WebView2, refreshes Extensions, and reloads the affected active page. Failure to reload is fail-soft: the preference remains saved and WebCom tells the user a restart is required.
- Extensions now shows the bundled blocker's site-exception count, a **Clear sites** action, and health details covering profile state, DNR generation, cosmetic/YouTube bypass behavior, and the Incognito boundary.
- Bundled blocker tests expanded from 4 to 8 cases, including paused-site YouTube bypass, manifest ordering, DNR exception slots, and cosmetic-rule gating. A new portable Ad Blocker preference suite covers host parsing/validation, the 64-site bound, atomic persistence, shipped-template rendering, and fail-closed rule generation.

- Added **Extensions v2**: native search plus All / Enabled / Disabled / Needs-attention filters with live counts. Attention is derived from unpacked-source health, including missing mappings, changed sources, unavailable scans, and uninitialized baselines.
- Saved unpacked sources are searchable too, and the Extensions compatibility notice explicitly states that WebCom does **not** claim Chrome Web Store compatibility or browser-owned Chrome toolbar/popup UI.
- Added a portable `extension_view_model` with dedicated search/filter/count/attention tests and Windows source-preflight guards.
- Upgraded Welcome into a live four-step setup checklist for **Default browser, Bookmarks, Workspaces, and Update checks**. Every action remains explicit; Windows Settings keeps final control over default-browser selection.
- Added portable onboarding-progress logic and regression guards for the setup checklist wiring.

- Rebuilt failed-navigation UX as a native WebCom recovery card with the failed address/error plus **Try again**, **Edit address**, and **New Tab** actions.
- Removed the old single floating reload button on failed tabs and routed recovery hit-testing through browser-owned native actions, including hover feedback.
- Added an explicit trust-boundary message explaining that the failed site cannot replace WebCom's recovery controls.
- Added MSVC source-preflight guards for all three recovery actions and the native failure-page boundary.

- Added a first-class native **Passwords & Autofill** manager (`webcom:passwords`) and routed the Settings/main-menu credential entry to it.
- The manager exposes the WebView2-supported profile controls: offer-to-save-passwords, general form/autofill enablement, clear saved passwords, and clear autofill form data. Private-tab save/autofill behavior remains disabled through the existing WebView2 settings path.
- Added an explicit in-product privacy boundary: WebCom does **not** read, list, export, decrypt, or display saved password values because this WebView2 API surface exposes settings/clearing rather than a safe credential-enumeration interface.
- `webcom:passwords` is accepted by the address resolver and restorable-session metadata so the manager behaves like other native WebCom pages.
- Added source-preflight guards for native routing, clearing controls, and the no-password-enumeration claim.

- Upgraded the native **Privacy Manager** with a shared search field that filters both cookie domains and saved permission origins without mutating profile data.
- Added live profile summary totals for stored cookie domains, cookie count, permission origins, and saved permission rules.
- Filtered cookie/permission rows compact their native layout while preserving the original per-domain **Clear** and per-origin **Reset** command targets; empty searches show explicit no-match feedback.
- Added source-preflight guards so Privacy Manager remains searchable and retains its summary/filter wiring.

- Added a per-profile **Never sleep these sites** list for automatic tab sleeping. The list is bounded to 64 exact normalized HTTP(S) hosts and is stored in the existing versioned `WEBCOM_PERFORMANCE` preference file alongside the sleep timeout.
- Added a tab-context action to add/remove the current site's exact host plus a Performance-menu action to clear the whole exception list. Adding an exception wakes a currently suspended tab immediately.
- Never-sleep exceptions affect **automatic** sleeping only; the explicit/manual sleep actions remain available. Timeout changes preserve site exceptions, and exception changes preserve the chosen timeout.
- Hardened the performance preference parser/serializer with DNS-style host validation, duplicate suppression, bounded loading, atomic replacement, reserved-profile rejection, and source-preflight guards for all exception paths.

- Expanded Downloads with per-entry **Source** actions that reopen only validated HTTP(S) origins through WebCom's normal address resolver.
- Added confirmation-gated **Retry failed** handling. WebCom resumes current-session failures when WebView2 allows it; otherwise it reopens at most five validated source pages in background tabs so bulk retry cannot steal focus repeatedly or create an unbounded tab burst.
- Added automatic retention cleanup for completed download-history entries older than 30 days. Failed, interrupted, paused, and active records are never removed by this policy, and downloaded files on disk are never deleted.
- Added a portable download-history model with hard retry bounds, retention-boundary tests, and source-preflight guards for the safety contract.

- Added per-profile configurable automatic tab sleeping: **1 minute / 5 minutes / 15 minutes / 30 minutes / Never**. The setting is stored in a separate versioned `WEBCOM_PERFORMANCE` file so the existing session/state schema remains backward compatible.
- The **1 minute** option keeps WebCom's existing aggressive Memory Saver target; the longer/disabled timeout choices keep the normal Balanced memory target while changing only automatic suspension timing.
- Settings, the main preferences menu, and Quick Commands now open a dedicated Performance control surface instead of exposing only a binary Memory Saver switch. Manual **Sleep background tabs now** remains available even when automatic sleeping is set to Never.
- Added portable timeout-policy tests, atomic preference persistence tests, reserved profile-name rejection, and MSVC source-preflight guards for all five timeout modes and their native wiring.

- Added workspace-scoped **Close duplicate tabs**. Candidate selection is handled by a portable model that preserves URL path/query case, keeps private/normal tabs distinct, and protects the active tab, pinned tabs, and tabs that own downloads.
- Added **Sleep other tabs** for the current workspace. Every candidate is passed through WebCom's existing suspend safety logic rather than forcing protected/audible/loading/download-active tabs to sleep.
- Added both actions to the tab context menu and Quick Commands.
- Added **History Top sites** chips with visit counts. Selecting a chip filters History to that host; its inline clear action removes that host's History entries only after confirmation and leaves cookies, permissions, bookmarks and downloads untouched.
- Added portable History host normalization/aggregation/deletion helpers with bounded top-site output and dedicated tests.
- Downloads now use the same local-date section headers as History, making long lists easier to scan by Today / Yesterday / date without changing persisted order.
- Added per-profile **custom New Tab shortcuts** with native Add/Remove controls and an eight-tile hard bound. Shortcut URLs are validated through WebCom's normal safe address resolver before persistence.
- Added recent-site **Pin** and **Hide** controls. Pin promotes a recent site into the custom shortcut set; Hide stores only a bounded host preference and deliberately leaves browsing history untouched.
- Added a versioned `WEBCOM_HOME_PREFERENCES` local format with atomic writes, bounded shortcut/hidden-host counts, and corruption-tolerant loading.
- Reused WebCom's native DPI-aware dialog through a generic text-entry mode for shortcut name/address entry instead of adding another renderer.
- Upgraded the native New Tab page into a lightweight dashboard without introducing another renderer or background service.
- Added direct New Tab quick actions for History, Downloads, Bookmarks, and Saved Sessions.
- Added bounded recent-site cards derived from History; repeated visits to the same normalized host collapse to the newest entry and full URLs are not shown on the card surface.
- Added workspace cards with live tab counts, active-workspace emphasis, one-click switching, workspace management, and New workspace creation when capacity allows.
- Added a portable `home_model` with dedicated tests plus source-preflight guards for the native dashboard wiring and bounded recent-site command range.

## Current v3 Windows/MSVC hardening

- Kept screenshot annotation coordinate clamping explicitly typed as Win32 `LONG` and GDI+ line coordinates explicitly cast to `INT`, preventing MSVC overload ambiguity.
- Native Windows UI tests include `storage.h` and use fully qualified `webcom::StartupMode::Continue`.
- Workspace rename/delete menu commands use dedicated `RenameWorkspaceCmd` / `DeleteWorkspaceCmd` identifiers instead of colliding with member-function names.
- Source preflight guards all three Windows-only compile-regression classes before the main browser target is compiled.
- Public product branding/version surfaces are normalized to **WebCom v3 / 3.0.0**; persistence and signed-manifest schema numbers remain unchanged for compatibility.

## Highlights

- Native Chrome-like Windows browser shell.
- Multiple profiles and workspaces.
- Vertical/horizontal tabs and tab search.
- History, bookmarks, passwords, cookies, and site controls.
- SSL/security details and permission management.
- Unpacked extension management and diagnostics.
- Reader mode, split view, capture, and annotation tools.
- Session recovery and reliability diagnostics.
- Signed update verification and rollback readiness checks.
- Stable/Beta channel policy.
- Accessibility support for the custom tab strip.
- Repeatable memory regression measurement.

## Recent Windows/MSVC hardening

- Separated workspace menu command IDs from member-function names.
- Fixed Win32 `LONG` / C++ `int` ambiguity in screenshot annotation coordinates.
- Forced GDI+ line drawing to an unambiguous integer overload.
- Qualified startup-mode references in the native Windows UI test harness.
- Expanded source-preflight coverage to catch these Windows-only compile regressions before the main browser build.
- Added contributor-focused build documentation and a GitHub-ready project README.
