# WebCom 0.7.4 public-beta checklist

- [ ] Fresh MSVC build and native UI tests pass.
- [ ] MSVC/security source preflight passes before native compile.
- [ ] Developer build has no configured update endpoint and sends no update traffic.
- [ ] Release build embeds only the intended Stable/Beta HTTPS endpoints and the production publisher fingerprint.
- [ ] Valid newer v2 manifest is offered; wrong channel/publisher/old version is rejected.
- [ ] Tampered package size/hash/product identity/signature/publisher all fail staging.
- [ ] Staging uses a `.part` file and commits only after every verification passes.
- [ ] Install action re-verifies and requires explicit confirmation + UAC.
- [ ] Canceling UAC reverts transaction to verified/staged state.
- [ ] New target version enters and clears startup health state correctly.
- [ ] Repeated interrupted health windows recommend rollback but never execute one automatically.
- [ ] Signed installer and v2 manifest pass the release scripts.
- [ ] Existing browser beta gates still pass.
