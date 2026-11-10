# WebCom 0.4.2 UI responsiveness update

- Added History to the main menu with Ctrl+H.
- Stores up to 500 recent normal-profile visits per profile; Incognito visits are excluded.
- History menu shows the 18 most recent entries and supports clearing browsing history.
- Removed native topmost Win32 tooltips to eliminate the transient yellow popup at the top-left during rapid layout/tab changes.
- Removed synchronous `RDW_UPDATENOW | RDW_ALLCHILDREN` repaint from native new-tab selection.
- New-tab/tab selection now invalidates through the retained double buffer and uses a short 140 ms frame cadence.
- Omnibox focus on native new-tab pages is deferred until the next message-loop turn to avoid layout/focus/repaint contention.
