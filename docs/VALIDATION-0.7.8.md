# WebCom 0.7.8 validation

0.7.8 deepens the WebView2 unpacked-extension manager while retaining the secure updater, recovery, local-file and Windows build-preflight layers.

## Completed portable validation
- GCC C++20: 29/29 CTest targets pass with zero build warnings.
- Clang C++20: 29/29 CTest targets pass with zero build warnings.
- 909 portable regression checks pass.
- 143 MSVC/WebView2 source-preflight checks pass.
- 1,052 explicit checks total, zero failures.
- Extension manifest tests cover identity, description, permissions, host permissions, background/service worker, content scripts, options page and toolbar/popup warnings.
- Extension-source tests cover bounded IDs, source update, source-folder ID replacement, atomic round-trip, explicit removal and fail-closed schema/input handling.
- Preflight now requires bounded/atomic per-profile source metadata plus source remember/forget, Details, Reload, Open Folder and profile-deletion cleanup paths.

## Windows acceptance required
1. Fresh-extract 0.7.8 and run `build.cmd`.
2. Load a simple Manifest V3 unpacked content-script extension.
3. Confirm Details shows its declared permissions/host permissions/features and source folder.
4. Confirm Open folder opens the original unpacked directory.
5. Edit the source and use Reload; confirm the installed copy is replaced and runs the changed code.
6. Modify files in a way that makes WebView2 stop reporting the installed extension; refresh Extensions and confirm its source remains under **Known unpacked sources not currently installed**.
7. Use Reload source from that section and confirm the extension returns to the installed list.
8. Restart WebCom and confirm the source mapping persists for that profile only.
9. Remove the extension explicitly and confirm its source mapping disappears.
10. Repeat with two profiles and confirm source mappings remain isolated.
11. Delete a profile containing extension source metadata and confirm the corresponding `ExtensionSources` file is removed.
12. Re-test enable/disable/remove, browser-engine recovery, extension bootstrap behavior and the existing toolbar/popup compatibility warning.

The WebView2 extension COM path cannot be executed in this Linux validation host; Windows/MSVC/WebView2 acceptance remains decisive.
