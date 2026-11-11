# WebCom 0.7.2 public-beta checklist

## Build and regression
- [ ] Fresh `build.cmd` succeeds with MSVC.
- [ ] Native UI regression harness passes.
- [ ] `dist\WebCom\WebCom.exe` starts with the pinned WebView2 loader beside it.

## Local-file behavior
- [ ] Ctrl+O opens local `.html`, `.htm` and `.pdf`.
- [ ] Absolute Windows path launch works from `WebCom.exe "C:\path\file.html"`.
- [ ] Canonical `file:///C:/...` launch works.
- [ ] PDF `#page=` fragments work.
- [ ] `.txt`, `.exe`, UNC/network paths and traversal attempts are rejected.
- [ ] Local-file tabs do not enter WebCom History or bookmark storage.
- [ ] Site-info identifies local files without website cookie/permission controls.
- [ ] Local HTML resources work without `--allow-file-access-from-files`.

## Installer / Default Apps
- [ ] Signed `/CURRENTUSER` install registers HTTP/HTTPS plus `.html`/`.htm`/`.pdf` capabilities.
- [ ] `WebCom.Url`, `WebCom.HTML`, and `WebCom.PDF` open commands point to the installed signed executable.
- [ ] Windows Settings remains the only place that changes actual defaults.
- [ ] Explorer activation of an associated HTML/PDF file reaches WebCom.
- [ ] Uninstall removes all WebCom URL/file ProgIDs and Applications registration.
- [ ] Elevated `/ALLUSERS` QA passes as well.

## Existing beta gates
- [ ] Signed installer publisher fingerprint matches the pinned release fingerprint.
- [ ] Fresh install and silent uninstall QA pass.
- [ ] HTTP/HTTPS external link activation still works.
- [ ] Profiles, Workspaces, session recovery, downloads, site controls and update-consent state pass smoke tests.
