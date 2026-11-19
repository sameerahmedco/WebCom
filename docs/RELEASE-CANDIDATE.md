# WebCom v3 Release Candidate gate

Latest-39 adds a single Windows Release Candidate workflow that combines the source/runtime gate with signed-installer and signed-update-package checks. Latest-40 kept that gate unchanged while hardening Windows accessibility/DPI policy; Latest-41 keeps the same gate and adds MSAA semantics for painted native-page actions while preserving the standard Win32 client accessibility tree. It is designed to answer one question: **is this exact signed build ready for public-release review?**

This workflow must run on real Windows. Portable GCC/Clang validation cannot prove MSVC, WebView2, Authenticode, Inno Setup, Default Apps registration, installer upgrade, or Windows process behavior.

## Required artifacts

Prepare the signed current installer and its matching stable manifest first. The manifest/package must use the same publisher certificate pin configured for WebCom updates.

Recommended inputs:

- current signed `WebCom-Setup-3.0.0-x64.exe`,
- matching `stable.v2.txt`,
- publisher certificate SHA-256 fingerprint,
- previous signed WebCom installer when one is available,
- optional matching Beta manifest/package when validating both channels.

## Run the complete Release Candidate gate

From a freshly extracted WebCom v3 source folder in Windows PowerShell:

```powershell
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File .\scripts\Test-ReleaseCandidate.ps1 `
  -CurrentInstaller .\release\WebCom-Setup-3.0.0-x64.exe `
  -StableManifest .\release\stable.v2.txt `
  -ExpectedPublisherSha256 <64-hex-publisher-fingerprint> `
  -PreviousInstaller <path-to-previous-signed-WebCom-installer>
```

If WebCom has **never had a previous signed build**, say so explicitly with `-NoPreviousSignedBuild`. The gate fails if neither a previous installer nor that explicit waiver is supplied.

By default this also runs the one-tab memory gate with a **300 MiB working-set budget**. Use `-SkipMemoryGate` only for diagnosis; do not use it for final memory sign-off.

If an elevated machine-wide installer pass is required, add:

```powershell
-RunMachineInstall
```

## What the gate does

### 1. Clean Windows acceptance

Runs `Test-WindowsAcceptance.ps1` and requires:

- fresh Release build through `build.cmd`,
- successful MSVC source preflight,
- zero WebCom-project MSVC warnings,
- packaged FileVersion/ProductVersion `3.0.0`,
- packaged WebView2 loader and bundled blocker assets,
- packaged native UI harness success,
- isolated real `WebCom.exe` launch,
- observed descendant `msedgewebview2.exe`,
- one-tab memory report and budget pass unless explicitly skipped.

### 2. Signed release matrix

Runs `Test-ReleaseMatrix.ps1` and requires:

- stable manifest/channel policy,
- optional beta manifest/channel policy,
- HTTPS package URLs,
- package hash/size match,
- valid Authenticode signature,
- exact publisher certificate pin,
- installer ProductVersion matching the manifest.

The matrix now emits `webcom-release-matrix-v2` JSON for Release Candidate aggregation.

### 3. Signed fresh-install acceptance

`Test-FreshInstall.ps1` now does more than check file layout. It also requires:

- signed installer and signed installed `WebCom.exe`,
- installed ProductVersion/FileVersion match,
- Default Apps registration integrity,
- isolated launch of the **installed** browser,
- observed WebView2 runtime process,
- silent uninstall,
- removal of WebCom registration keys.

It emits `webcom-fresh-install-v2` JSON.

### 4. Previous-to-current upgrade acceptance

When `-PreviousInstaller` is supplied, `Test-UpgradeInstall.ps1`:

1. installs the previous signed package into a temporary test location,
2. launches that installed browser against an isolated QA profile,
3. requires a real WebView2 process and persisted `state.v1`,
4. installs the current signed package over the same installation,
5. confirms the installer did **not** modify the browser profile state before launch,
6. verifies the upgraded signed executable/version,
7. launches the upgraded browser against the same profile,
8. verifies the existing state remains readable,
9. uninstalls and checks Default Apps/ProgID cleanup.

The two installer packages must not be byte-identical. The upgrade gate emits `webcom-upgrade-install-v1` JSON.

## Output

A passing run creates an `acceptance/release-candidate-*` directory containing the component reports plus:

```text
release-candidate.json
```

The summary schema is:

```text
webcom-release-candidate-v1
```

It records the exact installer/manifest hashes and SHA-256 references to each component report so the signed build being approved is unambiguous.

## Important release rule

`Build-ReleaseCandidate.ps1` creates signed candidate artifacts but **does not approve publication**. After `Test-ReleaseCandidate.ps1` passes on real Windows, run `Approve-PublicRelease.ps1`; only its `PUBLIC-RELEASE.json` output marks the exact Stable artifacts approved for publication.
## Signed staged-package identity

Latest-44 requires `signed-dist-verification.json` beside the current installer. This report is produced after `WebCom.exe` is Authenticode-signed and records its SHA-256 and publisher identity. Fresh-install QA records the SHA-256 of the executable actually installed by Inno Setup; the RC gate requires those hashes to match before the candidate can pass. See `docs/BUILD-PACKAGE-INTEGRITY.md`.


## Supply-chain evidence

Latest-45 additionally requires `release-provenance.json` and `webcom.spdx.json` beside the signed installer. The RC gate verifies that provenance against the current source-manifest SHA-256, dependency-lock SHA-256, signed-dist report, installer, release manifest, and SBOM. Public builds must contain official NuGet SHA-512 evidence for the locked WebView2 SDK. See `docs/SUPPLY-CHAIN.md`.


The Release Candidate summary retains the legacy `readyForPublicBeta` field for old tooling and now also emits `readyForPublicReleaseReview=true` for the final promotion gate.
