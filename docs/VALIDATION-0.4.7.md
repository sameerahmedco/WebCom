# WebCom 0.4.7 validation

## Completed in this environment

- Portable CMake build completes successfully.
- **8/8 portable test suites pass: 560 checks, 0 failures.**
- Storage tests cover session format v3 tab-group name/color/collapsed persistence, invalid/duplicate group rejection, and v1/v2 backward compatibility.
- Chrome-layout tests cover compact pinned tabs and collapsed/expanded group visibility.
- Bookmark tests cover folder persistence, HTML import/export, in-folder reordering, order-preserving folder moves, and rejection of accidental cross-folder reorder operations.
- Core tests cover all native `webcom:` pages, including Profiles.
- Existing launch-request, tab-search, appearance and omnibox ranking suites remain green.
- Source review fixed collapse/select state contention, nested native-action hit priority, and bookmark folder moves that would otherwise destroy manual order.

## Not completed in this environment

A new Windows executable was **not** built or run here because the required Windows SDK/toolchain and pinned WebView2 build dependency are unavailable in this host. The following require a real Windows acceptance run:

- Live WebView2 browsing and renderer behavior.
- Group collapse/expand animation and tab-strip visual fidelity at 100%, 150% and 200% DPI.
- Bookmark drag/drop and History delete hit targets with mouse, touchpad and keyboard navigation.
- Cross-window and cross-monitor tab dragging between simultaneously running profiles.
- Profile isolation for cookies, cache, saved credentials and private contexts.
- Accessibility, IME, high-contrast/reduced-motion behavior and screen-reader navigation.
- Complete process-tree RAM comparison against Chrome using identical pages/workloads.
- Code signing, installer/update flow and clean-machine launch.

Do not rename an older executable as v0.4.7. Build this source with the documented Windows toolchain and run `Test-UI.cmd` plus the live acceptance matrix before distribution.
