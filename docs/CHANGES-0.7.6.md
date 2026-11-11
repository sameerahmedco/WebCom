# WebCom 0.7.6 changes

## Recovery & Reliability center
- Added native `webcom:recovery`.
- Shows previous-session unclean evidence, update transaction state, failed health starts, rollback recommendation, preserved recovery version and update-check retry state.
- Exposes Support Diagnostics, Updates, explicit rollback when eligible, and a local recovery-report export.
- Added Recovery & Reliability entry to Settings, Help and Quick Commands.

## Persistent update retry/backoff
- Failed user-triggered manifest checks persist a bounded retry state in `update-retry.v1`.
- Delay schedule: 15s, 30s, 60s, 120s, 300s maximum.
- Only count, deadline and coarse failure category are persisted.
- Successful checks reset backoff.
- Switching release channel or disabling update checks resets backoff.
- The Updates page and Quick Commands disable the Check action until the retry deadline expires.

## Explicit local recovery report
- Export recovery report creates a bounded ZIP locally only after explicit user action.
- Contains reliability summary, normal diagnostics, privacy text and the bounded URL-redacting runtime log when present.
- No network submission path exists in the export handler.
- Privacy text states that reports are not uploaded automatically and should be reviewed before sharing.

## Preflight hardening
- Source preflight checks retry-delay bounds, failure-count bounds, atomic retry-state persistence, explicit retry deadline, recovery-report privacy wording, bounded runtime-log use, ZIP builder use and absence of updater network calls in the report-export handler.
