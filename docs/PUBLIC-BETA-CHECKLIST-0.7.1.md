# WebCom 0.7.1 public-beta checklist

- [ ] Fresh MSVC `build.cmd` succeeds.
- [ ] Native Windows UI tests pass.
- [ ] Signed installer builds and verifies publisher fingerprint.
- [ ] Administrative install registers HTTP/HTTPS candidate.
- [ ] `/CURRENTUSER` install registers per-user HTTP/HTTPS candidate.
- [ ] Windows Default Apps page lists WebCom.
- [ ] WebCom never changes the default without the user selecting it in Windows Settings.
- [ ] External HTTP/HTTPS activation opens/focuses WebCom correctly.
- [ ] Brand-new data root shows Getting Started once.
- [ ] Existing 0.7.0 user data is not interrupted by onboarding.
- [ ] Uninstall removes registration and leaves browser data policy unchanged.
- [ ] Stable/Beta update consent and rollback state smoke tests still pass.
