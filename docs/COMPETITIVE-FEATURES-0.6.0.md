# WebCom 0.6.0 competitive feature review

This file is a development plan, not a claim of one-for-one parity with every browser.

## Implemented or already present

- Profiles and Incognito/private isolation
- Tab groups, pinned tabs, tab search, mute and sleeping/unloading
- Persistent Workspaces
- Horizontal and vertical tabs
- Two-tab split view
- Quick Commands / command palette
- Visible-page PNG capture
- History with search and bulk cleanup
- Nested bookmark folders and HTML import/export
- Download bubble plus Pause/Resume/Cancel/Retry
- Site permissions, cookies/site data and HTTPS/TLS diagnostics
- Session recovery and local performance controls

## High-priority continuation

1. Reader mode / distraction-free reading.
2. Named sessions: save and restore a chosen set of tabs/workspace layout independently from crash/session restore.
3. Per-tab cookie containers for logging into the same service with isolated identities without requiring separate windows.
4. Collapsible/minimized vertical-tab rail and stronger drag/drop workspace UX.
5. Full-page and selection capture in addition to current viewport capture.
6. Better PDF affordances where WebView2 exposes safe APIs; do not duplicate a full PDF engine in the native shell.
7. All-sites permissions/data manager and clearer exception search/reset.
8. Improved accessibility pass: screen-reader names for every new control, high-contrast verification and full keyboard workspace/split navigation.

## Intentionally not bundled into 0.6.0

- Always-on mail/calendar/social web panels: each panel can create more live web content and conflicts with the low-memory objective.
- A custom plaintext password database: credentials remain under WebView2/Windows profile storage.
- Chrome Sync: WebView2 does not provide Chrome Sync to the host application.
- A custom extension store or Chromium-extension compatibility promise: that is not a safe capability to imply from a WebView2 shell.
