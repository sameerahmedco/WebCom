<p align="center">
  <img src="resources/webcom-icon.png" width="112" alt="WebCom logo">
</p>

<h1 align="center">WebCom v2</h1>

<p align="center">
  <strong>A familiar, native Windows browser focused on speed, profiles, privacy controls, and a clean Chrome-like experience.</strong>
</p>

<p align="center">
  <img alt="Version" src="https://img.shields.io/badge/version-v2%20%7C%202.0.0-2f81f7">
  <img alt="Platform" src="https://img.shields.io/badge/platform-Windows%2010%2F11-0078d4">
  <img alt="Language" src="https://img.shields.io/badge/C%2B%2B-20-00599c">
  <img alt="Renderer" src="https://img.shields.io/badge/renderer-WebView2-0f9d58">
  <img alt="Status" src="https://img.shields.io/badge/status-public%20beta-f0a500">
</p>

---

## What is WebCom?

WebCom is a Windows web browser written in **native C++20 / Win32** with **Microsoft Edge WebView2** for web rendering. The browser chrome itself—tabs, toolbar, menus, bookmarks bar, profile controls, native pages, and much of the surrounding UX—is built by WebCom rather than being a reskinned web page.

It is designed for people who want a familiar Chromium-style browsing experience with strong local profile separation, practical power-user tools, and deliberate control over browser resource use.

> **Version naming:** historical internal 0.5.x–0.8.x Windows builds are the **WebCom v1 generation**. The current public generation is **WebCom v2**, with machine-readable version **2.0.0** for Windows resources, installers, and updater comparisons.

## Our goal

Our goal is to build a browser that feels immediately familiar without turning the browser shell into a heavy second application layer. WebCom aims to combine a polished native Windows interface with isolated profiles, useful productivity features, transparent privacy/security controls, safe updates, and measurable performance budgets.

We are especially focused on:

- **Familiar UX** — Chrome-like navigation without copying Chrome internals.
- **Native feel** — Win32 controls and native windows where they make sense.
- **Profile isolation** — separate browsing contexts for work, personal, testing, and more.
- **Resource discipline** — avoid unnecessary hidden renderers and continuously measure memory regressions.
- **Security clarity** — visible connection state, site permissions, signed update verification, and no silent certificate bypasses.
- **Useful power features** — workspaces, vertical tabs, split view, reader mode, captures, sessions, recovery, and extension developer tools.

## v2 features

- Chrome-like native UI
- Multi-profile browsing
- Workspaces
- Vertical tabs
- Tab search
- History + bookmarks
- Password saving
- Autofill controls
- Cookie controls
- Site permissions
- SSL/security details
- Unpacked extensions
- Extension diagnostics
- Reader mode
- Split view
- Page capture + annotation
- Named sessions
- Crash recovery
- Safe updates
- Verified rollback
- Stable/Beta channels
- Accessibility support
- Memory regression checks

## Quick start

### Build from source

1. Use **Windows 10 or Windows 11 x64**.
2. Install **Visual Studio 2022 Build Tools** with **Desktop development with C++**, a Windows SDK, CMake, and Ninja.
3. Extract or clone this repository into a normal writable folder.
4. Run:

```bat
build.cmd
```

5. After the build succeeds, run:

```bat
run.cmd
```

The packaged browser will be created at:

```text
dist\WebCom\WebCom.exe
```

For the complete beginner-friendly setup, build, run, clean-build, and troubleshooting guide, read **[HOW_TO_BUILD.md](HOW_TO_BUILD.md)**.

## Repository map

| Path | Purpose |
| --- | --- |
| `src/` | Browser, Win32 UI, WebView2 integration, models |
| `tests/` | Portable model tests + native Windows UI regression harness |
| `tools/` | Source preflight checks for known MSVC/WebView2 failure classes |
| `scripts/` | Build, SDK restore, memory measurement, release/update tooling |
| `resources/` | Icons and Windows resources |
| `docs/` | Architecture, validation, roadmap, changelogs, beta checklists |
| `.github/workflows/` | Windows and portable CI builds |

## Build and test philosophy

The Windows build runs a dedicated **MSVC source preflight before the main browser compile**. This catches several classes of problems that previously appeared only on Windows, including COM/WebView2 interface mistakes, Win32 `LONG` vs `int` template ambiguity, and command identifiers that collide with member-function names.

The portable C++ models are also tested separately on non-Windows compilers. Native Win32/WebView2 behavior still requires a real Windows acceptance run before a release is considered ready.

## Current v2 build note

The latest Windows log successfully restored and verified the pinned WebView2 SDK and passed the source preflight before reaching the browser compile. The next MSVC failures were isolated to screenshot-annotation coordinate overloads and two unqualified test references. Both are corrected in the current source, and the preflight now guards those cases as well. A fresh Windows build remains the final confirmation for the native path.

## Browser engine and extension scope

WebCom uses the **Microsoft Edge WebView2 Runtime** for rendering web content. WebCom is not a Chromium fork and does not claim full Chrome browser API compatibility. Unpacked extensions are supported through WebView2's extension APIs where WebView2 supports their behavior; Chrome Web Store installation and browser-owned Chrome toolbar/popup UI are not emulated.

## Project status

**WebCom v2 is a public-beta codebase, not yet a final production release.** Before broad distribution, validate the browser on real Windows hardware for navigation, profile isolation, accessibility, signed update/rollback behavior, downloads, permissions, password/autofill UX, DPI scaling, and memory use.

Useful project documents:

- **[How to build and run](HOW_TO_BUILD.md)**
- **[Architecture](docs/ARCHITECTURE.md)**
- **[v2 features](docs/FEATURES-v2.md)**
- **[Roadmap](docs/ROADMAP.md)**
- **[Validation boundary](docs/VALIDATION.md)**
- **[Public beta checklist](docs/PUBLIC-BETA-CHECKLIST-v2.md)**

---

## Development history

The sections below preserve the internal milestone history for traceability.

## 0.7.9 extension source health and developer diagnostics

Each unpacked extension source mapping now carries an optional **source change stamp**. Existing 0.7.8/v1 metadata remains readable; the next save upgrades it to the v2 schema while preserving the same per-profile metadata filename. A successful Load/Reload records the current stamp. The Extensions page then classifies that source as **unchanged**, **changed - reload recommended**, **baseline pending - reload once**, or **scan unavailable**.

The stamp is intentionally a developer convenience, not a trust mechanism. It is derived from a bounded scan of up to 4,096 regular files using relative paths, sizes and modification times. It does not replace WebView2 validation, Authenticode, extension signing, malware scanning or a cryptographic content-integrity check.

