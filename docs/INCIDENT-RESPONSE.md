# WebCom v3 release incident response

This runbook starts **after** a WebCom release has already been approved or published. It does not replace the Release Candidate, public-release approval, or live publication-health gates.

WebCom release tooling intentionally does **not** modify a public CDN, website, Stable manifest endpoint, installed browser, or customer machine automatically. Incident actions remain explicit and reviewable.

## 1. Preserve evidence first

Before changing hosting or release files, preserve:

- `PUBLIC-RELEASE.json` and its SHA-256,
- the exact signed installer and Stable manifest,
- `release-candidate.json`,
- signed-dist verification,
- release provenance and SPDX SBOM,
- the release evidence ZIP,
- `published-release-health.json` when available,
- relevant server/CDN audit logs outside this repository.

Do not overwrite the current approval or evidence bundle while investigating.

## 2. Choose the incident path

### Hosting/CDN mistake only

If the approved binary itself is good but the live check finds a stale/wrong upload, restore the **exact approved** installer/manifest and rerun `Test-PublishedRelease.ps1`. Do not create a new approval for byte-identical artifacts.

### Withdraw the current release

If the current release should no longer be offered for new installs/updates but no previous release is being restored, prepare a withdrawal package:

```powershell
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File .\scripts\Prepare-ReleaseWithdrawal.ps1 `
  -CurrentPublicReleaseApproval .\release\PUBLIC-RELEASE.json `
  -Mode WithdrawOnly `
  -IncidentId INC-2026-001 `
  -Reason "Describe the release issue" `
  -StableManifestUrl https://downloads.example.org/webcom/stable.v2.txt
```

The command creates evidence and manual instructions only. It does not take the endpoint offline.

### Restore a last-known-good public release

Use only a **previously approved Stable release** and validate it first:

```powershell
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File .\scripts\Test-RollbackCandidate.ps1 `
  -CurrentPublicReleaseApproval .\release-current\PUBLIC-RELEASE.json `
  -PreviousPublicReleaseApproval .\release-previous\PUBLIC-RELEASE.json
```

Then prepare the withdrawal/restore bundle:

```powershell
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File .\scripts\Prepare-ReleaseWithdrawal.ps1 `
  -CurrentPublicReleaseApproval .\release-current\PUBLIC-RELEASE.json `
  -PreviousPublicReleaseApproval .\release-previous\PUBLIC-RELEASE.json `
  -Mode RestoreLastKnownGood `
  -IncidentId INC-2026-001 `
  -Reason "Describe the release issue" `
  -StableManifestUrl https://downloads.example.org/webcom/stable.v2.txt
```

The rollback candidate must be older, Stable, Authenticode-valid, hash-identical to its prior `PUBLIC-RELEASE.json`, and signed by the same pinned publisher certificate as the current release.

## 3. Important rollback limitation

Restoring an older Stable manifest protects **new downloads and eligible older installations**, but it does **not** automatically downgrade machines already running the newer affected WebCom version. WebCom Stable update policy rejects lower-version offers.

For affected machines already on the bad version:

1. use WebCom's verified **user-triggered rollback** when the preserved previous installer and recovery policy are available, or
2. ship a **higher-version hotfix** through the complete build → Windows RC → approval → publication → live-health chain.

Do not weaken version checks or publisher verification to force a downgrade.

## 4. Security incidents

For suspected signing-key compromise, malicious binary substitution, dependency compromise, or unauthorized release changes:

- stop publication/promotion work,
- withdraw the affected download/update endpoint,
- preserve all release and signing evidence,
- follow `SECURITY.md` and the publisher's certificate-revocation process,
- do not reuse a suspected signing key,
- treat any replacement binary as a new signed release with a new full RC/evidence chain.

A certificate rotation may make the normal same-publisher rollback path ineligible. In that case prefer a reviewed higher-version hotfix/recovery plan rather than bypassing publisher pinning.

## 5. After manual recovery

After restoring or replacing public files:

1. run `Test-PublishedRelease.ps1` against the approval that is now meant to be live,
2. archive the new `published-release-health.json`,
3. archive the incident ZIP and `RELEASE-WITHDRAWAL.json`,
4. record the incident timeline and remediation externally,
5. only resume normal release work when live health and support/security pages are healthy.
