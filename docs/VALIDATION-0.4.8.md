# WebCom 0.4.8 validation

0.4.8 is based on 0.4.7.4, which the user confirmed compiles successfully with MSVC on Windows after the SDK, COM-header and MSVC-source hotfixes.

Portable validation performed for the 0.4.8 tree:

- GCC C++20: 8/8 CTest suites passed, zero compiler warnings.
- Clang C++20: 8/8 CTest suites passed, zero compiler warnings.
- Total portable assertions/checks: 562, zero failures.
- Storage regression coverage now includes creation of a recovery generation and automatic fallback from a corrupt primary session.

Windows-only acceptance still required for 0.4.8:

- compile `WebCom.exe` and `WebCom-ui-tests.exe` with MSVC using `build.cmd`;
- pause/resume/cancel a real WebView2 download and verify state transitions;
- resume an interrupted live download where `CanResume` is true;
- verify a paused download remains visible after Clear completed;
- corrupt a copy of `state.v1` in an isolated test profile and verify the recovery toast and restored backup session;
- repeat tab/profile/DPI/smoothness acceptance checks from the existing validation matrix.
