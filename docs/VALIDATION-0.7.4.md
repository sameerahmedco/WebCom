# WebCom 0.7.4 validation

0.7.4 adds the runtime update transport/staging layer while keeping automatic update activity disabled.

## Portable validation
- GCC C++20: 23/23 CTest targets pass.
- Clang C++20: 23/23 CTest targets pass.
- 907 explicit regression/security checks pass, including 80 MSVC/security source-preflight checks.
- Zero GCC/Clang build warnings in the exact release tree.
- `update_runtime_model` tests cover HTTPS endpoint configuration, missing trust roots, channel mismatch, pinned-publisher mismatch, newer/same-version evaluation and traversal-safe deterministic staging paths.
- The compiled source preflight also asserts the native updater source retains WinHTTP cookie/auth isolation, HTTPS downgrade protection, SHA-256, WinVerifyTrust, signer-certificate extraction and Windows version-resource product identity checks.
- CMake itself rejects non-HTTPS release endpoints and malformed publisher fingerprints, even when a release engineer bypasses `Build.ps1`.

## Windows acceptance required
1. Fresh-extract 0.7.4 and run `build.cmd`; confirm source preflight passes before native compilation.
2. Confirm an ordinary developer build shows **Trusted endpoint: Not configured in this build** and performs no update network request.
3. Create a release build with real Stable/Beta HTTPS manifest URLs and the real publisher certificate SHA-256.
4. Serve a valid v2 manifest for a newer disposable signed installer and press Check for updates.
5. Change the manifest channel, publisher fingerprint, package hash, size, ProductName, ProductVersion or signer and confirm the corresponding release is rejected.
6. Press Stage and confirm the installer appears only after all verification succeeds; interrupted/tampered `.part` files must not become staged installers.
7. Press Install, cancel the confirmation and UAC independently, and confirm no install occurs and the staged package remains usable.
8. Allow installation of a disposable newer build and confirm the new version enters its health state.
9. Confirm 30 seconds of normal runtime or a graceful close clears the health transaction.
10. Simulate repeated early termination before health completion and confirm rollback recommendation appears without automatic rollback.
11. Re-run profiles, session recovery, local files, Default Apps, site controls and screenshot smoke tests.

The native WinHTTP/CNG/WinVerifyTrust/installer paths are Windows-only and cannot be executed in this Linux validation host. Windows acceptance remains decisive.