Extension **Details** now shows source scan file/byte counts and change state in addition to manifest identity. Manifest diagnostics also include optional permissions, optional host permissions, minimum Chromium version, commands, omnibox integration, DevTools page declarations, web-accessible resources and side-panel declarations. The existing toolbar/popup warning remains, and side-panel declarations are called out because WebView2 does not supply Chrome's browser-owned extension UI host.

The **Known unpacked sources not currently installed** section now exposes **Details**, **Reload source**, **Open folder**, and **Forget source**. Forget removes only WebCom's saved mapping; it never deletes the extension folder and never removes an installed extension. This gives developers a clean way to discard stale or moved source mappings without touching disk contents.

Installed extensions still use WebView2's supported `ICoreWebView2Profile7` / `ICoreWebView2BrowserExtension` APIs for enumeration, install, enable/disable and remove. Direct Chrome Web Store installation, Chrome Sync, and emulation of Chrome toolbar/icon/badge/popup UI are not claimed.

## 0.7.7 Chromium extension manager

WebCom now exposes a native `webcom:extensions` page from the main menu, Settings and Quick Commands. **Load unpacked...** opens a native folder picker and accepts the top-level folder of a Chromium extension containing `manifest.json`. Before installation, WebCom performs a bounded 2 MiB manifest preview to show the extension name, version and manifest version; WebView2 remains the authority for actual extension validation and installation.

Installed extensions are managed per WebCom profile through `ICoreWebView2Profile7`. The page lists extension name, ID and enabled state and provides immediate **Enable/Disable** and **Remove** actions. WebView2 persists installed/disabled state in the corresponding profile, so Work/Personal profiles can keep different extension sets. Profile deletion releases WebCom's extension/profile COM references before WebView2 deletes the profile data.

The environment explicitly enables extensions through `ICoreWebView2EnvironmentOptions6::put_AreBrowserExtensionsEnabled(TRUE)`. A hidden ephemeral bootstrap WebView is created only when the Extensions manager needs a profile object before any normal website has been opened; it is closed as soon as that profile is acquired and is excluded from session persistence.

**Compatibility limitation:** this release supports WebView2's unpacked-extension API, not direct Chrome Web Store installation. Extensions that rely mainly on content scripts/background/service-worker behavior can work where WebView2 supports them, but Chrome toolbar icon/badge/popup entry points are not supplied by WebView2. WebCom detects common toolbar/popup declarations in the selected manifest and warns before loading those extensions. The selected unpacked folder is referenced in place rather than copied, so it must remain available.

## 0.7.6 recovery center, retry backoff and explicit reliability reports

WebCom now has a native **Recovery & Reliability** page available from Settings, Help and Quick Commands. It summarizes whether the previous process ended uncleanly, the current update transaction phase, failed update-health starts, rollback recommendation, the preserved recovery installer version, and current update-check retry/backoff state. When rollback is recommended and the preserved recovery package matches the recorded previous version, the existing re-verified rollback action is exposed directly from this page.

Update checks are still user-triggered, but failed checks now enter a persistent bounded retry window: 15 seconds after the first failure, then 30, 60, 120, and a maximum of 300 seconds. The state stores only a failure count, retry deadline and coarse failure category. A successful signed-manifest check resets the backoff; changing release channel or disabling checks resets it as well. Automatic checking remains disabled.

**Export recovery report** creates a local ZIP containing `recovery.txt`, `diagnostics.txt`, `privacy.txt`, and the bounded `runtime.txt` when available. The report is never uploaded automatically. The privacy text explicitly documents URL redaction and excluded secret categories and asks users to review every file before sharing.

The MSVC/security source preflight was expanded to enforce the bounded retry policy, atomic retry-state writes, local-only recovery-report export, and the presence of the explicit privacy notice.

## 0.7.5 crash evidence and verified rollback recovery

WebCom now keeps a **bounded local runtime log** under its data root. The current log is capped and rotated to one previous generation instead of growing indefinitely. It records operational lifecycle events such as session start/clean shutdown, WebView2 process failures, update checks/staging, health retries and rollback activity. Event details are length-bounded and web/file URLs are redacted. Browsing URLs, cookie values, password values and form contents are not intentionally logged.

A tiny runtime session marker records whether the previous WebCom process reached a clean shutdown. If the marker is still active on the next start, WebCom records local crash/unclean-exit evidence without uploading anything. Support Diagnostics can export a ZIP **with or without** the bounded runtime log; inclusion requires an explicit Yes/No choice at export time.

After an update passes its health window, WebCom re-verifies the signed installer that produced the now-known-good version and preserves exactly one recovery generation. If a later update misses its health window twice, the Updates page can offer **Rollback to previous WebCom…** only when that preserved package matches the transaction's previous version. The recovery installer is fully re-verified again immediately before the explicit UAC launch. No automatic rollback occurs.

## 0.7.4 secure update check and verified staging

The native **Updates** page now supports a user-triggered release check when update consent is enabled and the release build contains configured Stable/Beta HTTPS manifest endpoints plus a pinned Authenticode publisher-certificate SHA-256. Developer builds leave those release settings blank and fail closed without contacting a placeholder server.

A successful check requires a v2 manifest for the selected channel, a newer semantic release version, and a manifest publisher fingerprint that exactly matches the publisher pin compiled into the running WebCom build. **Stage verified update** then downloads the installer on a worker thread to WebCom's data directory and only commits it after exact byte-size, SHA-256, Windows version-resource product/version, Authenticode trust, and signer-certificate fingerprint checks pass.

**Install verified update** is still explicit. WebCom re-verifies the local installer, asks again, records the update transaction, and launches the signed installer through Windows elevation. Automatic checks, downloads and installs remain off.

Post-update startup now has a local health transaction. The first successfully installed target enters `awaiting-health-check`; 30 seconds of normal runtime or a graceful close marks it healthy. A crash/termination before the health checkpoint is recorded on the next startup, and repeated failed health starts produce a rollback recommendation. WebCom does not automatically replace binaries for rollback in this milestone.

Release builds can pass `-StableManifestUrl`, `-BetaManifestUrl`, and `-PublisherSha256` to `scripts/Build.ps1`, or use `scripts/Build-PublicBeta.ps1` to build the browser with its trust roots, sign/package it, generate the v2 release manifest, and verify the resulting installer.

## 0.7.3 MSVC build hardening and preflight

WebCom now compiles a portable `webcom_source_preflight` executable and runs it as a CTest target. On Windows, `Build.ps1` also builds and executes that preflight **before compiling the native browser**. The scan targets failure classes that previously appeared only under MSVC: direct CookieManager access through the wrong WebView2 interface, COM/WebView2/GDI+ include-order regressions, mixed-type `auto` declarators, duplicate `internal_page` declarations, and `std::min`/`std::max` expressions that mix Win32 `LONG` geometry with DPI `int` math without an explicit common type.

