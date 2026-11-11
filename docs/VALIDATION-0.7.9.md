# WebCom 0.7.9 validation

0.7.9 deepens the WebView2 unpacked-extension developer workflow while retaining the existing updater, recovery, local-file and Windows build-preflight layers.

## Completed portable validation
- GCC C++20: 29/29 CTest targets pass.
- Clang 17 C++20: 29/29 CTest targets pass.
- 926 portable behavioral/regression checks pass.
- 150 MSVC/WebView2 source-preflight checks pass.
- 1,076 explicit checks total, zero failures.
- Extension manifest tests now cover identity, description, minimum Chromium version, required and optional permissions/host permissions, background/service worker, content scripts, options page, commands, omnibox, DevTools page, web-accessible resources, side-panel declarations and toolbar/popup compatibility warnings.
- Extension-source tests now cover source-stamp validation, deterministic source snapshots, file-add/remove change detection, v1-to-v2 metadata migration, bounded IDs/stamps, atomic round-trip, folder ownership replacement, explicit forget and fail-closed malformed/missing inputs.
- Source preflight requires bounded source scanning, v1/v2 metadata handling, source-health refresh, Details/Reload/Open Folder/Forget Source actions and profile-deletion cleanup paths.

## Source-change stamp scope
The source stamp is intentionally a lightweight developer signal. It scans at most 4,096 regular files and incorporates relative path, file size and filesystem modification time into a deterministic 64-bit stamp. It is not a cryptographic content hash, malware scan, signature check or trust decision. Files with deliberately preserved metadata can evade this convenience signal.

## Windows acceptance required
1. Fresh-extract 0.7.9 and run `build.cmd`.
2. Load a simple Manifest V3 unpacked content-script extension.
3. Confirm Details reports the expected manifest capabilities and source scan file/byte counts.
4. Confirm a successful first load/reload establishes a source baseline.
5. Restart WebCom and confirm the source mapping/baseline persists only in that profile.
6. Modify/add/remove a normal source file, refresh Extensions and confirm the source is marked **changed / reload recommended**.
7. Use Reload and confirm the changed extension is re-added through WebView2 and the source returns to **unchanged**.
8. Confirm Open folder opens the original unpacked source directory.
9. Confirm a v1 extension-source metadata file is accepted and upgrades to schema v2 on the next save without losing its source path.
10. Exercise a missing/unreadable source folder and confirm WebCom reports scan unavailable instead of crashing or deleting metadata.
11. When a saved source is no longer reported installed by WebView2, confirm it remains under **Known unpacked sources not currently installed**.
12. Use Details in that section and verify the compatibility/source-health information is useful and bounded.
13. Use **Forget source** and confirm only WebCom's source mapping is removed: the source folder stays on disk and an installed extension is not removed.
14. Re-add/reload the forgotten source and confirm mapping/stamp storage is restored.
15. Repeat with two profiles and confirm extension-source metadata never crosses profile boundaries.
16. Delete a profile containing extension-source metadata and confirm its corresponding metadata is cleaned up.
17. Re-test extension enable/disable/remove, browser-engine recovery and extension bootstrap behavior.
18. Confirm direct Chrome Web Store install and Chrome-owned toolbar/icon/badge/popup UI are not advertised as supported.

The WebView2 extension COM path cannot be executed in this Linux validation host; a real Windows/MSVC/WebView2 run remains release-decisive for these runtime behaviors.
