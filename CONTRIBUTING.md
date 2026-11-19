# Contributing to WebCom

WebCom v3 is a native C++20 / Win32 browser shell using Microsoft Edge WebView2.

## Before changing code

1. Read `HOW_TO_BUILD.md`, `docs/ARCHITECTURE.md`, `SECURITY.md`, and `PRIVACY.md`.
2. Keep public versioning on WebCom v3 / `3.0.0` unless the project owner explicitly changes the release line.
3. Do not weaken the source-manifest, dependency-lock, Authenticode, publisher-pin, update, privacy, or Release Candidate checks to make a build pass.
4. Do not add telemetry, automatic crash upload, silent software installation, silent default-browser changes, or credential-value access without explicit product/security review.

## Local validation

Portable changes should pass:

```text
cmake -S . -B build/core -DWEBCOM_BUILD_APP=OFF -DBUILD_TESTING=ON
cmake --build build/core --parallel
ctest --test-dir build/core --output-on-failure
node --test tests/ad_blocker_tests.cjs
```

Windows UI/WebView2 changes also require a fresh MSVC `build.cmd` run and the relevant Windows acceptance checks. Public binaries require the signed Release Candidate gate.

## Pull requests

Keep changes focused, add regression coverage for behavior changes, document security/privacy implications, and include the first failing compiler/test output when reporting a build regression.

Security vulnerabilities should follow `SECURITY.md`, not a public issue.
