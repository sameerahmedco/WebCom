# WebCom 0.7.2 changes

0.7.2 is the local-file and Windows file-association readiness milestone.

## Local HTML/PDF opening
- Ctrl+O opens a native file picker for `.html`, `.htm`, and `.pdf`.
- Absolute local-drive Windows paths are canonicalized to `file:///` URLs.
- Canonical file URLs for those supported extensions are accepted.
- PDF/HTML fragments are preserved; local-file query strings are rejected.
- UNC/network file URLs, traversal segments, malformed percent escapes, control characters and unsupported extensions are blocked.
- WebCom does not enable `--allow-file-access-from-files`.
- Local-file visits are excluded from browser History, bookmarks and website cookie/permission controls.

## File associations
- The Inno Setup installer adds `WebCom.HTML` and `WebCom.PDF` ProgIDs.
- Default Programs capabilities advertise `.html`, `.htm`, and `.pdf` as optional file associations.
- The user still chooses defaults through Windows Settings; installation never writes a user-choice hash.
- Fresh-install QA verifies the new capabilities, commands and uninstall cleanup.

## Persistence hardening
- Supported canonical local-file tabs can participate in normal session restore.
- The storage allow-list now covers current native management pages (Welcome, Shortcuts, Diagnostics, Updates, Sessions and Privacy) instead of only the older internal-page subset.
