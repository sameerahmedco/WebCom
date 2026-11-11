# WebCom 0.7.0 changes

0.7.0 is the public-beta release-infrastructure milestone.

## Update policy and rollback state
- Update checks are explicitly opt-in and default off.
- Stable/Beta remains user-selectable.
- `updates.v1` advances internally to a v2 header while preserving v1 migration; migrated users remain opted out of checks.
- `update-state.v1` stores only staged update transaction metadata, never browsing data.
- Portable transitions cover verified package, install scheduled, awaiting health check, success clearing, failed-start counting and rollback recommendation.

## Native Updates page
- Shows release channel, update-check consent, download/install status and current transaction phase.
- Shows staged target/previous versions and health-failure count when applicable.
- Can clear stale staged transaction metadata after confirmation.
- Automatic downloads and automatic installation remain disabled.

## Signed installer groundwork
- Added `installer/WebCom.iss` for x64 Inno Setup packaging.
- Added `scripts/Build-SignedInstaller.ps1` for compile -> Authenticode sign -> timestamp -> verify -> publisher fingerprint pin.
- Added `scripts/Test-FreshInstall.ps1` for isolated silent install/file-layout/uninstall QA.
