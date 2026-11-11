# WebCom 0.5.2 validation

0.5.2 is based on the Windows-compiling WebCom lineage confirmed by the user. Portable persistence and model code has been tested here; the new WebView2 credential-data clearing and server-certificate-error paths require a fresh Windows `build.cmd` and live acceptance run.

## Completed portable checks

- GCC C++20 configure/build: PASS, zero compiler warnings.
- Clang C++20 configure/build: PASS, zero compiler warnings.
- 10/10 CTest targets: PASS under both toolchains.
- 607 explicit regression checks: PASS.
- Session v4 round-trip coverage verifies per-profile password-save and general-autofill preferences.
- Session v3 migration coverage verifies older profiles inherit the legacy password preference and default general autofill safely.
- Existing profile-removal, recovery, site-permission, history, bookmark, omnibox, tab-search and layout coverage remains green.
- Static MSVC-risk scan performed for the new native code; no new mixed-type Win32 `std::max` expressions or mixed-type `auto` declarator lists were introduced.

## Windows acceptance required

Run `build.cmd` in a fresh extracted source tree, then verify:

1. `WebCom.exe` and `WebCom-ui-tests.exe` compile with MSVC and the pinned WebView2 SDK.
2. Create or open two normal profiles and set different **Offer to save passwords** and **Autofill addresses and forms** choices; restart and confirm each profile keeps its own values.
3. In Incognito, confirm password autosave and general autofill remain disabled regardless of the normal profile preference.
4. With disposable test credentials/data, run **Clear saved passwords** and **Clear autofill form data** separately and confirm each action removes only its intended WebView2 profile data kind.
5. Exercise a TLS certificate-error test page/environment. Confirm site information shows the certificate problem and **Certificate details** includes the WebView2 verification error plus subject/issuer/validity metadata when supplied.
6. Confirm WebCom does not continue/bypass a TLS certificate error.
7. Re-test the v0.5.1 site-info cookie count/details/clear action and real profile deletion paths.
8. Re-run `Test-UI.cmd` plus normal tab/profile/download/site-permission smoke tests.

No claim of production sign-off is made until the Windows/WebView2 checks above pass.
