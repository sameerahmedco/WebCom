# WebCom 0.7.5 validation

0.7.5 adds bounded local runtime/crash evidence and a preserved known-good rollback installer around the existing secure update path.

## Completed portable validation
- GCC C++20: 25/25 CTest targets pass with zero build warnings.
- Clang C++20: 25/25 CTest targets pass with zero build warnings.
- 847 portable regression checks pass.
- 94 MSVC/security source-preflight checks pass.
- 941 explicit checks total, zero failures.
- Runtime-log tests cover clean/unclean session markers, event append, URL redaction and invalid-event rejection.
- Recovery-model tests cover deterministic paths, atomic metadata round-trip, invalid metadata rejection, tampered-size failure and cleanup.
- Source preflight requires rollback health gating, immediate package re-verification, UAC launch, re-verification before recovery preservation, bounded log rotation and URL redaction guards.

## Windows acceptance required
1. Fresh-extract 0.7.5 and run `build.cmd`; confirm MSVC/security preflight succeeds before native compilation.
2. Run WebCom, exit normally, restart and verify the runtime session marker does not report an unclean previous start.
3. Terminate WebCom during a disposable test session, restart, and verify Support Diagnostics/runtime log records local unclean-exit evidence without any upload.
4. Export Support ZIP once with the runtime log and once without it; inspect `runtime.txt` and verify web/file URLs are redacted.
5. Perform a signed disposable update A -> B. Let B run through the 30-second health window, then confirm WebCom preserved B's installer as the single recovery generation.
6. Perform B -> C, terminate C before health completion twice, and confirm rollback is recommended.
7. Click Rollback to B; confirm the preserved B installer is re-verified, Windows shows UAC, WebCom closes only after launch is accepted, and B clears the pending transaction when it starts.
8. Tamper with the preserved recovery installer and confirm rollback is refused.
9. Re-run normal browser/update/installer/Default Apps smoke tests.

The signed rollback/UAC, WebView2 crash and native Windows installer paths cannot be executed in this Linux validation host; Windows acceptance remains decisive.
