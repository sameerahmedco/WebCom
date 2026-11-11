# WebCom 0.7.3 public-beta checklist

## MSVC build gate
- [ ] `build.cmd` starts the source preflight before compiling WebCom.
- [ ] Source preflight reports zero failures.
- [ ] `windows_header_smoke.cpp` compiles under the installed Windows SDK/MSVC toolset.
- [ ] `WebCom.exe` and `WebCom-ui-tests.exe` compile with `/W4 /permissive-`.
- [ ] Full CTest/native UI harness passes.

## Regression matrix
- [ ] Cookie/site-info paths work and CookieManager is still versioned through `ICoreWebView2_2`.
- [ ] Tabs, vertical tabs, split view, Workspaces and multiple profiles work.
- [ ] Password/autofill, downloads, history, bookmarks, Reader, capture/annotation and local HTML/PDF files work.
- [ ] HTTP/HTTPS/.html/.htm/.pdf Default Apps registration and uninstall cleanup pass.
- [ ] Signed installer and publisher-fingerprint checks pass.
- [ ] Startup/recovery and memory/performance smoke tests pass.
