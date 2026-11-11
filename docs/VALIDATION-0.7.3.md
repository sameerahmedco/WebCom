# WebCom 0.7.3 validation

0.7.3 adds an automated MSVC source-preflight gate and Windows header smoke translation unit while preserving the 0.7.2 feature set.

## Completed portable validation
- GCC C++20: 22/22 CTest targets pass with zero build warnings.
- Clang C++20: 22/22 CTest targets pass with zero build warnings.
- 884 explicit checks pass across the portable/model/preflight executables.
- `webcom_source_preflight` performs 69 MSVC-oriented source checks on the exact release tree and reports zero findings.
- The preflight verifies the only CookieManager call remains behind `ICoreWebView2_2`.
- Direct WebView2/GDI+ include ordering is checked against `win32_com_compat.h`.
- Mixed-auto, duplicate `internal_page`, and RECT/DPI min/max hazard rules pass.
- GCC/Clang emit zero warnings for the portable build.

## Windows acceptance required
1. Extract 0.7.3 into a fresh folder and run `build.cmd`.
2. Confirm `build-last.log` shows `Running MSVC source preflight before compiling the browser...` followed by a zero-failure preflight result.
3. Confirm `windows_header_smoke.cpp` compiles for both `WebCom.exe` and `WebCom-ui-tests.exe`.
4. Confirm the full MSVC browser and UI-test targets compile with `/W4 /permissive-`.
5. Run the complete CTest/native UI suite.
6. Re-run HTTP/HTTPS and HTML/PDF Default Apps/file-association QA from 0.7.2.
7. Re-run cookies/site info/profile deletion, startup/recovery, screenshots, Reader, split view, profiles/workspaces and memory smoke tests.

A Windows executable was not built in this Linux validation host; the MSVC run remains decisive.
