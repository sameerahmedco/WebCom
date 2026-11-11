# WebCom 0.6.9 changes

## Screenshot annotation
- Capture menu adds Annotate visible area and Annotate selected area.
- Native modal editor uses standard Win32 buttons for Pen, Rectangle, Undo, Copy, Save PNG and Done.
- No extra WebView renderer is created.
- Selected-area annotation reuses the tested selection geometry/cropping path.
- Removes the duplicate `auto* tab = Active();` declaration inherited in the v0.6.8 capture menu.

## Privacy-safe support bundle
- Support Diagnostics adds Export support ZIP.
- ZIP writer is portable, bounded and store-only; filenames cannot contain traversal or nested paths.
- Bundle contains diagnostics, update status and a privacy note only.
- It deliberately excludes browsing URLs, cookie/password values, form contents and authentication data.

## Signed-update verification groundwork
- `WEBCOM_RELEASE v2` adds package size, Authenticode requirement and pinned publisher certificate SHA-256.
- Legacy v1 manifests remain parseable but are never install-eligible.
- `New-ReleaseManifest.ps1` generates v2 metadata only from a package with a valid Authenticode signature.
- `Verify-UpdatePackage.ps1` checks size, SHA-256, Authenticode trust and publisher certificate fingerprint.
- Automatic download/install remains disabled.