The production and native UI-test targets also compile a dedicated `windows_header_smoke.cpp` translation unit containing the COM, WebView2, GDI+ and favicon header chain. This intentionally localizes Windows SDK/include-order breakage instead of letting it surface deep inside `app.cpp`.

During this hardening pass, several native layout expressions were normalized with explicit integer casts before `std::min`, eliminating another potential MSVC `LONG`/`int` deduction ambiguity. The existing `ICoreWebView2_2` CookieManager helper remains the only CookieManager access path.

The source preflight currently performs 69 MSVC-oriented source checks and the full portable release matrix now totals 884 explicit checks across 22 CTest targets.

## 0.7.2 local files and file associations

`Ctrl+O` now opens a native Windows file picker for `.html`, `.htm`, and `.pdf` files. The same supported files can be opened by passing an absolute Windows path to `WebCom.exe`, which also makes Windows file-association activation safe and deterministic. Local paths are canonicalized to `file:///` URLs; UNC/network file URLs, query strings, traversal segments, malformed percent escapes, unsupported file extensions, and control characters are rejected. PDF/HTML fragments remain supported.

Local files are rendered by the existing WebView2 tab and can use normal browser back/forward, print, find, capture and developer tools. WebCom does not add local-file visits to browsing History or bookmarks and does not expose website cookie/permission controls for them. It also does **not** enable Chromium's `--allow-file-access-from-files` switch.

The installer now advertises `WebCom.HTML` and `WebCom.PDF` ProgIDs plus Default Programs `FileAssociations` for `.html`, `.htm`, and `.pdf`. Windows Settings remains the authority for selecting defaults. The fresh-install QA script verifies those capabilities and their uninstall cleanup.

Session metadata now safely restores supported local-file tabs. The storage allow-list was also brought up to date for current native pages such as Diagnostics, Updates, Sessions and Privacy, preventing a management tab from causing an otherwise valid session save to fail.

## 0.7.1 default-browser and first-run readiness

The Inno Setup installer now registers a `WebCom.Url` ProgID plus Windows Default Programs capabilities for `http` and `https`, using Inno's `HKA` root so administrative installs register machine-wide while `/CURRENTUSER` installs register per-user. `ChangesAssociations=yes` refreshes Explorer's association choices after install/uninstall. No user-choice/default hash is written by WebCom.

A brand-new WebCom data root now opens one native **Getting Started** tab. Existing users with a current or backup session file are not interrupted after upgrade. The welcome state is stored in a tiny atomic `onboarding.v1` file, and the welcome tab itself is ephemeral so it cannot become part of session restore. The page links to Windows Default Apps, update choices, keyboard shortcuts and Start browsing.

Settings, Help and Quick Commands can reopen Getting Started or Windows Default Apps at any time. The shared address resolver now also recognizes all current native management pages consistently.

## 0.6.9 screenshot annotation, support bundles and signed update verification

The capture menu now includes **Annotate visible area** and **Annotate selected area**. The lightweight native editor provides Pen, Rectangle, Undo, Copy, Save PNG and Done using standard Win32 buttons, so the annotation toolbar is keyboard-focusable without allocating another WebView. The inherited duplicate local declaration in the v0.6.8 capture menu was also removed during this pass.

Support Diagnostics can export a bounded ZIP containing diagnostics, update-channel status and a privacy note. Bundle filenames reject traversal/nested paths, individual entries are bounded, and the generated support package deliberately excludes browsing URLs, cookie values, password values, form contents and authentication data.

The update manifest format now supports **WEBCOM_RELEASE v2**, which adds package size, `authenticode` signature requirement and a pinned SHA-256 publisher-certificate fingerprint. Legacy v1 manifests still parse for compatibility but are never install-eligible. `scripts/New-ReleaseManifest.ps1` generates v2 metadata only from an already-valid Authenticode-signed package, while `scripts/Verify-UpdatePackage.ps1` verifies package size, SHA-256, Authenticode trust and the publisher certificate fingerprint before accepting the package. Automatic installation is still disabled.

## 0.6.8 support diagnostics, Reader export and release-channel groundwork

A new native **Support Diagnostics** page reports the WebCom version, installed WebView2 Runtime version, immutable profile key and display label, active workspace, tab/sleep counts, workspace count, tab layout, motion/contrast state, release channel and WebCom data path. It deliberately does not include browsing URLs, cookie values or password values. Diagnostics can be copied to the clipboard or the local WebCom data folder can be opened directly.

Reader Mode now has an **Export** menu with plain text, print-friendly HTML and Markdown. HTML export carries the Reader text-size, width and serif/sans choices into a self-contained UTF-8 document with print CSS; Markdown contains the title, original source and extracted article text. Reader content remains ephemeral and excluded from crash/session restore.

WebCom also gains persistent global **Stable/Beta release-channel** selection and a bounded portable release-manifest parser that accepts only valid semantic release versions, compact HTTPS package URLs and 64-hex SHA-256 digests. This is updater groundwork only: automatic download/install remains disabled until cryptographic signature verification is implemented.

## 0.6.7 startup behavior and keyboard accessibility

Each normal profile now has an **On startup** preference with three choices: **Continue where you left off**, **Open the New Tab page**, or **Open specific pages**. Specific-page mode accepts up to 16 complete HTTP(S) URLs, rejects embedded URL credentials and case-insensitive duplicates, can add the current page directly, and can remove individual configured pages from the native menu. Session metadata advances to v7 while v1-v6 continue to load. Existing profiles migrate to the established Continue behavior.

Crash/session recovery remains separate from startup preference. If WebCom falls back to `state.v1.bak` and the user chooses **Keep recovered tabs**, the recovered session overrides the configured startup mode for that recovered run. Choosing **Start fresh** continues to sanitize the recovery generations as introduced in v0.6.6.

**F6 / Shift+F6** now cycles focus among the native tab strip, address bar and current page. WebView pages receive the matching WebView2 forward/backward focus reason, while the custom tab strip exposes a richer accessible label containing the active workspace, selected tab position and title, and emits a Windows name-change accessibility event when that information changes.

## 0.6.6 recovery controls, screenshot clipboard and profile shortcuts

The screenshot menu now supports **Copy visible area** and **Copy selected area** in addition to saving PNG files. WebCom captures through the existing WebView2 preview path, converts the result to a Windows bitmap, and transfers ownership to the clipboard only after `SetClipboardData` succeeds.

