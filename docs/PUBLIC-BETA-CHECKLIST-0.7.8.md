# WebCom 0.7.8 public-beta checklist

- [ ] Fresh MSVC `build.cmd` succeeds after the source preflight.
- [ ] Load unpacked Manifest V3 extension succeeds.
- [ ] Details reports permissions/host permissions/background/content scripts/options-page data accurately enough for a bounded preview.
- [ ] Open folder targets the recorded unpacked source.
- [ ] Reload replaces the installed copy and runs changed source.
- [ ] A source dropped by WebView2 remains visible under Known unpacked sources.
- [ ] Reload source restores a dropped extension.
- [ ] Restart preserves source mapping per profile.
- [ ] Explicit Remove deletes the source mapping.
- [ ] Source mappings do not leak between profiles.
- [ ] Profile deletion removes extension-source metadata.
- [ ] Chrome Web Store installation is not advertised as supported.
- [ ] Toolbar/icon/badge/popup limitations remain clearly disclosed.
- [ ] Existing updater, rollback, recovery, local-file and installer smoke tests still pass.
