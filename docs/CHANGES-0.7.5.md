# WebCom 0.7.5 changes

## Bounded runtime/crash evidence
- New `runtime_log` model with a 256 KiB current log and one rotated previous generation.
- Operational event details are bounded and web/file URLs are redacted.
- `session-state.v1` records whether the previous process reached a clean shutdown.
- Session start, clean shutdown, WebView2 process failures and key updater/health events are logged locally.
- No automatic crash upload/reporting is enabled.

## Support diagnostics
- Export Support ZIP now explicitly asks whether to include the bounded runtime log.
- Diagnostics without the runtime log remain available by choosing No.
- The privacy note tells the user to review operational error/status text before sharing.

## Verified recovery installer
- After an updated version passes its local health window, WebCom re-verifies the staged installer and preserves exactly one known-good recovery generation.
- Recovery metadata stores only version, SHA-256, byte size and pinned publisher fingerprint.
- A future rollback candidate must match the update transaction's `previous_version`.
- Before rollback, WebCom re-verifies exact size/hash, WebCom Windows product identity, Authenticode trust and publisher pin again.
- Rollback requires an explicit confirmation plus Windows UAC; no automatic rollback occurs.
- When the previous version starts, the existing update transaction is cleared as a completed rollback.