If `state.v1` is unreadable and WebCom successfully restores `state.v1.bak`, startup now asks whether to **Keep recovered tabs** or **Start fresh**. Starting fresh discards only recovered tab/group state while retaining profiles, workspaces, bookmarks, cookies, passwords, history and preferences. The sanitized fresh state is written to both the primary and recovery generation so a later failure cannot resurrect the discarded tabs.

Each profile menu now also offers **Create desktop shortcut**. The `.lnk` targets the current WebCom executable with `--profile "<immutable profile key>"`, so a custom display label can change without breaking the shortcut or moving browser data.

## 0.6.5 Reader defaults, empty folders and profile setup exchange

Reader Mode now remembers text size, content width and serif/sans choice **per profile**. Changing a Reader control updates that profile's default for future Reader tabs; Appearance storage advances to v5 while v1-v4 files migrate to the comfortable sans defaults. Reader content remains ephemeral and never becomes part of the saved profile.

Bookmark folders are now first-class metadata instead of existing only when at least one bookmark points at them. The Bookmark Manager can create empty or nested folders, show them before any bookmark is moved inside, rename an entire folder tree, drag bookmarks into them, and safely remove empty folder trees. Explicit folder metadata is stored separately so the established bookmark-file format and Chromium/Netscape HTML interoperability remain backward compatible.

Profiles can export/import a **WebCom profile setup** file containing browser preferences only: theme/accent, bookmark-bar visibility, motion/contrast, Reader defaults, password/autofill prompt preferences and vertical-tab layout. The exchange deliberately excludes the immutable profile key, tabs/URLs, bookmarks, cookies, history, downloads and every password value.

## 0.6.4 accessibility and safe external links

WebCom chrome now has a **High contrast WebCom chrome** mode and automatically honors Windows High Contrast when the OS enables it. The high-contrast palette uses deterministic black/white/yellow browser surfaces with enhanced text/focus contrast while leaving website content unchanged. Appearance storage advances to v4 and older v1-v3 files migrate with normal contrast.

A new searchable **Keyboard Shortcuts** native page documents the major tab, workspace, Reader, capture, History, Downloads and performance shortcuts without creating another WebView. Settings and Help link directly to it.

Web pages can now hand off only a deliberately small set of external protocols: `mailto:`, `tel:` and `sms:`. Each request is parsed by a portable allow-list, cancelled inside WebView2 first, then requires an explicit user confirmation before Windows receives it. Unknown/custom schemes, `file:`, `javascript:` and malformed/control-character payloads remain blocked. The address bar itself remains HTTP(S)-only.

## 0.6.3 direct manipulation and profile lifecycle

`Ctrl+Shift+S` now offers **Select area**, using a temporary native Win32 drag overlay over the visible WebView and cropping the existing `CapturePreview` PNG with bounded geometry. Tab dragging exposes live workspace drop targets in horizontal and vertical layouts, including collapsed vertical tabs. Bookmark Manager drag handles can move bookmarks directly onto folder headings or back to the bookmarks bar while preserving same-folder reordering. Profiles can now have a user-facing display name stored in Appearance metadata without renaming the immutable WebView2 profile key or browser-data directory; reset-appearance preserves that label.

## 0.6.2 capture, Reader polish and accessibility

`Ctrl+Shift+S` now opens a capture picker for the current visible area or the full scrollable page. Full-page capture uses the WebView2 DevTools transport and a bounded host-side screenshot decoder; the existing `CapturePreview` viewport path remains available. Reader Mode adds text-size controls, three content widths, serif/sans selection, clipboard copy and UTF-8 text export while keeping extracted content ephemeral. Workspaces can be changed with `Ctrl+Alt+Arrow` or `Ctrl+Alt+1…9`, new profiles can choose a color during creation, and Appearance includes a per-profile **Reduce WebCom motion** setting.

## 0.6.1 reading, sessions and privacy feature set

- **Reader Mode (F9):** extracts readable text from the current HTTP(S) page into a temporary WebCom-native reading tab. Reader text stays in memory, is excluded from crash/session persistence and recently-closed restore, and the original page remains one click away.
- **Named Sessions:** save the current workspace's website tabs under a reusable name, reopen them later as new tabs, overwrite deliberately, or delete them. Named sessions use a separate bounded atomic per-profile store instead of overloading crash recovery.
- **Privacy Manager:** native all-sites view of cookie domains/counts plus saved Camera/Microphone/Location/Notifications permission exceptions. Cookie values are never displayed. Individual cookie domains and permission origins can be cleared/reset, or all cookies/site storage can be cleared with confirmation.
- **Collapsible vertical tabs:** the 244-DIP vertical rail can collapse to a 64-DIP favicon rail. Group markers, split-view state, active selection and workspace context remain visible, while titles/close buttons return when expanded. The preference persists per profile.
- **Session format v6:** adds the vertical-rail collapsed state while retaining backward compatibility with v1-v5. Older vertical-tab profiles migrate with the rail expanded.
- **MSVC hardening:** legacy multi-variable `auto` declarations in favicon/history/profile/UI paths were split into explicit declarations to avoid the type-deduction failure pattern seen in earlier Windows builds.

## 0.6.0 competitive productivity feature set

This milestone adds a lightweight productivity layer inspired by current Chrome, Edge, Firefox, Brave and Vivaldi workflows without creating an always-on sidebar renderer or extra hidden WebViews.

- **Persistent Workspaces:** up to 32 named, color-coded tab spaces per profile. Only tabs in the active workspace are shown; tabs can be moved between workspaces from their context menu. Workspace names, active workspace and tab membership survive restart in session format v5.
- **Vertical tabs:** switch between the classic horizontal strip and a 244-DIP native vertical tab rail. The selected tab remains in view, long titles get more room, groups/pinned tabs/audio/sleep state remain visible, and native pages/WebView content shift rather than being covered.
- **Split view:** show two existing website tabs side by side in one WebCom window. It reuses the existing WebView2 controllers, keeps both tabs awake while visible, and does not create a third hidden renderer. Switching workspaces or navigating the primary side to an internal WebCom page closes the split safely.
- **Quick Commands:** `Ctrl+Shift+P` opens a searchable native command palette for tabs, workspaces, split view, capture, history, downloads, bookmarks, profiles, settings, find, print, Developer Tools, full screen, memory reporting and data cleanup.
- **Visible-page capture:** `Ctrl+Shift+S` saves the currently rendered WebView viewport to PNG through WebView2 `CapturePreview`. This is intentionally described as a visible-page capture; full-page capture is on the continuation roadmap.
- **Workspace-aware navigation:** tab search can jump across workspaces, numeric tab shortcuts and Ctrl+Tab cycle within the current workspace, and tab reordering stays scoped to the visible workspace model.

