# Background update checks and verified downloads

WebCom v3 separates **checking**, **downloading**, and **installing** updates so each security boundary is explicit.

## Defaults and consent

- A new profile starts with update checks **off**.
- Choosing Stable or Beta does not contact an update service by itself.
- Enabling **Update checks** allows WebCom to contact the configured HTTPS release-manifest endpoint in the background.
- **Automatic verified downloads** is a separate preference. Enabling it also enables update checks because a package cannot be downloaded safely without first validating a manifest.
- Installing an update is never automatic. A staged installer still requires the user to open Updates and explicitly launch the verified installer.
- Rollback remains explicit and is never automatic.

Older `WEBCOM_UPDATES` v1/v2 preferences migrate conservatively: existing check consent is preserved, but automatic downloads remain disabled until the user opts in.

## Check cadence

When update checks are enabled, WebCom schedules the first due background check after a short startup delay and then uses a six-hour cadence.

The last background contact time is stored in `update-automation.v1`, so repeatedly opening profiles or restarting WebCom does not intentionally restart the six-hour clock. Multiple profile windows share the same WebCom data root and use a process-wide network-operation gate plus the persisted timestamp to avoid duplicate simultaneous update work.

Transient failures use the existing bounded retry state. Only the first two failures receive quick retries; the third and later failures fall back to the normal six-hour background cadence. This avoids turning an unavailable endpoint into a tight polling loop.

## Verification before download and staging

A background check applies the same policy as a manual check:

1. HTTPS manifest endpoint only.
2. Selected release channel must match the manifest.
3. Stable rejects prerelease version labels.
4. Candidate version must be newer than the running version.
5. Manifest publisher fingerprint must match the publisher pin compiled into WebCom.
6. The manifest must be install-eligible signed-release schema v2.

If automatic verified downloads are enabled and a newer release passes those checks, WebCom may download the installer in the background. Before retaining it, WebCom verifies:

- exact package size,
- SHA-256,
- Windows Authenticode trust,
- expected WebCom product/version identity,
- the same pinned publisher certificate.

Only after all checks pass does the update enter `verified` staged state.

## Installation remains explicit

Background activity stops at a verified staged installer. WebCom displays a native notification and requires a user action before installer launch.

Immediately before launch WebCom verifies the staged package again, detects the registered install scope, and preserves that scope:

- per-user: `/CURRENTUSER`, no forced elevation;
- per-machine: `/ALLUSERS`, administrator approval required;
- unknown/portable/ambiguous: launch is blocked.

WebCom does not silently install, silently restart, or silently roll back the browser.

## Privacy boundary

When background checks are enabled, the configured update hosting service can receive ordinary network metadata from the HTTPS request, such as the user's IP address and the `WebCom/3.0.0 updater` user-agent string. WebCom does not attach browsing history, page URLs, cookies, passwords, form data, profile names, or support diagnostics to update requests.

Automatic verified installer downloads contact the package URL from the accepted release manifest. No update analytics or telemetry service is added by this feature.

## Windows release acceptance

Before public release, test all of the following on the signed Windows candidate:

1. Checks disabled: no scheduled release-manifest contact occurs.
2. Checks enabled: first due check occurs after the startup delay and does not display a modal success/failure dialog.
3. Two profile windows: only one background endpoint operation occurs for the same due window.
4. Repeated restart: the persisted six-hour cadence prevents repeated startup polling.
5. Failure path: first two failures use bounded retry; repeated failures fall back to the six-hour cadence.
6. Automatic downloads disabled: an available update notifies but does not download the installer.
7. Automatic downloads enabled: a valid signed installer is downloaded, verified, staged, and announced without launching Setup.
8. Tampered package/hash/signature/publisher: staging fails and no install action becomes available.
9. Per-user/per-machine/portable install-scope behavior still matches `UPDATE-INSTALL-SECURITY.md`.
10. Clearing/finishing an update transaction resumes future background checks.
