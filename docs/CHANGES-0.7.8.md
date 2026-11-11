# WebCom 0.7.8 changes

## Persistent unpacked source mapping
- Adds bounded per-profile `ExtensionSources/<profile>.v1` metadata.
- Atomic persistence, strict extension-ID validation, escaped fields and Unicode filesystem paths.
- One current extension ID is retained per source folder; a changed ID replaces the old folder owner.
- Successful unpacked installs remember the source path.
- Explicit extension removal forgets the source path.
- Profile deletion removes source metadata.

## Extension details and compatibility diagnostics
- Manifest preview now reports description, permissions, host permissions, background/service-worker usage, content scripts and options-page presence.
- Details keeps the Chrome toolbar/popup WebView2 compatibility warning.
- Manifest preview remains bounded to 2 MiB and WebView2 remains the authoritative installer/validator.

## Reload / development workflow
- Installed extensions with a saved source expose Reload and Open folder.
- Reload removes the installed copy first when necessary, then uses WebView2 `AddBrowserExtension` on the saved source.
- Known source folders that are no longer reported installed remain visible with Reload source/Open folder actions.
- This specifically handles the development case where changing unpacked extension files causes the runtime to drop the installed extension.
