# WebCom Security Policy

## Supported release

Security fixes are targeted at the current WebCom v3 line (`3.0.0`) while it is in Release Candidate / public-release hardening. Older preview archives are not supported security releases.

## Reporting a vulnerability

Please do **not** publish exploit details, credentials, private browsing data, or a working proof of concept in a public issue.

Use the repository's **GitHub Private Vulnerability Reporting / Security Advisory** flow when it is available. Include:

- the exact WebCom version/build hash,
- Windows version and architecture,
- WebView2 Runtime version,
- reproduction steps,
- security impact,
- whether the issue reproduces in a clean profile,
- minimal supporting files or screenshots with secrets removed.

If private vulnerability reporting is not enabled for the repository, contact the project owner privately before opening a public issue.

## Security boundaries

WebCom owns the Win32 browser shell, profile/state files, update verification, bundled blocker configuration, and release packaging. Website rendering, website process sandboxing, certificate validation, password storage/autofill implementation, and many web-platform behaviors are provided by the installed Microsoft Edge WebView2 Runtime.

WebCom does not claim compatibility with Chrome Sync or the full Chrome browser-extension platform.

## Release security expectations

Public binaries are expected to pass the repository's signed Release Candidate workflow. That workflow verifies the pinned WebView2 dependency, source manifest, PE architecture/version/security policy, Authenticode publisher identity, installer contents, fresh-install behavior, update metadata, and release provenance. A source ZIP or unsigned development package is not a security-approved public release.

## Disclosure

Please allow reasonable time for validation and a coordinated fix before public disclosure. This policy does not create a bug-bounty or payment commitment.
