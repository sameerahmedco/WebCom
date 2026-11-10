# WebCom 0.5.1 validation

0.5.1 is based on the Windows-compiling WebCom lineage confirmed by the user. The portable storage/profile-removal behavior and all existing models have been tested here; the new WebView2 CookieManager and Profile8 Windows paths require a fresh Windows `build.cmd` and live acceptance run.

## Completed portable checks

- GCC C++20 configure/build: PASS, zero compiler warnings.
- Clang C++20 configure/build: PASS, zero compiler warnings.
- 10/10 CTest targets: PASS under both toolchains.
- 605 explicit regression checks: PASS.
- New storage coverage verifies case-insensitive profile removal, refusal to remove the final profile, active-profile fallback, and recovery-backup sanitization after profile deletion.
- Existing site-permission, history, bookmark, omnibox, tab-search, layout and recovery coverage remains green.
- Static MSVC-risk scan performed for the new Windows-only code paths; no new mixed-type `auto` declarator lists, Win32 `LONG`/`int` `std::max` calls, or known Windows macro-collision identifiers were introduced.

## Windows acceptance required

Run `build.cmd` in a fresh extracted source tree, then verify:

1. `WebCom.exe` and `WebCom-ui-tests.exe` compile with MSVC and the pinned WebView2 SDK.
2. Open an HTTPS page and confirm the shield shows **Connection is secure**. Open a plain HTTP page and confirm it is shown as unencrypted.
3. Visit a site that sets cookies; reopen site information and confirm the cookie count/details menu appears, does not expose cookie values, and **Clear cookies for this site** removes the site's cookies after reload.
4. Exercise a certificate-error page/test environment and confirm the shield reports a certificate problem rather than a secure state.
5. Create a second profile, then delete the current profile. Confirm WebView2 closes the deleted profile, another profile remains usable, deleted WebCom metadata is gone, and downloaded files remain.
6. Confirm WebCom refuses to delete the final remaining profile and refuses deletion while an active download belongs to that profile.
7. Restart WebCom and confirm the recovery backup cannot restore the deleted profile.
8. Re-run `Test-UI.cmd` plus normal tab/profile/download/site-permission smoke tests.

No claim of production sign-off is made until the Windows/WebView2 checks above pass.
