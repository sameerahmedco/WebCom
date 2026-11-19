# WebCom v3 public release gate

WebCom should be treated as **Release Candidate source** until the exact signed Windows installer passes the Release Candidate workflow on real Windows. Portable tests are necessary but cannot prove WebView2 runtime behavior, Authenticode, installer registration, DPI/accessibility, or real memory use.

## Source freeze requirements

- portable GCC and Clang test matrices pass,
- blocker/YouTube tests pass,
- source preflight passes,
- `docs/source-hashes.sha256` exactly covers the clean source tree,
- `dependencies.lock.json` is unchanged or intentionally reviewed,
- `SECURITY.md`, `PRIVACY.md`, third-party notices, and release documentation are present,
- no compiler/debug/generated artifacts are committed to the clean source set.

## Windows build requirements

Build from a fresh extraction with normal warnings-as-errors and verified WebView2 dependencies. The staged package must pass `Verify-BuildPackage.ps1`, including PE architecture/version/security flags and required release evidence.

The Windows acceptance run must pass the native UI harness, real WebView2 smoke launch, DPI/accessibility checks, profile/private-mode behavior, update/recovery behavior, and the approved memory budget.

## Signed release requirements

Use the signing/release scripts rather than manually assembling an installer. The final RC evidence must bind the exact source manifest and dependency verification to the signed staged `WebCom.exe`, signed installer, release manifest, SBOM, provenance report, and fresh-installed executable.

Do not publish an unsigned portable development ZIP as the official public release.

## Canonical three-phase release flow

### 1. Build the signed candidate

Use `scripts/Build-ReleaseCandidate.ps1`. It performs the release build with locked/officially verified WebView2 inputs, signs the staged executable/installer, generates the Stable manifest, SPDX SBOM, and provenance evidence, but **does not approve publication**. `Build-PublicBeta.ps1` is retained only as a compatibility implementation for older automation.

### 2. Test the exact signed candidate on real Windows

Run `scripts/Test-ReleaseCandidate.ps1` against that exact installer/manifest. For public-release sign-off do not use `-SkipMemoryGate`, and include `-RunMachineInstall` so both the default per-user path and the explicit elevated machine-install path are verified. Supply the previous signed installer for upgrade QA, or explicitly use `-NoPreviousSignedBuild` only when no prior signed release exists.

### 3. Promote the tested candidate

Run `scripts/Approve-PublicRelease.ps1` using the generated `release-candidate.json`, the exact installer/Stable manifest, publisher fingerprint, and real public HTTPS product/repository/support/security/privacy/release-notes URLs. The promotion gate requires:

- `readyForPublicReleaseReview=true`,
- the memory gate enabled with a working-set budget no looser than 300 MiB,
- signed package, supply-chain, Windows, release-matrix, fresh-install, and machine-install gates all passed,
- upgrade QA passed or explicitly waived only because no previous signed WebCom build exists,
- installer Authenticode publisher identity still matches the release pin,
- exact installer/manifest/provenance/SBOM/report hashes still match the RC evidence,
- an explicit `OpenSource` or `Proprietary` licensing decision; open-source publication requires a non-empty license file.

A successful promotion writes `PUBLIC-RELEASE.json`, `PUBLIC-RELEASE.json.sha256`, and `PUBLIC-RELEASE-SHA256SUMS.txt`. These files record approval evidence only; the script deliberately does not upload or publish artifacts.

## Human sign-off that code cannot automate

Before publication, the project owner should also confirm:

- final publisher/company identity shown in Windows metadata and signing certificate,
- support/contact path and GitHub private vulnerability reporting availability,
- distribution website/domain and HTTPS release-manifest URLs,
- software licensing terms if source code is made public,
- privacy/security text has been reviewed for the intended distribution jurisdiction,
- release notes and known limitations are accurate.

These are ownership/legal/operational decisions and cannot be proven by a source build alone.
## One-command validation and publication boundary

Latest-47 adds `scripts/Invoke-PublicRelease.ps1` as the canonical elevated Windows orchestration path. It performs the existing build, RC, promotion, and evidence stages without uploading anything. After the approved artifacts are manually published, run `scripts/Test-PublishedRelease.ps1` against the live Stable manifest. That separate check proves the hosted manifest/installer still match `PUBLIC-RELEASE.json` and that the documented public HTTPS endpoints are reachable. See `docs/POST-RELEASE.md`.

## Post-release incident readiness

Latest-48 adds a post-release incident layer without changing the normal release ceremony. `Test-RollbackCandidate.ps1` proves that a proposed last-known-good target is a previously approved Stable release with exact hashes, valid Authenticode, correct version ordering, and the same publisher certificate. `Prepare-ReleaseWithdrawal.ps1` then prepares a checksummed `WithdrawOnly` or `RestoreLastKnownGood` evidence bundle but never changes public hosting automatically. See `docs/INCIDENT-RESPONSE.md`.
