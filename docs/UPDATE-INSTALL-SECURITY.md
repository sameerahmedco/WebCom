# WebCom update installation security

WebCom's update flow is deliberately split into **check → stage → verify → explicit install → health check**. It does not silently download or install a release merely because a channel is selected.

## Trust boundary

Before a package can be staged or launched, WebCom requires all of the following:

- HTTPS release manifest on the selected Stable/Beta channel,
- install-eligible v2 manifest schema,
- newer-version policy,
- exact package byte count,
- exact SHA-256,
- Windows Authenticode trust,
- WebCom product/version identity,
- publisher certificate SHA-256 matching the fingerprint compiled into the running WebCom build.

The staged installer is re-verified immediately before every install or rollback launch.

## Installation-scope preservation

Latest-49 does not assume every WebCom installation is machine-wide.

WebCom checks the Windows registration for the **current running `WebCom.exe`** and accepts an install scope only when exactly one matching registration exists:

- **Per-user**: the running executable matches WebCom's HKCU application registration. The update is launched with `/CURRENTUSER` and no forced elevation.
- **Per-machine**: the running executable matches WebCom's HKLM application registration. The update is launched with `/ALLUSERS` and Windows administrator approval (`runas`).
- **Unknown / ambiguous / portable**: WebCom fails closed. The verified package may remain staged, but WebCom will not launch the installer automatically.

This prevents a normal per-user install from being unexpectedly converted into an elevated machine install, and prevents an unregistered portable copy from guessing where an update should be installed.

## Cancelled or failed installer launch

Before launching a verified installer, WebCom persists the transaction as `install-scheduled`.

If the installer succeeds and the target WebCom version starts, the transaction enters the local startup health window.

If the installer is cancelled or fails before replacing WebCom, the **previous version** will still be running on the next launch. Latest-49 detects that exact state and changes the transaction back to `verified` while preserving the staged package identity. The package must then pass full verification again before another launch attempt.

This avoids an `install-scheduled` transaction becoming permanently stuck after a cancelled installer.

## Rollback

The in-product rollback path uses the same scope-preservation policy:

- rollback installer is re-verified,
- per-user rollback uses `/CURRENTUSER` without forced elevation,
- per-machine rollback uses `/ALLUSERS` with administrator approval,
- unknown scope is blocked,
- automatic rollback remains disabled.

Rollback readiness still depends on the existing health/recovery policy and preserved known-good installer.

## Windows release acceptance

Before Stable release, test all of the following on actual Windows:

1. Per-user install → stage update → install: no unnecessary UAC prompt; installer stays per-user.
2. Per-machine install → stage update → install: UAC is requested; installer stays machine-wide.
3. Portable/unregistered `WebCom.exe`: staging may succeed, but automatic installer launch is unavailable.
4. Cancel the interactive installer after WebCom schedules it; reopen the old version and confirm the Updates page returns to a verified staged transaction rather than remaining `install-scheduled`.
5. Retry the staged update and confirm it is re-verified before launch.
6. Repeat the scope rules for user-triggered rollback.
7. Confirm successful target-version startup enters the 30-second local health window and a successful health check clears the update transaction.
