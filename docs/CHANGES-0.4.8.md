# WebCom 0.4.8 changes

Version 0.4.8 is the **recovery and download-control update** built on the Windows-compiling 0.4.7.4 baseline.

## Session recovery

- Session metadata now keeps `state.v1.bak` as a last known-good generation.
- Before replacing a valid primary session, WebCom rotates that valid generation into the backup.
- A fresh successful save seeds the backup automatically.
- Startup validates `state.v1` first and automatically falls back to the backup only if the primary is unreadable, malformed, unsafe, oversized or otherwise invalid.
- The first profile window shows a non-blocking recovery notice when the backup was used.
- Recovery never restores arbitrary schemes or credential-bearing URLs; the backup passes the same parser and safety validation as the primary session.

## Downloads

- The native Downloads page now exposes Pause / Resume and Cancel for live operations.
- Paused downloads remain controllable even though WebView2 represents a pause as `Interrupted` with `USER_PAUSED`.
- Interrupted downloads use WebView2 `CanResume`/`Resume` when the runtime still owns a resumable operation.
- If a previous-run download cannot be resumed in place, Retry opens a validated HTTP(S) source in a new tab instead of invoking an arbitrary operating-system scheme.
- Paused operations are preserved by Clear completed and continue to block sleep/tab-close protection until resumed or cancelled.
- Download rows now use compact native action buttons and retain the progress bar while paused.

## Settings and polish

- Settings documents the automatic session-recovery behavior.
- Downloads copy and action hierarchy were tightened to make the page behave more like a first-class browser surface.
- All features remain native Win32 UI and do not create a hidden WebView for browser chrome or animation.
