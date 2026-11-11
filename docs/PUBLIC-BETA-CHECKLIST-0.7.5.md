# WebCom 0.7.5 public-beta checklist

- [ ] Fresh MSVC build passes the source/security preflight.
- [ ] Normal exit leaves a clean runtime-session marker.
- [ ] Forced termination produces local unclean-start evidence on next launch.
- [ ] Runtime log stays bounded/rotates and does not contain full web/file URLs.
- [ ] Support ZIP works both with and without `runtime.txt`.
- [ ] Healthy signed update preserves exactly one known-good recovery installer.
- [ ] Two failed health starts recommend rollback.
- [ ] Rollback is unavailable when no matching recovery package exists.
- [ ] Tampered recovery installer is rejected.
- [ ] Valid rollback installer is re-verified and requires explicit confirmation/UAC.
- [ ] Previous version startup clears the failed update transaction.
- [ ] Existing updater, profiles, files, cookies, downloads and Default Apps smoke tests still pass.
