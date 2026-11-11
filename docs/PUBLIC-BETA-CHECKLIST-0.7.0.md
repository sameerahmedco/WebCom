# WebCom 0.7.0 public-beta acceptance checklist

Before publishing a beta installer:

1. Build `WebCom.exe` with a fresh MSVC `build.cmd` run.
2. Run `Test-UI.cmd` and all Windows UI smoke tests.
3. Verify cookies/site info, profile deletion, password/autofill, Workspaces, Split View, Reader, screenshots/annotation and external-protocol prompts.
4. Create the installer with `Build-SignedInstaller.ps1` using the production code-signing certificate and HTTPS RFC3161 timestamp service.
5. Verify installer Authenticode status and pinned publisher certificate SHA-256.
6. Generate a v2 release manifest and verify it with `Verify-UpdatePackage.ps1`.
7. Run `Test-FreshInstall.ps1` on a clean Windows VM/user profile.
8. Confirm install/uninstall does not remove user data outside the application install directory.
9. Exercise update transaction transitions in a disposable data root and verify rollback recommendation after repeated failed health starts.
10. Confirm automatic download/install remains disabled in the browser.
11. Record installer SHA-256, publisher fingerprint, manifest and acceptance logs for the release.
