# WebCom 0.7.7 public-beta checklist

## Build
- [ ] Fresh MSVC `build.cmd` succeeds.
- [ ] Source preflight succeeds before native compilation.
- [ ] Native UI harness still passes.

## Extensions
- [ ] Extensions page initializes with no website previously opened.
- [ ] Load unpacked Manifest V3 content-script extension.
- [ ] Restart persistence verified.
- [ ] Disable/enable behavior verified.
- [ ] Remove behavior verified.
- [ ] Per-profile isolation verified.
- [ ] Toolbar/action-popup compatibility warning verified.
- [ ] Invalid/missing manifest rejected.
- [ ] Profile deletion with installed extensions succeeds.
- [ ] WebView2 engine recovery followed by Refresh Extensions succeeds.

## Existing beta gates
- [ ] Secure update check/staging/install smoke tests still pass.
- [ ] Recovery/rollback/retry/reliability-report flows still pass.
- [ ] HTTP/HTTPS/local-file activation and Default Apps registration still pass.
