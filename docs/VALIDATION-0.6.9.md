# WebCom 0.6.9 validation

0.6.9 keeps the Windows-compiling lineage and corrected `ICoreWebView2_2` CookieManager path. Portable feature/model code was validated here; screenshot annotation and PowerShell Authenticode workflows still require a fresh Windows acceptance run.

## Portable validation
- GCC C++20: 20/20 CTest targets passed, zero build warnings.
- Clang C++20: 20/20 CTest targets passed, zero build warnings.
- 765 explicit regression checks passed.
- Diagnostics ZIP tests cover safe names, traversal rejection, case-insensitive duplicate rejection, size limits and ZIP signatures.
- Update tests cover v1 compatibility, v2 install-eligibility, HTTPS-only URLs, Authenticode requirement, nonzero package size and pinned publisher SHA-256 validation.

## Windows acceptance required
1. Build from a fresh extracted 0.6.9 folder with `build.cmd`.
2. Capture a visible page and selected area, open annotation, draw Pen/Rectangle, Undo, Copy and Save PNG.
3. Export Support Diagnostics ZIP and inspect all three files; confirm no current browsing URL, cookie value, password value or form data appears.
4. Use an Authenticode-signed disposable package with `New-ReleaseManifest.ps1`, then verify it using `Verify-UpdatePackage.ps1`.
5. Tamper with the package and confirm verification fails on SHA-256.
6. Test a different signed publisher and confirm publisher-fingerprint verification fails.
7. Re-test screenshots, cookies/site info, profile deletion and startup/recovery behavior.

Automatic update download/install is intentionally not enabled in this release.
