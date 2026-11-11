# WebCom 0.7.9 changes

## Extension source health
- Successful unpacked extension loads/reloads now store a lightweight per-profile source change stamp.
- Saved sources are classified as unchanged, changed/reload recommended, baseline pending, or scan unavailable.
- The snapshot is bounded to 4,096 regular files and uses path/size/mtime metadata; it is a developer convenience, not a security hash.
- Existing extension-source metadata schema v1 remains readable and upgrades to schema v2 on save while keeping the existing per-profile `.v1` storage filename for path compatibility.

## Richer manifest diagnostics
- Adds minimum Chromium version reporting.
- Adds optional permissions and optional host permissions.
- Adds commands, omnibox, DevTools page, web-accessible resources and side-panel declaration reporting.
- Retains background/service-worker, content-script, options-page and toolbar/popup compatibility reporting.

## Known-source recovery controls
- Saved sources that are not currently reported installed now have Details, Reload source, Open folder and Forget source actions.
- Details includes bounded manifest preview plus source file count, byte count and change/baseline status.
- Forget source is deliberately non-destructive: it removes only WebCom's saved mapping and never deletes the extension source folder or removes an installed extension.

## Validation growth
- Extension-source tests increase to 27 checks.
- Extension-manifest tests increase to 25 checks.
- Portable behavioral checks increase to 926.
- MSVC/WebView2 source-preflight checks increase to 150.
- Explicit validation total is 1,076 checks, with GCC and Clang each passing 29/29 CTest targets in the portable host.
