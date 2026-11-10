# WebCom 0.4.1 branding update

Date: November 10, 2026 (Asia/Karachi).

Version 0.4.1 is intentionally limited to application branding. The approved
WebCom square artwork is the source of truth in `resources/webcom-icon.png`. A
multi-size `resources/webcom.ico` is generated from it for normal Windows builds.

For the portable preview binary, the 0.4.0 executable code is unchanged. Its
embedded icon resources, manifest version, version resource, and same-length
About version string were updated in place to 0.4.1. This avoids claiming a new
engine/UI build when only branding changed. The portable binary contains 16, 24,
32, 48, 64 and 256 pixel icon entries; Windows can scale the 256 pixel entry for
other high-DPI sizes. The source ICO contains additional sizes for future native
builds.

Portable C++ tests were rebuilt from the 0.4.1 source with the Windows UI disabled:
all seven CTest suites passed. The Windows executable itself was not launched in
this Linux environment.
