# WebCom v2 continuation plan

2.0.0 moves the Windows browser source into a stricter public-beta hardening phase. Portable policy and regression gates are in source; real Windows/MSVC/WebView2 acceptance remains release-decisive.

## Next acceptance and product layer
1. Run the full Windows/MSVC build and native UI harness with the new tab-strip accessibility provider linked through `oleacc` / `oleaut32`.
2. Exercise Narrator and at least one UI Automation inspection tool against horizontal and vertical tabs, including selection, tab close/reorder, workspace switching, Incognito labels, sleeping tabs, and high-contrast mode.
3. Run Stable/Beta signed package matrix tests on a clean VM, including beta-to-final and rejected Stable-prerelease cases.
4. Exercise rollback from a deliberately failed health window and verify every `RecoveryReadiness` failure mode is fail-closed.
5. Capture one-tab, five-tab, sleeping-tab, and two-profile memory runs with the 300 MiB one-tab private-commit target visible in QA artifacts; optimize only from measured regressions.
6. Re-run 0.7.9 unpacked-extension source-health acceptance and only add deeper background/service-worker diagnostics where public WebView2 APIs expose reliable information without keeping extra web processes alive.
7. After those Windows acceptance passes, produce the first signed 0.8.x beta candidate rather than treating this Linux source validation as release certification.