Session format v5 remains plain UTF-8 metadata and does not persist cookies, passwords, form values or private tabs. The loader remains backward-compatible with state formats v1-v4.

## 0.5.2.1 cookie API compile hotfix

Password saving and general form autofill are now independent **per-profile** preferences rather than one global browser toggle. Existing session formats migrate safely: older profiles inherit the previous password-saving preference and default form autofill to enabled. New profiles inherit the current profile's credential/autofill choices. Incognito continues to force both features off.

The Passwords and autofill menu now exposes separate toggles plus **Clear saved passwords** and **Clear autofill form data** actions. Those actions use WebView2 profile browsing-data APIs so WebCom does not read or maintain a plaintext credential database. The site-information shield also captures certificate metadata when WebView2 reports a TLS verification failure and can show the requested address, verification error, subject, issuer and validity window. WebCom does not bypass or weaken certificate errors.

## 0.5.1 privacy, connection security and profile lifecycle

The omnibox site-info shield now leads with connection security. HTTPS pages show a secure connection state, HTTP pages are explicitly marked unencrypted, and certificate-related navigation failures are surfaced as certificate problems instead of looking like a normal secure page. The same panel now includes a Cookies and site data row with a per-site cookie count and a native details menu. Cookie values are intentionally never displayed; WebCom shows only cookie names, domain/path and Secure/HttpOnly flags, and can clear cookies matching the current site.

Profiles now have a real Delete current profile action in the profile menu and native Profiles page. WebCom refuses to delete the last profile or a profile with an active download, asks for destructive confirmation, then uses the WebView2 profile-deletion API before removing WebCom-owned history, bookmarks, permissions, appearance and saved-session metadata. Downloaded files remain on disk. Session primary and recovery generations are both sanitized so the deleted profile cannot be resurrected by recovery.

## 0.5.0 site controls and browser chrome maturity

HTTP(S) pages now expose a native shield button in the omnibox. Camera, Microphone, Location and Notifications can be set to Ask, Allow or Block per site and profile. WebCom stores the rules atomically, synchronizes changes to the WebView2 profile permission API, and keeps Incognito on Ask-each-time rather than inheriting normal-profile overrides. Clearing browsing data also clears WebCom's saved permission rules.

Profiles with downloads now show a compact recent-download button between the omnibox and profile control. It displays up to five recent items with state/progress and routes active items to the native Downloads page; completed items can open directly. The button is layout-aware and disappears on narrow windows before the omnibox is squeezed.

## 0.4.9 organization and event-driven downloads

History now supports filtered bulk deletion plus 7-day and older-than-30-day cleanup. Bookmark folders can be nested with `/`, nested hierarchy round-trips through Chromium/Netscape bookmark HTML, and folder trees can be renamed from bookmark context actions. Tab groups use a clearer colored chip and can be closed as a group. The Profiles page can open every saved profile or close every other profile window.

Live Downloads now subscribe to WebView2 byte/state change events when available; the old 250 ms polling loop is no longer the normal path. A one-second fallback timer is used only if event registration fails. Completion and unexpected interruption surface non-blocking WebCom notifications.

## 0.4.8 recovery and download controls

WebCom now maintains a last known-good session generation and falls back to it automatically
when the current session file cannot pass the normal safety/parser checks. Downloads gained
native Pause/Resume/Cancel controls, runtime resume support for resumable interruptions, and
a safe Retry path that reopens only validated HTTP(S) sources. Paused downloads remain
protected and visible instead of being mistaken for completed history.

## 0.4.7.4 MSVC compile hotfix

This hotfix addresses the concrete MSVC errors reported after the COM-header fix: a Windows `small` macro collision in favicon decoding, a duplicate native-page variable, ambiguous `LONG`/`int` `std::max` calls, and mixed-type `auto` declarator lists for temporary filesystem paths. It also cleans the remaining warning in the name dialog and the explicit-black appearance regression check.

## 0.4.7.3 Windows build hotfix

This hotfix fixes the MSVC/Windows SDK header failure reported after the WebView2 SDK restored successfully. WebCom's native targets previously defined `WIN32_LEAN_AND_MEAN`, which left newer MSVC/SDK combinations without the COM ABI declarations required by `WebView2.h` and GDI+. The native browser targets now use the full Win32 header surface and explicitly include COM ABI headers before WebView2/GDI+. The WebView2 SDK remains pinned to Microsoft.Web.WebView2 1.0.4314.35.

## 0.4.7.1 build hotfix

This hotfix addresses build failures reported against 0.4.7. The build now avoids the newly introduced `std::set::contains` dependency, improves WebView2 restore verification, and writes `build-last.log` beside `build.cmd`. If the optional native UI harness is the only failing target, run `build.cmd -SkipUiTests` to build/package the browser executable while isolating that harness for diagnosis.

## Build and try v2 on Windows

The current delivery is source-only. On a Windows 11 x64 development machine,
install Visual Studio Build Tools with **Desktop development with C++**, a Windows
SDK and CMake tools, then run:

```bat
build.cmd
run.cmd
```

Keep the Microsoft WebView2 Evergreen Runtime installed. The project pins its
WebView2 SDK build dependency in the documented build path; see `BUILDING.md`.
An optional Linux-to-Windows cross-build script is also included, but it requires
Zig 0.13.0 and the official WebView2 SDK to be supplied locally.

## What is implemented

