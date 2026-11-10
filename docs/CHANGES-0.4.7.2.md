# WebCom 0.4.7.2 SDK restore hotfix

- Fixes the WebView2 SDK restore failure caused by requesting a non-existent `*.nupkg.sha512` sibling URL from NuGet's flat-container API.
- Uses NuGet catalog metadata for SHA-512 verification when the registration/catalog endpoints are available.
- Does not block the build when catalog metadata is unavailable behind a proxy; the package is still downloaded over HTTPS and validated as a ZIP, checked for the exact `Microsoft.Web.WebView2` package ID and `1.0.4314.35` version, and checked for the required x64/ARM64 SDK files before use.
- Deletes a corrupt or structurally invalid cached package so the next build downloads a clean copy.
