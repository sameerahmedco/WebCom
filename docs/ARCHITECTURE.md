# WebCom v2 architecture

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

WebCom enables WebView2 browser-extension support and manages unpacked extensions per profile through supported WebView2 interfaces. Source metadata and change diagnostics are maintained by WebCom, while WebView2 remains the authority for extension validation and runtime support.

Chrome Web Store installation, Chrome Sync, and browser-owned Chrome toolbar/popup UI are outside the current scope.

## Updates and recovery

Release manifests are validated against channel policy, package metadata, cryptographic hashes, Authenticode trust, and the configured publisher identity before a package is considered eligible.

Rollback remains explicit and is gated by recovery-readiness checks. Recovery packages are re-verified before launch.

## Accessibility

The custom tab strip exposes native accessibility semantics for tab roles, names, selected/focused state, bounds, hit testing, keyboard shortcuts, and selection actions. Real Windows Narrator/UI Automation testing remains part of release acceptance.

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