| Area | Current implementation |
| --- | --- |
| Interface | Custom tabs and Windows caption, stable rounded address field, local autocomplete for tabs/bookmarks/history, website favicons, native menus/search, reusable paint buffers and a WebCom application icon |
| Appearance | Graphite, Charcoal, Midnight and Forest themes; automatic/preset/custom profile colors; independent bookmarks-bar visibility, Reader defaults, reduced motion and high contrast per profile |
| Tabs | New, close, duplicate, reopen, compact favicon-only pinned tabs, persistent named/color-marked tab groups, searchable tab picker, mouse/keyboard reordering, middle-click close and tab mute/unmute |
| Google | Default address-bar search, native normal new-tab page with Google search and website shortcuts, and an AI Mode website shortcut |
| Bookmarks | Per-profile root bar plus top-level folder buttons, persistent empty/nested folder objects, searchable Bookmark Manager, Ctrl+D save/remove, direct drag-to-folder/root organization, folder-tree create/rename/remove, and nested Netscape/Chromium-compatible HTML import/export |
| Profiles | Run multiple named profiles simultaneously with separate runtime data; safe display labels, preferences-only profile setup export/import, cross-profile tab moves, and supported permanent profile deletion while another profile remains |
| Extensions | Per-profile WebView2 unpacked-extension manager with saved source folders, source-change diagnostics, richer manifest capability details, Reload/Open Folder/Forget Source recovery actions, enable/disable and remove; Chrome toolbar/popup UI remains a WebView2 limitation |
| Private browsing | Native Incognito new-tab page; Ctrl+Shift+N or `--private` opens a private tab using the runtime's InPrivate mode |
| Session restore | Normal tab URLs, titles, selected tab, pin/keep-awake state and preferences are saved locally; a validated last-known-good backup is maintained for automatic recovery; background pages load only when selected |
| Memory | Balanced and Saver modes, best-effort background suspension, low-memory hints, manual sleep and explicit unload |
| Credentials | Per-profile WebView2 password-save and general-autofill preferences, separate clear-password/clear-autofill actions for normal profiles, and no app-owned plaintext password database |
| Capture | Visible-area, full-page and native drag-selection PNG capture paths; selection crop geometry is separately regression-tested |
| Downloads | Native per-profile Downloads page with event-driven byte/state progress, Pause/Resume/Cancel, resumable interruption handling, safe Retry, completion/interruption notifications, completed-file opening, plus a compact recent-download toolbar bubble |
| Site controls | Native omnibox shield with HTTPS/HTTP/certificate status, per-site cookie count/details/clear action, and per-profile Camera/Microphone/Location/Notifications Ask/Allow/Block rules; Incognito remains ask-each-time |
| Internal pages | Native searchable History and Bookmark Manager plus Downloads and Settings tabs use the buffered Win32 renderer instead of creating another WebView |
| Local files | Ctrl+O and Windows file activation for local-drive `.html`, `.htm` and `.pdf`; no UNC/network-file URLs and no broad file-to-file access switch |
| Page tools | Menu zoom controls, print preview, Find in page and developer tools through WebView2 APIs |
| Popups | User-initiated HTTP(S) popups become tabs in the originating profile/private context; unsolicited popups are blocked |
| Diagnostics | Menu memory snapshot and PowerShell process-tree measurement script |
| Recovery | Renderer failure page, shared-environment recovery on reload, runtime-install guidance, plus automatic session fallback from a validated last-known-good backup |

### WebCom icon

The approved ice-blue **W** on the charcoal tile is the single WebCom application
mark. The same artwork is embedded in `WebCom.exe` at multiple Windows icon sizes
and is used for the executable, taskbar, Alt+Tab and WebCom window icon surfaces.
The portable package also includes `WebCom.ico` and `WebCom-icon.png` for shortcuts
and future installer/package metadata. Website favicons and profile color badges stay
site/profile-specific by design.

## What changed in 0.4.8

Version 0.4.8 is the **recovery and download-control update**. Session persistence now
rotates a validated previous generation into `state.v1.bak` before replacing the primary
session file. If the primary session is malformed or otherwise fails the same safety checks
used during normal restore, WebCom automatically loads the backup and shows a non-blocking
recovery notice in the first profile window.

The native Downloads page now controls live WebView2 download operations: Pause/Resume,
Cancel, and resume of interrupted operations when WebView2 reports `CanResume`. WebView2
represents a paused download as an interrupted operation with `USER_PAUSED`; WebCom preserves
that operation and presents it as Paused instead of discarding it. Previous-run interruptions
that cannot be resumed in place can retry by opening only a validated HTTP(S) source in a new
tab. Paused operations survive Clear completed and continue to protect their source tab from
sleep/close/unload mistakes.

## What changed in 0.4.7

Version 0.4.7 is the **tab-group, history and library interaction update**. Tab groups now persist their own color and collapsed/expanded state in session format v3. A collapsed group occupies one representative slot; expanding it fans the hidden tabs back out with the same native 160 ms reflow language used by tab opening and closing. Group context actions include collapse/expand, rename and eight color choices. Older v1/v2 WebCom sessions remain readable.

History rows now have an in-page delete action in addition to search/date grouping, while private browsing still never enters the saved History list. The Bookmark Manager supports drag reordering within a folder, and moving a bookmark between folders preserves the user's manual ordering instead of silently alphabetizing the whole collection. Nested action hit testing was tightened so row-level open actions no longer steal hover/clicks from delete/remove controls.

The native Profiles page is the primary profile-management surface, showing each profile's identity color, current/running state and open/focus action while multiple profiles remain live in separate windows. Existing cross-window tab dragging uses screen coordinates, so the same move path works across monitors; the destination reloads the page in its isolated profile rather than transferring WebView session state.

## What changed in 0.4.6

Version 0.4.6 is the **tab organization and library-management update**. Pinned tabs now use a true compact favicon-only region on the left of the strip, while normal tabs retain the available width. Named tab groups persist with the saved session, keep same-group tabs together where pinning rules allow, and use a stable color marker plus the group name on the leading tab. Group assignment and removal live in the tab context menu.

History and Bookmark Manager now have native in-page filtering fields. Bookmark storage supports folders, the bookmarks bar renders folders as real folder buttons with dark popup menus, and bookmarks can be imported from or exported to standard Netscape/Chromium bookmark HTML. The storage reader remains backward compatible with older WebCom bookmark/session formats.

Tabs can be moved between simultaneously open profile windows from the tab context menu or by dragging onto another WebCom profile window. The destination keeps the URL, private-mode state, pinned/keep-awake metadata and group metadata, then reloads the page in that profile's isolated WebView2 context. Active downloads intentionally block the move. Omnibox suggestion hover states now use the same short native crossfade language as the rest of the chrome.

## What changed in 0.4.5

Version 0.4.5 is the **omnibox and bookmark-management update**. The address bar now
shows a custom native suggestion panel while you type. Suggestions are ranked from
open tabs, this profile's bookmarks and recent history, with a Google search action.
Choosing an open-tab result switches to that tab rather than navigating a duplicate.
The matching and ranking logic is local; WebCom does not send suggestion queries to
a separate autocomplete service.

**Ctrl+Shift+O** opens a first-class native Bookmark Manager. It lists saved titles
and URLs, opens a selected bookmark in the current tab, and removes bookmarks without
leaving the page. The Settings page links directly to the manager. History rows are
now grouped under local-date headings such as Today and Yesterday while retaining
per-profile storage and Incognito exclusion.

Pinned tabs now move to the left side of the tab order when pinned and return just
after the pinned set when unpinned. Omnibox suggestions use the same buffered native
painting strategy as the rest of the browser chrome; no hidden WebView is created.
Google remains the default search engine and the approved WebCom icon remains canonical.

## What changed in 0.4.4

Version 0.4.4 is the **native browser-pages and tab-motion update**. History,
Downloads and Settings now open as first-class WebCom tabs rendered by the same
buffered Win32 UI as the browser chrome, so these pages do not allocate an extra
web renderer. **Ctrl+H** opens full profile History, **Ctrl+J** opens Downloads,
and the main Settings command opens the native Settings page.

