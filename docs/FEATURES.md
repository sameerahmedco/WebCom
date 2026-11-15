# WebCom v3 features

- Chrome-like native UI with destination-first WebView handoff on tab switches, hidden-until-laid-out startup controls, buffered omnibox suggestion reveals, and reduced-motion-safe tab/reflow/menu animation.
- Multi-profile browsing.
- Workspaces.
- Vertical tabs.
- History.
- Bookmarks.
- Password saving.
- A dedicated native **Passwords & Autofill** page exposes WebView2-supported save-password and form-autofill toggles plus confirmation-gated clear actions. It explicitly states that WebCom does not read, list, export, or display saved password values because those remain owned by the WebView2 profile store.
- Cookie controls.
- Site permissions.
- Privacy Manager v2 is searchable across both cookie domains and permission origins and shows live bounded profile totals for cookie domains/cookies and permission origins/saved rules; existing scoped Clear/Reset actions keep their original targets.
- SSL/security UI.
- **Extensions v2** adds native search, All/Enabled/Disabled/Needs-attention filters, live counts, unpacked-source health classification, and existing Details/Enable/Disable/Reload/Open folder/Remove actions. WebCom explicitly does not claim Chrome Web Store or Chrome toolbar/popup compatibility.
- **Ad Blocker v2** adds profile-scoped site exceptions from the site-information shield. Exceptions are DNS-host validated and capped at 64, then applied to the bundled extension's DNR initiator exclusions, cosmetic selectors, and YouTube helper before an automatic bundled-extension reload. Extensions shows the exception count, health details, and a confirmation-gated **Clear sites** action. Incognito remains explicitly unsupported by the bundled blocker.
- Extension diagnostics.
- Reader mode.
- Failed navigations use a native recovery surface that shows the failed address/error and keeps **Try again**, **Edit address**, and **New Tab** actions owned by WebCom rather than by site content.
- Split view.
- Capture tools.
- Crash recovery.
- Safe updates.
- Verified rollback.
- Stable/Beta channels.
- Accessibility support: custom tab-strip MSAA semantics plus a native-client MSAA bridge for painted internal-page actions. Visible painted actions expose push-button roles, names, screen bounds, hit-testing, focus state/events and default activation while standard Win32 edit/button children remain in the accessibility tree. Visible native focus outlines, keyboard-accessible internal-page actions, profile + Windows High Contrast composition, Windows/profile/Remote Desktop reduced-motion policy, and per-monitor DPI-aware native chrome are also included.
- Memory checks. The native **Performance snapshot** separates WebCom host vs WebView2 process-group working/private memory and includes tab/sleep/workspace state plus the current automatic-sleep policy.
- Native **About WebCom** page reports WebCom/WebView2 version state, release channel, Windows default-browser state, profile, sleep policy and direct maintenance/support actions.

## New Tab and workspace dashboard

- New Tab remains a native Win32 surface: no extra WebView, renderer, or background worker is created for the dashboard.
- Quick actions open **History**, **Downloads**, **Bookmarks**, and **Saved Sessions** directly.
- Recent-site cards are derived from WebCom History, deduplicated by normalized host, and capped at six entries; clicking a card navigates the current New Tab rather than spawning a hidden page.
- Workspace cards show the active workspace and live tab counts, switch spaces in one click, expose the existing workspace manager, and offer **New workspace** when capacity allows.
- Compact windows keep the existing search/shortcut layout and progressively reveal dashboard sections only when there is enough vertical room.

- New Tab now supports up to eight per-profile custom shortcut tiles stored in a bounded versioned local file. Users can add/remove shortcuts directly from New Tab.
- Recent-site cards expose **Pin** and **Hide** actions. Pin promotes the site into custom shortcuts; Hide is a reversible New Tab preference and does not delete browsing history.

- New Tab supports up to eight per-profile custom shortcut tiles with native add/remove controls and safe URL validation.
- Recent-site cards can be pinned into custom shortcuts or hidden via a reversible bounded host preference; hiding a recent site does not delete browsing history.

- History v2 adds bounded **Top sites** chips with visit counts, one-click filtering, and confirmation-gated whole-site history cleanup.
- Downloads are grouped visually by local date while keeping their existing persisted order and action behavior.

- Tab productivity adds workspace-scoped **Close duplicate tabs** with active/pinned/download protection and **Sleep other tabs** using the existing safe suspension path.

## Performance controls

- Automatic background-tab sleeping is configurable per profile at **1 / 5 / 15 / 30 minutes / Never**.
- The preference lives in a separate versioned `WEBCOM_PERFORMANCE` file with atomic writes and hardened profile-name validation, avoiding a migration of WebCom's existing session/state format.
- **1 minute** maps to WebCom's aggressive Memory Saver target; 5/15/30/Never keep the Balanced target.
- Manual sleep remains available regardless of the automatic timeout.
- Each profile can keep up to **64 exact HTTP(S) hosts** on a **Never sleep these sites** list. Host matching is normalized (`www.` stripped, lowercase), applies to automatic sleeping only, and does not disable manual sleep.
- Automatic sleep polling is adaptive rather than fixed: 1-minute mode checks every 10 seconds, 5-minute mode every 30 seconds, 15/30-minute modes every 60 seconds, and Never disables the automatic polling timer entirely.
- Downloads history, Site Permissions, Saved Sessions, and unpacked-extension source metadata/health are loaded on first use instead of during every ordinary browser constructor.

## Downloads v2

- Each completed/failed download can open its original validated HTTP(S) source page.
- **Retry failed** handles at most five records at once: resumable current-session operations resume directly, otherwise validated source pages reopen in background tabs.
- Completed download-history records older than 30 days are pruned automatically from WebCom's local list; downloaded files are never removed by this history cleanup.

## Setup and onboarding

- The Welcome page shows a live four-step setup checklist for **Default browser**, **Bookmarks**, **Workspaces**, and **Update checks**.
- Each checklist row opens the existing native setup surface directly; WebCom never silently changes Windows defaults or imports user data.
- Setup progress is computed by the portable onboarding model from current browser state rather than stored as a fake completion flag.
## Public-beta recovery and support

- **Recently Closed** persists up to 20 normal-profile entries across restarts and restores workspace, group, pinned, and keep-awake context. Private/ephemeral tabs are excluded, and the History page exposes direct restore/clear controls.
- Completed downloads use an actionable native notice that can open the completed file when it is still available; update-available notices open the native Updates page.
- The native **Report a Problem** page can copy a privacy-safe report template and open diagnostics/recovery export actions. WebCom does not automatically upload diagnostics, browsing URLs, page titles, cookies, passwords, or form contents.
- Privacy Manager reports the bundled Ad Blocker profile state and paused-site count without inventing blocked-request statistics that the WebView2 MV3 host surface does not reliably expose.
- Successful full-profile browsing-data cleanup also clears WebCom History, Recently Closed recovery, and saved site permissions while leaving downloaded files and bookmarks intact. Profile deletion removes the profile's Home, Performance, and Recently Closed metadata as well.

