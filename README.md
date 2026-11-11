<p align="center">
  <img src="resources/webcom-icon.png" width="112" alt="WebCom logo">
</p>

<h1 align="center">WebCom v2</h1>

<p align="center">
  <strong>A native Windows browser built for a familiar experience, strong profile separation, useful power features, and disciplined resource use.</strong>
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

- Chrome-like native UI
- Multiple profiles
- Workspaces
- Vertical or horizontal tabs
- Tab search
- History and bookmarks
- Password saving and autofill controls
- Cookie controls
- Site permissions
- SSL/security details
- Unpacked extensions
- Extension diagnostics
- Reader mode
- Split view
- Page capture and annotation
- Saved sessions
- Crash recovery
- Safe update checks
- Verified rollback support
- Stable/Beta channels
- Accessibility support
- Memory regression checks

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

## Browser engine and extension scope

WebCom uses the **Microsoft Edge WebView2 Runtime** to render web content. WebCom is not a Chromium fork and does not claim full Chrome browser-API compatibility.

Unpacked extensions are supported through WebView2's extension APIs where the runtime supports their behavior. Direct Chrome Web Store installation, Chrome Sync, and browser-owned Chrome toolbar/popup UI are not emulated.

### WebCom Ad Blocker

Ad blocking turns on automatically when a normal profile opens its first web page. WebCom keeps the bundled files in its writable application-data folder. Open **Extensions → WebCom Ad Blocker → Turn off / Turn on** to change the setting for that profile, then reload open pages. The choice survives restarts and bundled blocker updates. Incognito uses its separate WebView2 context and currently has no bundled blocker.

The extension blocks requests to common ad networks and hides common ad slots. On YouTube it also filters player ad data and presses the player's skip button when one becomes available. YouTube playback and coverage can vary as the site changes. The Extensions page lets you disable the blocker if a site breaks.

## Project status

**WebCom v2 is currently a public-beta codebase.** Before broad distribution, validate the browser on real Windows hardware for navigation, profile isolation, accessibility, signed update/rollback behavior, downloads, permissions, password/autofill UX, DPI scaling, extension behavior, and memory use.

## Documentation

- **[Build and run WebCom](HOW_TO_BUILD.md)**
- **[Architecture](docs/ARCHITECTURE.md)**
- **[Features](docs/FEATURES.md)**
- **[Release notes](docs/RELEASE-NOTES.md)**
- **[Roadmap](docs/ROADMAP.md)**
- **[Validation](docs/VALIDATION.md)**
- **[Public beta checklist](docs/PUBLIC-BETA-CHECKLIST.md)**

## Contributing

Build WebCom locally, keep changes focused, run the available tests, and include the first meaningful compiler/linker error from `build-last.log` when reporting Windows build failures.

---

<p align="center"><strong>WebCom v2 — native browsing, practical controls, measurable performance.</strong></p>
