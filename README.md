<p align="center">
  <img src="resources/webcom-icon.png" width="112" alt="WebCom logo">
</p>

<h1 align="center">WebCom v3</h1>

<p align="center">
  <strong>A native Windows browser built for a familiar experience, strong profile separation, useful power features, and disciplined resource use.</strong>
</p>

<p align="center">
  <img alt="Version" src="https://img.shields.io/badge/version-v3%20%7C%203.0.0-2f81f7">
  <img alt="Platform" src="https://img.shields.io/badge/platform-Windows%2010%2F11-0078d4">
  <img alt="Language" src="https://img.shields.io/badge/C%2B%2B-20-00599c">
  <img alt="Renderer" src="https://img.shields.io/badge/renderer-WebView2-0f9d58">
  <img alt="Status" src="https://img.shields.io/badge/status-public%20beta-f0a500">
</p>

---

## What is WebCom?

WebCom is a Windows web browser written in **native C++20 / Win32** with **Microsoft Edge WebView2** for web rendering. The browser shell—tabs, toolbar, menus, bookmarks bar, profile controls, native pages, and supporting UX—is built by WebCom.

The project is designed for people who want a familiar Chromium-style browsing experience with local profile separation, practical productivity tools, visible privacy/security controls, and measurable performance budgets.

## Our goal

WebCom aims to make everyday browsing feel **fast, familiar, transparent, and under the user's control** without turning the browser shell into another heavy web application.

We focus on:

- **Familiar UX** — straightforward navigation and Chrome-like interaction patterns.
- **Native Windows feel** — Win32 UI where it improves responsiveness and integration.
- **Profile isolation** — separate browsing contexts for work, personal use, testing, and more.
- **Resource discipline** — avoid unnecessary hidden web processes and measure memory regressions.
- **Security clarity** — visible connection state, site permissions, signed update verification, and fail-closed recovery paths.
- **Useful power tools** — workspaces, vertical tabs, split view, Reader mode, capture tools, sessions, recovery, and extension diagnostics.

## Features

- Chrome-like native UI with destination-first WebView tab handoffs, buffered omnibox suggestion reveals, hidden-until-laid-out startup controls, effective profile/Windows/Remote-Desktop reduced-motion policy, live Windows High Contrast refresh, and per-monitor DPI-aware native chrome
- Multiple profiles
- Workspaces with live workspace cards directly on New Tab for one-click switching and creation
- Vertical or horizontal tabs
- Tab search
- History and bookmarks
- New Tab dashboard with native quick actions for History, Downloads, Bookmarks, and Saved Sessions plus deduplicated recent-site cards
- Dedicated native **Passwords & Autofill** manager for profile-scoped save-password/autofill toggles and confirmation-gated clearing, with an explicit WebView2 credential-store privacy boundary
- Cookie controls
- Searchable **Privacy Manager** with live cookie-domain/cookie totals and permission-origin/rule totals, plus honest Ad Blocker on/off and paused-site state, while keeping per-domain Clear and per-origin Reset actions
- Site permissions
- SSL/security details
- **Extensions v2** with native search, All/Enabled/Disabled/Needs-attention filters, live counts, unpacked-source health, enable/disable/reload/remove controls, and an explicit WebView2 compatibility boundary
- Extension diagnostics
- Reader mode
- Native failed-page recovery surface with **Try again / Edit address / New Tab** controls that stay browser-owned even when a site fails to load
- First-run **setup checklist** with live progress for Default browser, Bookmarks, Workspaces, and Update checks; all actions remain explicit and Windows keeps control of default-app selection
- Split view
- Page capture and annotation
- Saved sessions
- Persistent **Recently Closed** recovery across restarts (up to 20 entries) with workspace/group/pinned/keep-awake context; private/ephemeral tabs are never persisted
- Crash recovery
- Actionable native notifications for completed downloads and available updates; download completion can open the finished file and update availability opens the Updates page
- Privacy-safe native **Report a Problem** page with a copyable report template, diagnostics/recovery export actions, data-folder access, and an explicit no-automatic-upload boundary
- Safe update checks
- Verified rollback support
- Stable/Beta channels
- Accessibility support with custom tab-strip MSAA semantics plus a native-client MSAA bridge for painted browser-page actions: visible actions expose push-button roles, accessible names, screen bounds, hit testing, focus events and default activation while WebCom preserves the standard Win32 child-control tree; keyboard focus rings, live Windows High Contrast composition, and effective Windows/profile reduced motion remain supported
- Memory regression checks with a native **Performance snapshot** that separates WebCom host vs WebView2 process-group memory and a repeatable Windows scenario script that can emit CSV/JSON artifacts
- Native **About WebCom** page with version, WebView2 Runtime, release channel, default-browser state, active profile, sleep policy, and direct links to updates/default apps/performance/diagnostics/data folder
- Downloads v2: original-source actions, bounded **Retry failed**, and automatic cleanup of completed download-history entries older than 30 days without deleting downloaded files
- Per-profile automatic tab-sleep timing: **1 / 5 / 15 / 30 minutes / Never**, plus a bounded exact-host **Never sleep these sites** list; both live in the separate performance preference file so older session/state files remain compatible
- Adaptive performance polling follows the selected sleep policy: **10s / 30s / 60s / 60s / off** for 1m / 5m / 15m / 30m / Never, reducing idle timer wakeups when aggressive sleeping is not requested
- Cold-start I/O keeps persisted **Downloads, Site Permissions, Saved Sessions, and unpacked-extension source metadata/scans** off the ordinary browser constructor path; each bounded store loads before its first read/write surface so existing data cannot be overwritten by an empty lazy state

