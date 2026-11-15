# MSVC build-regression notes

Latest-42 is driven by real Windows build logs from the older WebCom v0.8/v2 source line. Those logs showed that the WebView2 SDK restore and MSVC source preflight could succeed, yet the main build could still fail later in Windows-only translation units.

## Failure classes seen in the Windows logs

### Screenshot annotation editor

MSVC rejected two `std::clamp` calls where Win32 `POINT`/`RECT` values (`LONG`) were mixed with integer literals. The compiler could not choose a single template type.

The same source also passed raw `LONG` coordinates to `Gdiplus::Graphics::DrawLine`, where both integer and floating-point overloads were viable.

Current v3 requires:

- `std::clamp<LONG>` for Win32 point/rectangle clamping;
- normalization to `int` before ordinary integer image-space clamping;
- explicit `static_cast<INT>` coordinates for GDI+ line drawing.

The source preflight now includes both positive checks for these forms and negative checks that reject the old ambiguous patterns.

### Native Windows UI test harness

The older harness failed around `StartupMode::Continue`. Even after qualifying the enum, a positional `SavedProfile` aggregate remained fragile because adding/reordering fields in `SavedProfile` can shift initializer meanings and make MSVC diagnostics misleading.

Latest-42 replaces that positional seed with field-explicit helpers:

- `SeedNewTab(...)` assigns the fields it needs;
- `SeedProfile(...)` assigns profile name, tabs, credentials preferences, workspace, vertical-tab flags and `webcom::StartupMode::Continue` explicitly.

The source preflight rejects the old positional startup-mode aggregate pattern.

### Workspace command identifiers

An older build used command identifiers named `RenameWorkspace` / `DeleteWorkspace`, colliding with member-function names in `BrowserApp::Impl`. Current v3 uses `RenameWorkspaceCmd` / `DeleteWorkspaceCmd`, and the preflight rejects the old collision pattern.

## Why this matters

The purpose of these checks is not to claim that Linux portable builds substitute for Windows/MSVC. It is to make known Windows-only failures fail **before** the expensive main browser compile whenever their source patterns reappear.

A real Windows `build.cmd` run remains the authoritative check. If MSVC reports a new error class, add a minimal source fix plus a preflight guard only when the pattern is specific and reliable enough to prevent recurrence.
## Latest-43 warning policy

Known fatal patterns are still guarded by source preflight, but new WebCom-owned compiler warnings should not wait for a later acceptance log. Native WebCom and the Windows UI harness now use `/W4 /WX` by default. Angle-bracket SDK/system headers are treated as external at warning level 0 so the policy targets repository code rather than third-party implementation details.

`build.cmd -AllowCompilerWarnings` is intentionally diagnostic-only. It disables native `/WX` for investigation, emits a visible warning, and is forwarded by `Test-WindowsAcceptance.ps1` only when that runner is also explicitly told to allow warnings. The normal Release Candidate path keeps both compile-time `/WX` and the post-build repository warning scan.

