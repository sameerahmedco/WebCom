# WebCom 0.6.8 changes

## Support diagnostics
- Added native `webcom:diagnostics`.
- Shows WebCom version, WebView2 Runtime version, profile label/key, release channel, workspace and bounded browser-state counts.
- Omits browsing URLs, cookie values and password values.
- Copy diagnostics and Open data folder actions.

## Reader export
- Reader controls now expose Export instead of only Save text.
- TXT, print-friendly HTML and Markdown output.
- HTML carries Reader size/width/font choices and print CSS.
- Reader content stays temporary and out of session recovery.

## Update-channel groundwork
- Persistent Stable/Beta preference in `updates.v1`.
- Strict portable release-manifest parser: semantic version, Stable/Beta channel, HTTPS URL, SHA-256 and publication timestamp.
- No automatic update download or install yet; signature verification remains required before enabling that path.
