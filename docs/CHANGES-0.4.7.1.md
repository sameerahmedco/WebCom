# WebCom 0.4.7.1 build hotfix

- Replaces `std::set::contains` calls introduced by 0.4.7 with `find`, avoiding failures on older MSVC standard-library revisions.
- `storage.h` now owns its `<cstdint>` dependency directly.
- WebView2 SDK restore verifies the package against NuGet's official `.sha512` endpoint instead of a brittle hard-coded checksum.
- `build.cmd` writes `build-last.log` on every run.
- `Build.ps1 -SkipUiTests` can isolate the browser executable from the optional Windows UI harness during diagnostics.
- CMake reports a clear compiler-floor error for unsupported MSVC versions.