## Quick start

### Build from source

1. Use **Windows 10 or Windows 11 x64**.
2. Install **Visual Studio 2022 Build Tools** with **Desktop development with C++**, a Windows SDK, CMake, and Ninja.
3. Clone or extract the repository into a normal writable folder.
4. Run:

```bat
build.cmd
```

5. After the build succeeds, run:

```bat
run.cmd
```

The packaged browser is created at:

```text
dist\WebCom\WebCom.exe
```

For complete setup, build options, clean-build instructions, and troubleshooting, see **[HOW_TO_BUILD.md](HOW_TO_BUILD.md)**.

## Repository map

| Path | Purpose |
| --- | --- |
| `src/` | Browser shell, Win32 UI, WebView2 integration, feature models |
| `tests/` | Portable model tests and native Windows UI regression tests |
| `tools/` | Source preflight checks for known MSVC/WebView2 failure classes |
| `scripts/` | Build, SDK restore, memory measurement, release/update tooling |
| `resources/` | Icons and Windows resources |
| `docs/` | Architecture, features, validation, roadmap, release notes |
| `.github/workflows/` | Windows and portable CI builds |

## Build philosophy

The Windows build runs a dedicated **MSVC source preflight before the main browser compile**. This catches known Windows-only failure patterns early, including COM/WebView2 interface mistakes, Win32 type ambiguity, and command identifiers that can collide with member-function names.

Portable C++ models are also tested independently with non-Windows compilers. Native Win32/WebView2 behavior still requires a real Windows acceptance run before a public binary is considered release-ready.

Latest-43 makes the native MSVC browser and Windows UI harness **warnings-as-errors by default** (`/W4 /WX`). Third-party/system angle-bracket headers are treated as external at warning level 0 so the release policy targets WebCom-owned code. `build.cmd -AllowCompilerWarnings` exists only for diagnosis and is explicitly not release-signoff safe; the Windows acceptance runner still scans repository-source warning lines after the build.

Latest-44 adds a strict **build-package integrity gate** before portable ZIP creation and before/after executable signing. It verifies PE architecture, Windows version metadata, bundled blocker assets, source-manifest freshness, absence of compiler/debug artifacts, release trust inputs, and signed executable identity. Release Candidate fresh-install QA then proves the installer contains that exact signed `WebCom.exe`.

