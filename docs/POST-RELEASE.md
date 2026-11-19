# WebCom v3 publication and post-release verification

WebCom does not upload release artifacts automatically. Public release is intentionally split into two stages:

1. **local approval and evidence generation**, then
2. **verification of the files actually published to the Internet**.

This prevents a release from being considered healthy merely because the local Windows RC passed.

## One-command local release validation

Run from an **elevated Windows PowerShell** session because public sign-off requires both the default per-user installer path and the explicit machine-wide installer path.

```powershell
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File .\scripts\Invoke-PublicRelease.ps1 `
  -StableManifestUrl https://downloads.example.org/webcom/stable.v2.txt `
  -PackageUrl https://downloads.example.org/webcom/WebCom-Setup-3.0.0-x64.exe `
  -PublisherSha256 <64-hex-publisher-certificate-fingerprint> `
  -SigningCertificateSha1 <40-hex-certificate-selector> `
  -TimestampUrl https://timestamp.example.org/ `
  -PreviousInstaller <previous-signed-WebCom-installer> `
  -PublisherDisplayName "<publisher name>" `
  -ProductWebsite https://example.org/webcom `
  -RepositoryUrl https://github.com/<owner>/<repo> `
  -SupportUrl https://example.org/webcom/support `
  -SecurityUrl https://example.org/webcom/security `
  -PrivacyUrl https://example.org/webcom/privacy `
  -ReleaseNotesUrl https://example.org/webcom/releases/3.0.0 `
  -LicenseMode OpenSource `
  -LicenseFile .\LICENSE
```

For the first signed WebCom release, replace `-PreviousInstaller ...` with `-NoPreviousSignedBuild`. Do not use that waiver after a signed public build exists.

The orchestrator:

1. builds/signs the exact Stable candidate,
2. runs the full real-Windows Release Candidate suite with the memory gate enabled,
3. requires machine-install acceptance,
4. runs upgrade compatibility or the explicit first-release waiver,
5. runs the public-release promotion gate,
6. creates a self-contained evidence archive,
7. writes `PUBLIC-RELEASE-RUN.json` and its SHA-256.

It deliberately **does not publish or upload anything**.

## Release evidence archive

`New-ReleaseEvidenceBundle.ps1` creates:

```text
WebCom-3.0.0-public-release-evidence.zip
```

The archive contains the exact approved installer, Stable manifest, approval record, RC report, acceptance reports, signed-dist verification, SBOM, provenance, source-manifest identity, dependency lock, public policy documents, and an internal `EVIDENCE-MANIFEST.json` with hashes and byte counts.

Every copied file is hashed again before compression. The evidence ZIP itself also receives a `.sha256` file.

Archive this bundle with the release even if it is not distributed to end users.

## After manual publication

Once the approved installer and Stable manifest are live at their intended HTTPS URLs, run:

```powershell
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File .\scripts\Test-PublishedRelease.ps1 `
  -PublicReleaseApproval .\release\PUBLIC-RELEASE.json `
  -StableManifestUrl https://downloads.example.org/webcom/stable.v2.txt
```

This downloads the **live** Stable manifest and installer and verifies that they are still the exact approved artifacts.

The check requires:

- HTTPS with no downgrade redirect,
- live Stable-manifest SHA-256 equals the approved manifest,
- manifest version/channel is the approved Stable version,
- live installer SHA-256 and byte count match the manifest,
- live installer SHA-256 matches `PUBLIC-RELEASE.json`,
- Authenticode remains valid,
- publisher certificate SHA-256 matches approval,
- product, repository, support, security, privacy, and release-notes URLs are reachable over HTTPS.

It writes:

```text
published-release-health.json
published-release-health.json.sha256
```

A local RC pass does not substitute for this post-publication check: hosting mistakes, stale CDN objects, or the wrong uploaded file can only be detected after publication.

## If the live check fails

Do not update the Stable manifest to point to an unapproved replacement. Restore the exact approved artifact or withdraw the release endpoint while the issue is investigated.

If a new binary is required, treat it as a new release candidate: rebuild, retest, approve, archive evidence, publish, and re-run the live health check.
## Release incident / withdrawal workflow

If a healthy local RC was published but the release later needs to be withdrawn, do not improvise a lower-version manifest or replace binaries without evidence. Use `docs/INCIDENT-RESPONSE.md`.

For a previously approved last-known-good release, validate it first:

```powershell
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File .\scripts\Test-RollbackCandidate.ps1 `
  -CurrentPublicReleaseApproval .\release-current\PUBLIC-RELEASE.json `
  -PreviousPublicReleaseApproval .\release-previous\PUBLIC-RELEASE.json
```

Then prepare either a withdrawal-only or last-known-good restore package with `Prepare-ReleaseWithdrawal.ps1`. The tooling generates checksummed incident evidence and manual actions only; it never edits hosting/CDN state automatically.

An older Stable manifest **cannot automatically downgrade machines already running the affected newer version**. Those clients require the verified in-product rollback path when available or a new higher-version hotfix that completes the full release chain.

