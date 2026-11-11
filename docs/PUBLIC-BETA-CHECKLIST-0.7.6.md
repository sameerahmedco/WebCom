# WebCom 0.7.6 public-beta checklist

- [ ] Fresh MSVC build succeeds after MSVC/security preflight.
- [ ] Recovery & Reliability opens from Settings, Help and Quick Commands.
- [ ] Failed update checks enter 15/30/60/120/300-second bounded backoff.
- [ ] Retry state survives restart and clears on successful check.
- [ ] Channel change and disabling update checks clear retry state.
- [ ] Unclean previous shutdown is shown on Recovery & Reliability.
- [ ] Recovery report ZIP is generated only on explicit action and never uploaded automatically.
- [ ] `runtime.txt` is bounded and redacts HTTP/HTTPS/file URLs.
- [ ] Eligible rollback appears on both Updates and Recovery pages and still re-verifies package before UAC launch.
- [ ] Existing installer, Default Apps, local-file, profile, session and update-security smoke tests pass.
