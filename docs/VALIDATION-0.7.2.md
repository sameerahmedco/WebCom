# WebCom 0.7.2 validation

0.7.2 adds safe local HTML/PDF opening and optional Windows Default Apps file-association registration while retaining the 0.7.1 default-browser/onboarding infrastructure.

## Completed portable validation
- GCC C++20: 21/21 CTest targets pass with zero build warnings.
- Clang C++20: 21/21 CTest targets pass with zero build warnings.
- 815 explicit regression checks pass.
- Core address tests cover absolute Windows local paths, canonical `file:///` URLs, spaces, Unicode percent escapes, PDF fragments, literal `#` filenames, unsupported extensions, UNC/network paths, traversal segments, malformed escapes and local-file query rejection.
- Storage tests cover canonical local HTML session round-trip plus current native management-page persistence.
- Static Windows-source audit confirms the only `get_CookieManager` call remains behind `ICoreWebView2_2`.
- The new source does not reintroduce the prior MSVC mixed-`auto` declarator failure pattern in the changed local-file code.

## Windows acceptance required
1. Build with `build.cmd` from a fresh extracted source tree.
2. Press Ctrl+O and open local `.html`, `.htm` and `.pdf` files from a local drive.
3. Confirm local HTML relative CSS/script/image resources render without enabling `--allow-file-access-from-files`.
4. Confirm local PDF navigation/fragment behavior works and Print/Find/Capture remain available.
5. Confirm an unsupported local file such as `.txt` or `.exe`, a UNC/network file URL, and a traversal path are rejected.
6. Close/restart with a supported local file tab open and confirm normal session restore.
7. Build/install the signed Inno package in `/CURRENTUSER` mode and confirm Windows Default Apps lists WebCom for HTTP, HTTPS, `.html`, `.htm` and `.pdf`.
8. Select WebCom manually for one supported file type, open a file from Explorer, and confirm the bounded single-instance launch path opens it in WebCom.
9. Uninstall and confirm `WebCom.Url`, `WebCom.HTML`, `WebCom.PDF`, `Applications\WebCom.exe`, RegisteredApplications and capability entries are removed.
10. Repeat the installer association QA in elevated `/ALLUSERS` mode.

The native Windows UI, WebView2 local-file rendering, Inno Setup registration and Explorer activation paths cannot be executed in this Linux validation host; Windows acceptance remains decisive.
