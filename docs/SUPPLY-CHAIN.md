# WebCom v3 supply-chain and release provenance

Latest-45 adds an auditable dependency and release-provenance chain around the existing build/package/signing gates.

## Locked dependency input

`dependencies.lock.json` is the source-of-truth record for the WebView2 SDK used to build WebCom. It pins:

- NuGet package ID: `Microsoft.Web.WebView2`
- SDK version: `1.0.4314.35`
- official HTTPS flat-container package URL
- official NuGet registration/catalog sources used to obtain package hash metadata
- SHA-512 as the expected official package hash algorithm

The lock file itself is covered by `docs/source-hashes.sha256`, so changing the dependency input also changes the source identity approved for the release.

## Development vs release restore

Ordinary developer builds may continue when official NuGet catalog metadata is unavailable, provided the package still passes HTTPS download, ZIP safety, NuGet identity/version, and SDK-layout validation. An explicitly supplied `-SdkDir` is also allowed for development.

Public/signed builds are stricter. `Build-ReleaseCandidate.ps1` enables `-RequireVerifiedDependencies`, which causes `Restore-WebView2.ps1` to:

1. restore the package identified by `dependencies.lock.json`,
2. obtain the official SHA-512 from NuGet registration/catalog metadata,
3. compute the downloaded package SHA-512,
4. require the two values to match,
5. reject `-SdkDir` because an arbitrary extracted directory cannot prove the original NuGet package hash,
6. emit `webcom-dependency-verification-v1` evidence.

If official hash metadata cannot be obtained, the public-release dependency gate fails rather than silently weakening the release.

## Dependency verification evidence

The generated dependency report records:

- dependency-lock SHA-256,
- package ID and version,
- package source URL,
- whether official catalog hash verification succeeded,
- actual and official NuGet package SHA-512,
- package byte size,
- SHA-256 of `WebView2.h`,
- SHA-256 of x64 and ARM64 `WebView2Loader.dll`.

The staged Windows package carries both `docs/dependencies.lock.json` and `docs/DEPENDENCY-VERIFICATION.json`. `Verify-BuildPackage.ps1` rejects mismatched lock/report identity and requires official NuGet hash verification for release-ready packages.

## SPDX SBOM and provenance

After the signed installer and update manifest are produced, `New-ReleaseProvenance.ps1` creates:

- `webcom.spdx.json` — SPDX 2.3 SBOM containing WebCom and its locked Microsoft WebView2 SDK dependency,
- `release-provenance.json` — WebCom-specific release provenance.

The provenance report binds together:

- `docs/source-hashes.sha256` SHA-256,
- `dependencies.lock.json` SHA-256,
- verified WebView2 NuGet SHA-512,
- post-sign `signed-dist-verification.json` SHA-256,
- signed staged `WebCom.exe` SHA-256,
- signed installer SHA-256,
- release-manifest SHA-256,
- SPDX SBOM SHA-256.

The report stores artifact filenames and cryptographic identities rather than user-specific absolute filesystem paths.

## Release Candidate enforcement

`Test-ReleaseCandidate.ps1` requires `release-provenance.json` and `webcom.spdx.json` beside the signed installer. The gate independently recomputes the installer, release-manifest, signed-dist, source-manifest, dependency-lock, and SBOM hashes and rejects any mismatch.

It also checks that:

- the provenance schema is `webcom-release-provenance-v1`,
- official WebView2 package hash verification is present,
- the SBOM is SPDX 2.3,
- the SBOM contains exactly one WebCom package at the expected version,
- the SBOM contains the locked WebView2 version,
- signed staged executable identity still matches the installer-installed executable later in the RC flow.

## Release evidence set

Archive these together for a candidate that is approved on Windows:

```text
WebCom-Setup-3.0.0-x64.exe
stable.v2.txt
signed-dist-verification.json
release-provenance.json
webcom.spdx.json
release-candidate.json
windows/windows-acceptance.json
fresh-install.json
upgrade-install.json            # when a previous signed release exists
memory-one-tab.json             # when the memory gate is enabled
```

The existence of these files alone does not mean the candidate passed. `release-candidate.json` must report a successful real Windows Release Candidate run.