The Downloads page tracks current and completed downloads for the profile, shows
byte progress when the runtime exposes a total size, opens completed files, opens
the Windows Downloads folder, and can clear completed entries without cancelling
active transfers. History remains profile-local and excludes Incognito activity.

Opening and closing tabs now animate the remaining tab geometry with a 160 ms
cubic ease-out reflow rather than snapping immediately to the new positions. Menu
opening honors Windows' animation preference and uses a short native fade where
animations are enabled. Existing 100 ms hover crossfades and the 140 ms selected-tab
transition remain independent. No hidden WebView is pre-created for these effects.

The native tooltip-popup path stays disabled to prevent the yellow/top-left flash
reported during rapid tab/layout changes. The approved WebCom square icon remains
the canonical application mark, and Google remains the default search engine.

## The redesigned interface

At 100% scaling, the top regions are **40 pixels for tabs/caption, 44 pixels for
navigation, and an optional 37 pixels for bookmarks**. Content begins at pixel
121 with the bookmarks bar shown, or 84 with it hidden. Those dimensions scale
with Windows DPI. The active tab has curved shoulders; the address field uses
the profile's readable accent for focus. There is no permanent bottom status
bar; short notifications appear temporarily.

Open **Menu → Settings**, then choose **Appearance** on the native Settings page for **Graphite**, **Charcoal**, **Midnight** or **Forest**. Graphite is the default; Charcoal is the closest
palette to the supplied reference. **Profile color** offers Automatic, Blue,
Green, Yellow, Rose, Lavender, Cyan, Peach, Silver, and **Custom color…**. Enter
six hexadecimal digits, with or without `#`, such as `#8BD4BF`. The profile
badge uses that exact color; focus indicators may use a lighter related shade
to stay visible on dark backgrounds. **Reset appearance** restores Graphite,
the automatic profile color and a visible bookmarks bar for this profile.

The normal new-tab page provides a Google search field and Google, YouTube and
GitHub shortcuts. **AI Mode** opens Google's AI Mode website; its availability
and behavior are controlled by Google. WebCom is not affiliated with Google.

Open **Menu → Settings** and use the **Performance** card to switch between Balanced and Memory Saver. The Settings page also exposes password-save prompts, bookmarks-bar visibility, profile color and browsing-data controls. **Menu → Performance** or **Shift+Esc** opens the memory snapshot.
Zoom controls apply to loaded web pages; the native new-tab pages retain their
Windows DPI layout. Short Incognito windows use compact spacing, with mouse-wheel
or Page Up/Page Down scrolling when necessary. Print and the menu's Find action depend on the installed
runtime exposing their APIs. Ctrl+F uses that Find API from the native toolbar
and is passed through to the runtime when a web page has focus.

The layout follows the supplied reference; pixel-for-pixel rendering across
Windows versions and DPI settings has not yet been verified.

## Bookmarks and Incognito

The initial bookmarks are **Google, YouTube and GitHub**. The reference image's
personal bookmark destinations were not supplied, so WebCom uses these known
public addresses. Each normal profile has its own saved list, up to 100 entries.
An intentionally empty list remains empty after restart.

Press **Ctrl+D** to save the current page, or remove it if already bookmarked.
The star in a web page's address bar has the same action. Right-click a
bookmark to open or remove it; **Menu → Bookmarks** includes entries that do not
fit on the bar. **Ctrl+Shift+B** toggles the bar without deleting bookmarks.
Bookmark titles and HTTP(S) URLs are stored locally.

Press **Ctrl+Shift+N** to open the native Incognito page. The command line also
accepts:

```bat
WebCom.exe --private
WebCom.exe --private --url https://example.com
```

Private tab URLs and titles are excluded from restored and recently closed
tabs. Bookmarks deliberately saved from Incognito and downloaded files remain
on the device. Incognito does not hide activity from websites or the network.
The private page describes those limits without claiming a cookie-blocking
setting that WebCom has not configured. **Ctrl+T creates a normal tab.**

## Memory behavior

| Mode or condition | Behavior |
| --- | --- |
| Fresh native new tab | No WebView2 controller is created for that tab |
| Restored background tab | Metadata only until first selected |
| Balanced | Eligible background tabs become sleep candidates after 5 minutes |
| Memory Saver | Eligible background tabs become sleep candidates after 1 minute; inactive pages also receive lower memory-target hints |
| Playing audio, loading, downloading, pinned or keep-awake | Excluded from automatic sleep |
| Selecting a sleeping tab | Resumes the page and restores its normal memory target |
| Explicit **Unload tab** | Closes its controller after confirmation; the page reloads on selection and unsaved page state is lost |

The timer checks eligibility every 10 seconds. Suspension is best effort and
may be declined by a busy runtime. The UI marks a tab sleeping only after a
successful callback and a confirmed suspended state. Automatic sleep does not
destroy the page controller. Use **Keep this tab awake** for calls, live
dashboards, silent media, background processing or other pages that must keep
running; this version cannot reliably detect every background workload.

C++ gives the interface direct control over object lifetime, but web rendering
and JavaScript still run in WebView2's browser processes. One shared environment
is used across all tabs, named profiles and their windows. This follows Microsoft's guidance
for avoiding redundant process groups, but **no specific RAM reduction has
been established**. See Microsoft's [performance guidance](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/performance)
and [multiple-profile documentation](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/multi-profile-support).

## Profiles, passwords and Google accounts

Click the profile name on the toolbar, or press **Ctrl+N**, to open the profile
picker. Selecting another profile opens its own window while the current
profile's tabs remain running. Selecting an already-open profile focuses that
window. Version 0.4 supports **one window per named profile**; closing one window
leaves the others running, and closing the last window exits WebCom. Names are
1–40 ASCII letters, digits, spaces, underscores or hyphens and match without
regard to case.

The command line can select or create a profile:

```bat
WebCom.exe --profile Work
WebCom.exe --profile Personal https://example.com
WebCom.exe --profile Work --private
```

A valid new name creates a local profile. Launching into an existing window
opens the requested address, or a new tab when no address is given. All windows
share one process-level session owner, so saving from one window also captures
the other open profiles. Existing 0.2 and 0.3 session and bookmark files remain
compatible. New appearance preferences use a separate file. On restart, WebCom
opens the last-focused profile; open the other saved profiles from the picker
when needed.

The app's data directory is:

```text
%LOCALAPPDATA%\WebCom
```

