# WebCom 0.7.4 changes

## User-triggered update check
- Update checks remain opt-in and never run automatically.
- Stable/Beta manifest URLs and the publisher certificate SHA-256 are generated into release builds from CMake settings.
- Empty developer configuration fails closed and performs no network request.
- HTTPS manifests are fetched with WinHTTP on a worker thread with cookies and automatic authentication disabled and HTTPS-to-HTTP downgrade redirects blocked.
- Manifest bodies are bounded to 64 KiB in the browser.
- v2 manifest channel, semantic version, HTTPS package URL, SHA-256, size, Authenticode requirement and publisher fingerprint are validated.
- Manifest publisher fingerprint must match the fingerprint pinned into the running build.

## Verified package staging
- Stage downloads to a `.part` file under `Updates/staging/<version>`.
- Download must match the exact manifest size.
- SHA-256 is computed with Windows CNG/BCrypt.
- Installer Windows version resources must identify `ProductName=WebCom` and match the manifest's numeric version.
- WinVerifyTrust must accept the Authenticode signature.
- The trusted signer certificate's DER SHA-256 must match the build pin.
- Only after every check passes is the `.part` file committed to the deterministic staged installer path.

## Explicit install + health state
- Staged installer is re-verified immediately before launch.
- Installation requires a confirmation dialog and Windows elevation; WebCom never installs silently.
- Update transaction moves to `install-scheduled` before launcher handoff.
- First startup of the target version enters `awaiting-health-check`.
- 30 seconds of normal runtime or a graceful close marks the update healthy.
- A startup following an interrupted health window increments the failure count; repeated failures show rollback recommended.
- Automatic binary rollback is intentionally not implemented yet.

## Release workflow
- `Build.ps1` accepts Stable/Beta manifest URLs plus the publisher pin.
- Added `Build-PublicBeta.ps1` to build with trust roots, create/sign the installer, generate a v2 manifest and verify the final package.
- MSVC source preflight now asserts the updater retains WinHTTP cookie/auth isolation, HTTPS downgrade protection, SHA-256, WinVerifyTrust, signer-certificate inspection and Windows product-identity verification.
