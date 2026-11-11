# WebCom 0.7.1 validation

0.7.1 adds Windows Default Apps registration and a one-time native Getting Started experience while retaining the public-beta update/installer infrastructure from 0.7.0.

## Completed portable validation
- GCC C++20: 21/21 CTest targets pass with zero build warnings.
- Clang C++20: 21/21 CTest targets pass with zero build warnings.
- 794 explicit regression checks pass.
- New onboarding-state tests cover missing state, atomic round-trip, reset, unknown schema and invalid boolean values.
- Core address tests now cover Welcome, Diagnostics, Updates and Privacy internal pages.

## Windows acceptance required
1. Build with `build.cmd` from a fresh extracted source tree.
2. Build/install with Inno Setup in normal administrative mode and confirm WebCom appears under Windows Default Apps for HTTP and HTTPS.
3. Repeat installer QA with `/CURRENTUSER` and confirm per-user registration works.
4. Verify install/uninstall refreshes association choices and uninstall removes WebCom registration without changing the user's selected replacement default.
5. On a brand-new WebCom data directory, confirm the native Getting Started page appears once and does not return after restart.
6. On an existing v0.7.0 data directory, confirm upgrade does not inject the welcome page.
7. Use **Set WebCom as default** and confirm WebCom only opens Windows Settings; the user must make the final selection.
8. After selecting WebCom for HTTP and HTTPS in Windows Settings, reopen Getting Started/Settings and confirm WebCom reports itself as the current handler.
9. Launch an external HTTPS link from another application and confirm it reaches the running WebCom process through the existing bounded launch-request IPC path.

The installer/default-app and native Windows UI paths cannot be executed in this Linux validation host; Windows acceptance remains decisive.