`state.v1` contains normal tab metadata and preferences. `Bookmarks` contains
the separate per-profile bookmark files. `Appearance` contains each profile's
theme, color and bookmarks-bar visibility. `User Data` is the runtime-managed
browsing store. Private tabs are omitted from `state.v1`.
Private tabs belonging to the same named profile share its InPrivate session;
private browsing is not network anonymity, and downloaded files remain.

Password-saving is enabled for normal profiles through the WebView2 API. The
runtime controls prompts, storage and filling. WebCom does not read, export,
intercept or implement its own encryption for password values. Disabling the **Passwords and autofill** setting on the native Settings page stops new save prompts; passwords
already stored may still autofill. **Delete browsing data** requests the
runtime's all-data deletion operation, including saved passwords. Read the
[official autofill API behavior](https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2settings4).

**Chrome Sync is not included, and Google sign-in may be blocked by WebView2's
embedded-browser restrictions.** Local profiles and default-browser handoff do
not transfer another browser's account session into WebCom. See [WebView2 browser
differences](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/browser-features)
and [Chromium sign-in restrictions](https://www.chromium.org/developers/how-tos/api-keys/).

## Closing and session restore

The window close button, **Alt+F4**, **Menu → Close this window**, and the window-close command
close that profile window directly. There is no multi-tab, private-tab,
download or failed-save confirmation on this path. WebCom first attempts to
save normal tab metadata across the open profile windows. A rejected file
replacement preserves the previous saved session, but changes since the last
successful save may be lost. The close still completes.

Closing a window ends its private tabs and active downloads and discards
unsaved page contents. Normal tab addresses and titles can be restored; form
drafts, scroll positions, full navigation history and private sessions cannot.
Other profile windows keep running; closing the final window exits the app.
Individual tab actions retain their existing download and explicit-unload
prompts. Save work on its website before closing when it needs to survive.

## Keyboard shortcuts

| Shortcut | Action |
| --- | --- |
| Ctrl+T | New normal tab |
| Ctrl+N | Open the profile picker to create, open or focus a profile window |
| Ctrl+Shift+N | New private tab |
| Ctrl+W / Ctrl+F4 | Close current tab |
| Ctrl+Shift+T | Reopen last closed normal tab, within the current session/profile |
| Ctrl+L / Alt+D / F6 | Focus address bar |
| Ctrl+Tab / Ctrl+Shift+Tab | Next / previous tab |
| Ctrl+Shift+A | Search the current profile's open tabs by title or address |
| Ctrl+Shift+PgUp / Ctrl+Shift+PgDn | Move the current tab left / right |
| Ctrl+1…8 / Ctrl+9 | Select a numbered tab / last tab |
| Alt+Left / Alt+Right | Back / forward |
| Ctrl+R / F5 | Reload, or stop a currently loading page |
| Ctrl+J | Open the native Downloads tab |
| Ctrl+D | Save/remove the current page's bookmark |
| Ctrl+Shift+B | Show/hide this profile's bookmarks bar |
| Ctrl++ / Ctrl+- / Ctrl+0 | Increase/decrease/reset web-page zoom |
| Ctrl+P | Runtime print preview |
| Ctrl+F | Runtime Find in page; native-toolbar focus uses the Find API |
| Ctrl+Shift+Delete | Request browsing-data deletion for the current normal runtime profile |
| Alt+F / Alt+E | Main menu |
| Shift+Esc | Memory snapshot |
| F11 | Toggle full screen |
| F12 | Developer tools for the current web page |
| Alt+F4 | Close this profile window without a confirmation |

Native-toolbar Tab navigation is kept separate from website form navigation.
The menu also exposes page zoom, Find, Print and developer tools directly.

## Check RAM usage

In WebCom, press **Shift+Esc** or choose **Menu → Performance**. The snapshot
includes `WebCom.exe` and the runtime's reported process group. The runtime's
crash reporter is not included in that API list. Read failures are reported as
incomplete snapshots.

For repeated samples, open PowerShell in the extracted portable folder:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Measure-WebComMemory.ps1 -Watch
```

The script filters the process tree to WebCom and its `msedgewebview2.exe`
descendants. It does not count every WebView2 process on the computer. The
process-only execution-policy argument does not modify the machine policy.

Record both working set and private commit. Working set is resident memory;
summed values can double-count shared pages. Private commit is private
allocated memory and can include pages that are not currently resident.
Microsoft defines the counters in [PROCESS_MEMORY_COUNTERS_EX](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters_ex).

Compare the same pages, activity and idle durations with Chrome's full process
group. A tiny native host process alone does not prove a low-memory browser.

## Build and project files

The complete instructions are in `BUILDING.md`. With the Visual Studio C++
workload, Windows SDK and CMake tools installed:

```bat
build.cmd
run.cmd
```

The project includes the native app, eight portable test suites for core,
storage, bookmarks, launch requests, tab search, appearance, omnibox ranking and chrome layout,
a native Windows UI regression executable, checked PowerShell SDK restore, memory
measurement, an optional Linux cross-build script and a Windows GitHub Actions
workflow. The portable tests were executed for 0.4.7; the Windows app and native
UI harness were not rebuilt in this environment because the required Windows
cross-build toolchain is unavailable. On Windows, **Test-UI.cmd** runs that harness with temporary profile data
and native tabs, without navigating websites or using your normal WebCom data.
It does not validate WebView2 rendering. See `BUILDING.md` and
`docs/VALIDATION.md` for the exact boundary. No repository has been published
or workflow run on the user's behalf.

## Current limits and next work

- Native Windows runtime testing, accessibility and high-DPI visual QA remain.
- RAM usage has not been benchmarked against Chrome or the earlier Linux build.
- Nested bookmark folders, direct Chrome-profile import, Linux-profile migration,
  extensions UI, Chrome Sync, a password-list editor and a signed installer remain
  future work. History supports search, date grouping and per-entry deletion; date/range
  bulk deletion can be added later.
- Each named profile has one live window; duplicate windows for the same profile and
  profile rename/delete remain future work. The native profile manager, local omnibox
  autocomplete, cross-window/cross-monitor tab moves and compact pinned tabs are implemented.
- HTTP(S) browsing is supported. Local files, executable URL schemes and
  external protocol handlers are not exposed by this preview.
- Session restoration preserves URLs and titles, not forms, scroll positions,
  complete navigation history or private sessions. Tab and window closure can
  discard unsaved page contents; window closure has no confirmation.
- The engine follows the installed Evergreen Runtime. The WebCom shell itself
  has no automatic updater in this preview.

The next development gate is Windows acceptance testing and a reproducible RAM
comparison. `docs/VALIDATION.md` gives the exact checks; `docs/ARCHITECTURE.md`
explains the implementation and `docs/ROADMAP.md` gives the continuation order.