For release-candidate Windows builds, `scripts/Test-WindowsAcceptance.ps1` performs the clean MSVC/runtime gate. Latest-39 adds `scripts/Test-ReleaseCandidate.ps1`, which combines that result with signed release-matrix verification, signed fresh-install/runtime/uninstall QA, the default 300 MiB one-tab memory target, and optional previous-to-current installer upgrade/profile-compatibility testing. Signed artifacts are not considered approved merely because they were generated.

## Browser engine and extension scope

WebCom uses the **Microsoft Edge WebView2 Runtime** to render web content. WebCom is not a Chromium fork and does not claim full Chrome browser-API compatibility.

Unpacked extensions are supported through WebView2's extension APIs where the runtime supports their behavior. Direct Chrome Web Store installation, Chrome Sync, and browser-owned Chrome toolbar/popup UI are not emulated.

### WebCom Ad Blocker

Ad blocking turns on automatically when a normal profile opens its first web page. WebCom keeps the bundled files in its writable application-data folder. Open **Extensions → WebCom Ad Blocker → Turn off / Turn on** to change the global setting for that profile. The choice survives restarts and bundled blocker updates. Incognito uses its separate WebView2 context and currently has no bundled blocker.

**Ad Blocker v2 adds real per-site controls.** Open the site-information shield on an HTTP(S) page and choose **Pause ad blocking on this site** / **Resume ad blocking on this site**. WebCom stores up to 64 validated host exceptions per profile, regenerates the bundled extension's DNR initiator exclusions plus the matching cosmetic/YouTube bypass config, reloads the bundled extension, and reloads the active page. Extensions shows the saved exception count and provides **Clear sites**. Profile deletion removes the exception store.

The extension blocks requests to common ad networks and hides common ad slots. On YouTube it also filters player ad data and presses the player's skip button when one becomes available. YouTube playback and coverage can vary as the site changes. A paused site bypasses network, cosmetic, and YouTube-helper filtering from the bundled blocker. WebCom still does not claim Chrome Web Store compatibility or browser-owned Chrome toolbar/popup support.

## Versioning

- **WebCom v1** represents the early development line through the v0.5-era builds.
- **WebCom v2** represents the v0.5-to-v0.8 development line.
- **WebCom v3** is the current product generation. Public packages and documentation use **v3**; machine-readable semantic versioning uses **3.0.0**.

Internal persistence/update schema numbers are intentionally independent from the product generation so older profiles and signed update metadata remain compatible.

## Project status

**WebCom v3 is currently a public-beta codebase in Release Candidate hardening.** Latest-39 provides the single Windows gate for the exact signed build; Latest-40 hardens Windows accessibility/DPI behavior; Latest-41 adds MSAA semantics for painted native-page actions, and Latest-42 hardens the exact MSVC compile-regression classes found in the older real Windows build logs, Latest-43 enforces warning-clean native builds, and Latest-44 binds clean source/package/signer/installer executable identity before release approval. The codebase should not be labeled release-ready until the gate actually passes on Windows, including multi-DPI, High Contrast, reduced-motion, keyboard, and assistive-technology acceptance.

## Documentation

- **[Build and run WebCom](HOW_TO_BUILD.md)**
- **[Architecture](docs/ARCHITECTURE.md)**
- **[Features](docs/FEATURES.md)**
- **[Release notes](docs/RELEASE-NOTES.md)**
- **[Roadmap](docs/ROADMAP.md)**
- **[Validation](docs/VALIDATION.md)**
- **[Windows memory acceptance](docs/MEMORY-ACCEPTANCE.md)**
- **[Windows release acceptance](docs/WINDOWS-ACCEPTANCE.md)**
- **[MSVC build regression notes](docs/MSVC-BUILD-REGRESSION.md)**
- **[Release Candidate gate](docs/RELEASE-CANDIDATE.md)**
- **[Build-package integrity](docs/BUILD-PACKAGE-INTEGRITY.md)**
- **[Public beta checklist](docs/PUBLIC-BETA-CHECKLIST.md)**

## Contributing

Build WebCom locally, keep changes focused, run the available tests, and include the first meaningful compiler/linker error from `build-last.log` when reporting Windows build failures.

---

<p align="center"><strong>WebCom v3 — native browsing, practical controls, measurable performance.</strong></p>

