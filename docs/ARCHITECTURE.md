# WebCom v3 architecture

## Selected stack

- **Language:** C++20.
- **Desktop shell:** native Win32.
- **Web renderer:** Microsoft Edge WebView2.
- **Build system:** CMake + Ninja through the Windows build wrapper.
- **Primary Windows compiler:** MSVC.

WebCom keeps the browser shell native while delegating website rendering and Chromium network behavior to WebView2.

## Browser shell

The native application owns the browser chrome and supporting UX, including:

- tab strip and tab search;
- toolbar and omnibox;
- bookmarks bar and native menus;
- profile and workspace controls;
- history, bookmarks, downloads, settings, and other internal views;
- capture/annotation and Reader controls;
- update, recovery, diagnostics, and accessibility surfaces.

Website content is hosted in WebView2 controllers inside WebCom windows.

## Profiles and workspaces

WebCom uses separate WebView2 profiles to isolate browsing contexts. WebCom-owned metadata is also stored per profile where appropriate, including preferences, history, bookmarks, permissions, sessions, and extension-source metadata.

Workspaces organize tabs inside a profile without creating a second browser identity. Multiple named profiles can run at the same time.

## Local state and privacy

WebCom stores its own state under the user's local application-data area. Browser state is written using bounded, validated formats and recovery generations where needed.

Private browsing is excluded from normal saved history/session behavior. Diagnostic/export features are designed to avoid collecting credentials and other secret values.

## Web/native boundary

WebCom does not reimplement a web engine. Navigation, website rendering, cookies, certificates, and supported extension execution are provided through WebView2 APIs.

Native WebCom pages and tools are used where a browser-owned surface does not need another web renderer. This reduces unnecessary renderer creation for browser chrome.

## Extensions

Extensions v2 view state (search, Enabled/Disabled/Needs-attention classification, and counts) is computed by a portable model. Source-health scans remain bounded and are refreshed only when the Extensions surface or an explicit extension-source action needs them.

WebCom enables WebView2 browser-extension support and manages unpacked extensions per profile through supported WebView2 interfaces. Source metadata and change diagnostics are maintained by WebCom, while WebView2 remains the authority for extension validation and runtime support.

Chrome Web Store installation, Chrome Sync, and browser-owned Chrome toolbar/popup UI are outside the current scope.

The bundled **WebCom Ad Blocker v2** remains an unpacked MV3 extension managed through WebView2. Per-site exceptions are WebCom-owned profile metadata (maximum 64 validated DNS hosts). Before registering/reloading the bundled extension, WebCom generates profile-specific DNR `excludedInitiatorDomains` and a synchronous MAIN-world site config used by cosmetic selectors and the YouTube helper. This avoids presenting a UI-only exception that would leave network blocking active. The bundled blocker is not installed in the separate Incognito WebView2 profile.

## Updates and recovery

Release manifests are validated against channel policy, package metadata, cryptographic hashes, Authenticode trust, and the configured publisher identity before a package is considered eligible.

Rollback remains explicit and is gated by recovery-readiness checks. Recovery packages are re-verified before launch.

## Accessibility

The custom tab strip exposes native accessibility semantics for tab roles, names, selected/focused state, bounds, hit testing, keyboard shortcuts, and selection actions. Latest-41 also wraps the standard top-level Win32 client `IAccessible` object and appends virtual children for WebCom's painted native-page actions. Those virtual children expose push-button roles, meaningful action names, screen bounds, hit testing, focus state/events, and default activation while the standard edit/button HWND accessibility tree remains delegated to Windows. The child-ID arithmetic/navigation is shared with a portable tested model. Real Windows Narrator/UI Automation/MSAA testing remains part of release acceptance.

## Testing and preflight

The repository uses three layers of validation:

1. portable C++ model tests;
2. a Windows/MSVC source-preflight gate for known compile/API failure patterns;
3. native Windows/WebView2 acceptance testing.

Portable tests and source preflight reduce regressions but do not replace real Windows runtime testing.

## Module map

- `src/` — browser shell, Win32/WebView2 integration, feature models.
- `tests/` — portable and native regression tests.
- `tools/` — source preflight checks.
- `scripts/` — build, SDK restore, release, update, and performance tooling.
- `resources/` — application icons and Windows resources.
- `docs/` — current project documentation.

Performance preferences are profile-scoped and versioned separately from session/state data. The same bounded file carries the automatic sleep timeout and up to 64 normalized exact-host Never-sleep exceptions; automatic sleep consults those exceptions, while manual suspension remains explicit and available.
The native performance timer is rescheduled from the portable sleep policy: 10 seconds for 1 minute, 30 seconds for 5 minutes, 60 seconds for 15/30 minutes, and disabled for Never. This reduces idle wakeups without changing manual sleep behavior.

Cold-start profile I/O is split by feature. Downloads history, Site Permissions, Saved Sessions, and unpacked-extension source metadata/source-health scans are lazy. Mutation paths explicitly load the existing bounded store before writing so deferred I/O cannot erase prior records. History remains available for New Tab recent sites and omnibox suggestions.

## Runtime observability

The browser-owned About/Diagnostics/Recovery surfaces stay native. Shift+Esc collects a point-in-time Windows process-group memory snapshot and formats it through the portable `memory_snapshot_model`; the host process and WebView2 process group are reported separately and incomplete runtime-process enumeration is called out explicitly. Release acceptance uses `scripts/Measure-WebComMemory.ps1` for repeated scenario-labelled samples rather than making a claim from one live snapshot.
