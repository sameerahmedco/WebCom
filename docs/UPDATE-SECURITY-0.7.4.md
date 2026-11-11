# WebCom 0.7.4 update security model

WebCom 0.7.4 deliberately separates **check**, **stage**, and **install**. None are automatic.

1. **Build trust root:** release builds embed Stable/Beta manifest HTTPS URLs plus one expected Authenticode signer-certificate SHA-256. Developer builds embed none.
2. **Manifest check:** user must enable update checks and press Check. The v2 manifest must match the selected channel, describe a newer version and carry the same publisher fingerprint as the build pin.
3. **Package stage:** user presses Stage. The installer downloads to a temporary `.part` path. WebCom verifies exact size, SHA-256, ProductName/ProductVersion, Windows Authenticode trust and the signer certificate fingerprint.
4. **Commit:** only a fully verified file is renamed into the staged installer path and recorded in `update-state.v1`.
5. **Install:** user presses Install, WebCom re-verifies the staged file, asks again, marks install scheduled, invokes Windows elevation and closes its windows.
6. **Health:** the new target build records a local health window. Normal runtime/graceful close clears the transaction; repeated interrupted health windows recommend rollback.

The updater does not share WebView/browser cookies, does not use stored website credentials, does not accept HTTP endpoints, does not trust a publisher fingerprint supplied only by the downloaded manifest, and does not perform unattended installation or rollback.
