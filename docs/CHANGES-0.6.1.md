# WebCom 0.6.1 changes

Version 0.6.1 is the **reading, sessions and privacy** milestone built on the 0.6.0 Workspaces/vertical-tabs/split-view foundation.

## Reader Mode

- **F9** extracts text from `article`, `main`, `[role=main]` or the document body after removing common navigation/media/form noise.
- WebView2 script results are decoded by a bounded portable JSON-string parser with surrogate-pair coverage.
- Reader content renders in WebCom's native internal-page surface and is capped at 300,000 characters.
- Reader tabs are ephemeral: they are excluded from saved-session/crash recovery and recently-closed-tab restore.
- The original tab remains available through **Open original**.

## Named Sessions

- Save all normal HTTP(S) tabs in the current workspace as a named session.
- Reopen a session later as new tabs, preserving saved workspace names and pinned state.
- Overwrite and delete actions require explicit intent.
- Storage is per-profile, bounded, validated and atomically replaced under `Sessions/<profile>.v1`.
- Profile deletion removes its named-session store.

## Privacy Manager

- Native page groups cookies by domain and lists saved permission-exception origins.
- Cookie values are never requested for display.
- Clear one cookie domain or reset one origin's saved permission exceptions.
- Clear all cookies/site storage using WebView2's profile browsing-data API, without deleting saved passwords, history or downloaded files.

## Collapsible vertical tabs

- Vertical rail now switches between 244-DIP expanded and 64-DIP collapsed modes.
- Collapsed mode preserves favicon selection, group-color marker, workspace identity and split-view indicator.
- Expanded/collapsed preference persists per profile.
- Session metadata moves to v6 and reads v1-v5; v5 vertical-tab profiles migrate expanded.

## Windows build hardening

- Split legacy same-line `auto` declarators in favicon, History, profile move, bookmark UI and omnibox code into independent declarations.
- Existing `ICoreWebView2_2` CookieManager helper remains the only direct cookie-manager access path.
