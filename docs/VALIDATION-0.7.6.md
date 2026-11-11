# WebCom 0.7.6 validation

0.7.6 adds a native recovery/reliability center, persistent bounded update-check backoff and an explicit local recovery-report package around the 0.7.5 crash-evidence/verified-rollback foundation.

## Completed portable validation
- GCC C++20: 27/27 CTest targets pass with zero compiler warnings.
- Clang C++20: 27/27 CTest targets pass with zero compiler warnings.
- 873 portable regression checks pass across 26 regression executables.
- 110 MSVC/security source-preflight checks pass.
- 983 explicit checks total, zero failures.
- Update-retry tests cover bounded 15/30/60/120/300-second delay progression, deadline calculation, invalid categories, atomic round-trip, reset and corrupt-state fail-closed behavior.
- Reliability-report tests cover unclean-session evidence, update/rollback status, recovery package status, retry status and explicit local-only privacy language.
- Core/storage tests cover `webcom:recovery` routing and persistence.
- Source preflight enforces bounded retry policy, bounded failure count, atomic retry-state writes, explicit retry deadlines, local-only recovery-report export, bounded runtime-log use, ZIP-builder use and privacy wording.
- Static native-source audit confirms exactly one `get_CookieManager` call remains and it is still through `ICoreWebView2_2`.

## Windows acceptance required
1. Fresh-extract 0.7.6 and run `build.cmd`; confirm the expanded MSVC/security preflight succeeds before native compilation.
2. Open Recovery & Reliability from Settings, Help and Quick Commands.
3. Force an update manifest transport/validation failure, immediately click Check again, and confirm WebCom blocks the retry and shows the remaining cooldown.
4. Restart during the cooldown and confirm the retry state persists; wait until the deadline and confirm Check becomes usable again.
5. Switch Stable/Beta or disable update checks and confirm the retry state clears.
6. Force an unclean WebCom exit and confirm the next launch shows previous-session evidence on Recovery & Reliability.
7. Export recovery report ZIP and inspect `recovery.txt`, `diagnostics.txt`, `privacy.txt` and `runtime.txt`; confirm no automatic network/upload occurs and runtime URLs are redacted.
8. Repeat the 0.7.5 signed update health-failure/rollback acceptance path and confirm the rollback button also appears from Recovery & Reliability when eligible.
9. Re-run normal browser, installer, Default Apps and update security smoke tests.

The native Windows UI, signed updater transport/UAC and actual rollback path cannot be executed in this Linux validation host; Windows acceptance remains decisive.
