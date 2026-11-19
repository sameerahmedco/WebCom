# WebCom v3 build-package integrity

Latest-44 adds a Windows package-integrity gate between compilation and signing. The goal is to prevent a successful compiler run from turning the wrong architecture, stale source tree, debug artifact, incomplete blocker payload, or mismatched executable into a distributable release.

## Staged package verification

`scripts/Verify-BuildPackage.ps1` validates a `dist\WebCom*` directory and emits the versioned report schema:

```text
webcom-build-package-v1
```

The verifier checks:

- `WebCom.exe` and `WebView2Loader.dll` are real PE files and have the requested x64/ARM64 machine type;
- `WebCom-ui-tests.exe`, when required, matches that same architecture;
- ProductVersion, FileVersion, ProductName, and FileDescription match WebCom v3 expectations;
- the bundled Ad Blocker contains the expected Manifest V3 payload;
- compiler/debug artifacts such as PDB, OBJ, LIB, ILK, EXP, PCH, TLOG, and temporary files did not leak into the staged package;
- packaged `BUILD-INFO.txt` identifies the expected architecture/configuration and, when required, a passed test run;
- every clean source file is represented by `docs/source-hashes.sha256`, every recorded hash still matches, and the manifest does not contain stale paths.

The source-tree comparison excludes only generated/build destinations such as `build*`, `dist`, `release`, `.deps`, acceptance output, Git metadata, and `build-last.log`.

## Release-ready build policy

A package checked with `-RequireReleaseReady` must additionally prove that:

- it is a Release build;
- the native UI harness was built;
- tests are recorded as passed;
- MSVC warnings-as-errors were enabled;
- a compact HTTPS Stable update-manifest endpoint was compiled into the build configuration;
- the compiled publisher SHA-256 pin matches the publisher identity expected for the release.

This is still a **pre-sign build policy**, not permission to publish.

## Signed executable identity

`Build-SignedInstaller.ps1` runs the package verifier twice:

1. **before signing**, to reject an invalid staged build before certificate operations;
2. **after signing `WebCom.exe`**, with `-RequireSignedExecutable`.

The post-sign pass validates Authenticode status and the exact signer certificate SHA-256, then writes:

```text
signed-dist-verification.json
```

That report includes the signed `WebCom.exe` SHA-256.

## Installer identity binding

`Test-FreshInstall.ps1` records the SHA-256 of the installed `WebCom.exe` before uninstalling the isolated test installation.

`Test-ReleaseCandidate.ps1` requires `signed-dist-verification.json` beside the signed installer and fails if:

- the report was not produced under release-ready + signed-executable policy;
- the publisher pin differs;
- source/UI evidence is missing;
- the `WebCom.exe` installed by the signed installer has a different SHA-256 from the signed staged executable.

This prevents a stale or differently assembled installer from passing solely because its version string and signature are otherwise valid.

## Portable ZIP boundary

`Build.ps1` also creates `dist\WebCom-v3-Windows-<arch>.zip`. Treat that ZIP as a **development/test artifact**. Public distribution uses the Authenticode-signed installer produced by `Build-SignedInstaller.ps1` / `Build-ReleaseCandidate.ps1` and approved by the Release Candidate gate.

## Windows requirement

The package verifier reads Windows PE/version resources and Authenticode state, so the full integrity chain is Windows-only. Portable GCC/Clang validation checks the source wiring and policy guards but cannot substitute for the real Windows run.

## Locked dependency evidence

Latest-45 extends the integrity gate to dependency inputs. Every staged package carries the source `dependencies.lock.json` plus `DEPENDENCY-VERIFICATION.json`. Release-ready verification requires official NuGet catalog SHA-512 proof for the locked WebView2 package and rejects a staged lock/report mismatch. See `docs/SUPPLY-CHAIN.md`.
