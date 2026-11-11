# WebCom 0.5.2 changes

Version 0.5.2 is the **credentials, autofill and TLS diagnostics** milestone built on the Windows-compiling lineage.

## Credentials and autofill

- Password-save preference moved from a global setting to each saved profile.
- General address/form autofill is a separate per-profile preference.
- Session format advances to v4 and remains backward compatible with v1-v3.
- Older profiles inherit the previous global password-save preference and default general autofill to enabled.
- New profiles inherit the current profile's password/autofill choices.
- Incognito forces both password saving and general autofill off.
- Added separate **Clear saved passwords** and **Clear autofill form data** actions using WebView2 profile browsing-data APIs.
- WebCom does not read, expose or store password values itself.

## TLS diagnostics

- WebCom subscribes to WebView2 server-certificate-error events where available.
- On a TLS verification failure, the current tab records the requested URI, verification status, certificate subject, issuer, and validity dates.
- Site information exposes **Certificate details** only when WebView2 supplied failure metadata.
- WebCom leaves the WebView2/Windows verification action unchanged and does not bypass TLS errors.
- Successful HTTPS pages continue to show the normal secure connection state; this release does not claim full certificate-chain inspection for successful navigations.

## Settings and persistence

- Password/autofill settings are grouped under **Passwords and autofill**.
- Existing v0.5.1 cookies/site data, site permissions, HTTPS/HTTP state and profile deletion remain unchanged.
