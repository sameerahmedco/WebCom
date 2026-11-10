# WebCom 0.5.1 changes

Version 0.5.1 is the **privacy, connection security and profile lifecycle** milestone built on the Windows-compiling lineage and the 0.5.0 site-control/download-bubble work.

## Site cookies and data

- The site-info shield shows the current site's cookie count after WebView2 returns it.
- A native cookie-details menu lists cookie names, domain/path, and Secure/HttpOnly flags. Cookie values are deliberately not exposed in WebCom chrome.
- **Clear cookies for this site** deletes only cookies returned by WebView2 for the active URI and prompts the user to reload.
- Cookie state is profile-scoped through WebView2's CookieManager.

## Connection security

- HTTPS pages show **Connection is secure**.
- HTTP pages show **Connection is not secure** and explain that traffic is not encrypted.
- Certificate-related navigation failures are retained on the tab and shown as **Certificate problem** with a more specific failure label.
- The omnibox shield uses different visual emphasis for secure, insecure and certificate-error states.

## Profile deletion

- The profile menu and native Profiles page expose **Delete current profile**.
- WebCom refuses to delete the final remaining profile or a profile with an active download.
- Deletion requires explicit destructive confirmation.
- WebCom uses `ICoreWebView2Profile8::Delete` to mark the underlying WebView2 profile for deletion, then removes WebCom-owned history, bookmark, permission, appearance, download-history and saved-session metadata for that profile.
- Downloaded files are intentionally left on disk.
- Both current and recovery session generations are sanitized so recovery cannot restore a deleted profile.

## Compatibility

- Existing 0.5.0 profile, session, permission, history and bookmark files remain readable.
- No new hidden WebView is kept alive for cookies or security display. A temporary WebView is created only when deleting a profile that currently contains only native WebCom pages and therefore has no WebView2 profile object to query.
