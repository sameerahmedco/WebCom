# WebCom 0.4.0 architecture

## Selected stack

The application is C++20 with a Win32/GDI/GDI+ interface and Microsoft Edge WebView2
for web content. C++ is used for direct, predictable ownership of native UI and
browser-controller objects. The choice does not imply that C++ universally
uses less memory than every other compiled systems language.

The interface, native new-tab pages and dialogs use Windows controls, GDI and GDI+.
WebView2 is instantiated only for a real web page or a user-initiated popup.
This follows Microsoft's recommendation to avoid a web renderer for initial
native UI. Source: [WebView2 performance](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/performance).

The SDK is pinned to `Microsoft.Web.WebView2` **1.0.4314.35**. The restore script
checks the package SHA512 against a digest verified from the NuGet catalog.
The separate installed Evergreen Runtime receives Microsoft's updates; this
application does not bundle an obsolete fixed Chromium engine. Sources:
[NuGet package](https://www.nuget.org/packages/Microsoft.Web.WebView2/1.0.4314.35),
[runtime distribution](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/distribution).

## Native interface and page tools

The shell is drawn from the supplied dark Chrome reference. At 96 DPI the
caption/tab region is 40 pixels high, the navigation toolbar 44 pixels, and the
optional bookmarks row 37 pixels. Web content begins at y=121 with the bar shown
or y=84 with it hidden. `D()` scales native
geometry for the window's DPI. The custom frame handles caption dragging,
resize hit tests and minimize/maximize/close controls. Native tabs are drawn
with curved selected-tab shoulders and use explicit hit rectangles.

`chrome_layout.h` computes tab-strip and omnibox geometry without Win32
dependencies. The tab-strip layout retains the active tab in the visible range
and reserves room for new-tab, tab-search and window-caption actions. The
omnibox keeps its field inset and text origin stable across focus changes; an
unfocused first click selects the current text. The bookmark star is available
for web pages and hidden on native new-tab pages.

`native_ui.inl` holds layout, painting and the bookmark UI. It is
included after `BrowserApp::Impl` so the implementation remains in one C++
translation unit. The omnibox and normal new-tab search field are native edit
controls; toolbar actions use owner-drawn buttons. Short notices use a temporary
overlay instead of reserving a permanent status bar. The normal new-tab page
renders the bundled Google logo and launches Google search through the common
URL resolver. AI Mode is an ordinary link to Google's website.

`render_helpers.h` owns reusable DIB paint buffers for the frame, tab strip and
buttons. Painting copies completed dirty regions to the destination without
allocating a new bitmap on every frame. Layout computes each child control's
final geometry and visibility, batches changes with deferred positioning and
skips unchanged controls. An already-hidden control is not briefly shown by
the layout pass. Buffers and cached geometry are invalidated when their size
or DPI requires it.

WebView event callbacks queue UI work through a 16-millisecond timer. Address
updates carry the originating active-tab ID and check it again before updating
the edit control, protecting text entered while callbacks are pending. Native
tab selection remains immediate. Controller visibility and bounds are changed
only when needed, and the native layout does not apply compositing styles to
the WebView child. These are implementation changes intended to reduce flicker;
they are not a measured frame-rate or Windows visual-acceptance result.

The Ctrl+Shift+A picker uses `tab_search.cpp` for its native dialog and the
portable `tab_search_model.cpp` for title/address matching and ranking. Results
carry stable tab IDs; selection resolves the live tab after the dialog returns.
The picker covers the current profile window. Tab reordering moves the owned
tab record and keeps selection attached to its ID. Mute/unmute uses
`ICoreWebView2_8`; mute state is tracked during the live session and is not added
to the saved session-file format.

Mouse reordering captures the tab strip and tracks the dragged tab by stable
ID. Movement beyond the drag threshold sets an insertion target; dropping
moves the owned tab record once. Escape, capture loss and cancellation clear
the gesture. The close button records the pressed tab ID and closes only when
released over that same close target. WebView focus handoff is deferred while
the tab strip owns capture so selecting a tab does not cancel a drag.

`dark_menu.cpp` implements the native popup used for application, profile,
settings and tab menus. It supplies DPI-scaled rows, a highlighted profile row,
color swatches, aligned shortcuts, compound zoom buttons, keyboard navigation
and scrolling when necessary. The tab picker and menu both take a snapshot of
the owner's palette; all popup fields, hover states, dividers and focus rings
use it. Buffers are reused and drawing is clipped to the client area. Menu
selections are dispatched only after the popup returns. Menus that refer to
mutable tabs/bookmarks retain stable identifiers or snapshots across the nested
message loop, since page callbacks may run while the popup is open.

Page zoom calls `ICoreWebView2Controller::put_ZoomFactor`; it does not scale the
native new-tab layout. Print uses `ICoreWebView2_16::ShowPrintUI`. The menu's Find
action obtains `ICoreWebView2Find` through `ICoreWebView2_28` and creates its
options through `ICoreWebView2Environment15`; an older runtime receives update
guidance. Ctrl+F invokes the API from a native control and passes through to
the runtime when the web page has focus. Developer tools use
`OpenDevToolsWindow`. These are runtime integrations awaiting Windows acceptance
verification, not claims of a bundled Chrome feature set.

The Incognito page is native too, and explains the actual private-tab behavior
without asserting an unconfigured third-party-cookie policy. Ctrl+Shift+N and
the `--private` command-line option select InPrivate mode. Ctrl+T deliberately
opens a normal tab; command-line handoff preserves an explicit private request.
When available content height is below 606 DIPs, the private page reduces its
top spacing and icon size to keep the content usable in shorter windows.

## Per-profile appearance and native images

`appearance.cpp` defines Graphite, Charcoal, Midnight and Forest palettes using
portable 24-bit RGB values. Each `Impl` owns its loaded `Appearance`, derived
`Palette`, native brushes and profile badge colors. There are no mutable global
theme colors, so simultaneous windows can use different themes. Applying a
change refreshes edit brushes and paint buffers, recalculates layout and
updates existing controllers' default background color. Pages still receive a
dark color-scheme preference; the shell does not replace website styling.

`appearance_ui.inl` implements the native Appearance menus, eight preset
profile colors, automatic profile colors and a six-digit hex-color dialog.
The badge displays the chosen RGB color exactly with black or white lettering.
Focus and selection accents are lightened when necessary to keep at least a
3:1 contrast against each palette's relevant dark surfaces. This numeric check
does not substitute for a Windows accessibility review. Ctrl+Shift+B changes
the profile's saved bookmarks-bar visibility. Preference writes complete before
the UI applies them; a failed write leaves the current appearance unchanged.

Website favicons come from `ICoreWebView2_15::GetFavicon` in PNG form, using the
icon already available to WebView2. WebCom does not perform a separate request
to a favicon URL. `favicon_image.h` reads at most 256 KiB plus one overflow byte,
checks the PNG header and limits dimensions to 512×512 before decoding. It then
retains a separate 32×32 premultiplied bitmap, 4 KiB of pixel data per icon plus
object overhead; the original image and stream are released. Decode failure
falls back to a native icon.

Favicon callbacks carry a weak owner, tab ID, controller generation and request
number. Navigation and controller release invalidate older requests; returning
to the native new-tab page clears the old site icon. No thumbnail cache or
app-owned persistent favicon database is added. Every retained GDI+ image is
destroyed before the window's GDI+ shutdown. The WebCom application icon is
embedded from `resources/webcom.ico`; `webcom-icon.svg` is its source artwork.

## Ownership and processes

`BrowserApp` holds a shared `Session` for the life of the Win32 message loop.
The session owns one `AppState`, the live-window registry and one lazily created
`ICoreWebView2Environment`. Each profile window has its own shared `Impl`, a
weak reference to the session and an immutable profile name. The window owns
its controls, tab metadata and controllers. Each loaded tab owns one controller
and one WebView. Controllers for a named profile use that local profile within
the common user-data folder.

There is one window per named profile. Opening another profile creates or
focuses that window without tearing down the original window's tabs. Names are
matched case-insensitively. `AppState::active_profile` records the last-focused
profile for the next startup; it does not determine another window's identity.
Keyboard dispatch finds the window that owns the focused/message HWND. Ctrl+N
opens the profile picker. A fresh process opens the saved last-focused profile;
the other saved profiles remain available on demand.

Window procedures and subclass callbacks retain an `Impl` for their duration,
including nested modal loops. On `WM_NCDESTROY`, the top-level procedure clears
the HWND association before removing the window from the session registry.
Closing a window therefore leaves other windows running; only removing the last
window posts `WM_QUIT`. Session teardown also releases any remaining windows
and controllers if startup or the message loop exits through an error.

`WM_CLOSE` has no confirmation path in 0.4. It makes a best-effort `Save(false)`
call, marks the window closing, cancels tab capture, stops its timers, releases
its controllers and destroys the HWND. A failed save does not block closing or
open a dialog. That window's downloads and private tabs end; other windows
keep their controllers and session ownership. Normal tab metadata may be
restored from the last successful save, while unsaved page contents cannot.
Individual tab/download and explicit-unload prompts remain separate actions.

Creating separate user-data folders per tab/profile would start extra process
groups. Named profiles in a shared environment retain isolated cookies/site
storage while avoiding that particular duplication. This does not mean every
tab shares one renderer, nor does it remove Chromium site isolation. Source:
[multiple-profile support](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/multi-profile-support).

Event closures capture a weak application owner and a numeric tab ID plus a
generation. They find the live tab when invoked. Closing or unloading increments
the generation, removes event registrations, completes popup deferrals, closes
the controller and releases COM references. The callback object contains C++
exceptions at its COM ABI boundary.

Environment initialization belongs to the session, so simultaneous first
navigations cannot create competing environments. Browser-process failure
invalidates all windows' references for that environment generation and releases
their controllers. Duplicate crash notifications from the same generation are
ignored. Reload requests a new environment and other tabs remain eligible for
lazy recreation. Renderer failure remains scoped to the affected tab where the
runtime reports it that way. These recovery paths still require live-runtime
Windows testing.

The startup mutex keeps one app owner of the session file in a Windows login
session. A second invocation uses bounded `WM_COPYDATA` to send a versioned
packet containing profile, address and normal/private intent. The portable
codec checks lengths, UTF-8, control characters, profile names and protocol
fields. The receiver separately resolves and validates the address before
opening a tab. Legacy message IDs are still accepted. `--profile Work` creates
a valid missing profile or routes to the existing Work window; the session
alone chooses the target to activate. The sender grants foreground permission
to the existing process without first activating an arbitrary profile window.

The mutex name is retained from 0.2 to prevent separate versions writing the
same state concurrently. An older instance that rejects the new protocol
produces a restart instruction. Close all old instances before launching the
upgrade.

## Loading and sleep policy

Tabs fall into native-new-tab, unloaded metadata, creating, loaded, suspend
pending, suspended or failed states. Restored background tabs remain metadata
until first selected. A failed page can be retried with Reload.

The policy in `core.cpp` checks the active/loading/audio/pin/keep-awake/pending
flags and a monotonic timestamp. It has a five-minute Balanced delay and a
one-minute Saver delay. A ten-second UI timer evaluates eligible loaded tabs.
Downloads are pruned on every tick and block sleep while in progress.

`TrySuspend` is called only after the controller is hidden. Its completion must
report success and the runtime must report `IsSuspended`. A tab that was
reselected, started loading, became audible/protected or began downloading is
resumed. Loading state is associated with the current NavigationId so an old
cancelled navigation cannot clear the flag for a newer load. Sources:
[suspension API](https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2_3),
[memory target API](https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2_19).

Automatic sleep retains the controller and page state. Destroying a controller
is reserved for closure or the explicit Unload action, with a page-state loss
confirmation for unloading. This is a conservative starting point: suspension
cannot provide the same memory recovery as discarding every background page.

## Local state and privacy

`storage.cpp` serializes a bounded, versioned UTF-8 text file into
`%LOCALAPPDATA%\WebCom\state.v1`. It uses a same-directory temporary file, flush
and atomic replacement. Validation occurs before replacement. The previous
file survives a rejected save. Missing/corrupt state falls back to a Default
profile without navigating malformed stored addresses.

The session's `SaveAll` first snapshots every live profile window into the
shared `AppState`, then makes one atomic state-file replacement. Saving or
closing one window therefore cannot overwrite another open profile with a
stale copy. Closed profiles retain their most recent snapshot. The 0.4 changes
preserve the existing `state.v1` format and 0.2/0.3 data compatibility.

The file stores normal tab URLs/titles, selected indices, pin/keep-awake flags,
profile names and preferences. It never stores cookie or password values.
Private tabs are excluded by each window's snapshot operation. Private tabs also
are excluded from the in-memory recently closed list. Shared native search
controls are cleared when their tab owner changes. Each profile window has its
own controls; profile opening does not reuse another window's edit buffers.

`appearance.cpp` stores theme, optional exact profile color and bookmarks-bar
visibility in `%LOCALAPPDATA%\WebCom\Appearance\<lowercase-profile>.v1`.
The small, versioned file is validated before a flushed temporary write and
atomic replacement. Missing, malformed or oversized files fall back to the
default Graphite palette, automatic profile color and visible bookmarks bar.
Profile-name normalization matches the existing case-insensitive identity.
The separate file keeps new appearance options out of older session records.

`bookmarks.cpp` stores each profile's explicit bookmarks in
`%LOCALAPPDATA%\WebCom\Bookmarks\<lowercase-profile>.v1`. This is independent of
session restoration. Records contain UTF-8 titles and complete HTTP(S) URLs;
userinfo, malformed UTF-8, unsupported schemes and control characters are
rejected. Limits are 100 bookmarks, 1,024 bytes per title, 16,384 bytes per URL
and 6 MiB per serialized file. Validation precedes a flushed temporary write
and atomic replacement, preserving the old file on a rejected save.

Missing or corrupt bookmark files produce Google, YouTube and GitHub defaults;
a valid empty file remains empty. Ctrl+D is an explicit save/remove action,
including from private tabs, so intentionally saved private-page bookmarks
remain in the active profile. No bookmark is created automatically from private
browsing. Bookmarks are ordinary local metadata, not encrypted secrets.

Profile limits are 32 profiles, 200 tabs per profile, 16,384 UTF-8 bytes per URL,
4,096 per title and 4 MiB per file. The application clips/sanitizes site titles
before persistence and rejects URL userinfo so a password embedded in a URL
cannot enter the session file.

The WebView2 runtime owns its browsing-data folder and password-save workflow.
WebCom uses the API to enable save prompts for normal profiles. InPrivate
controllers disable password save/general autofill requests. This is not a
custom password manager or cloud synchronization service. Source:
[settings API](https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2settings4).

## Web/native boundary

No JavaScript host object or web-message bridge is exposed to arbitrary sites.
The native toolbar does not render remote HTML. Top-level navigation accepts
HTTP(S), with `about:blank` supported for engine-owned popup creation. The
omnibox rejects executable/custom/local-file schemes and URL userinfo.

No command-line flags disable sandboxing, site isolation, GPU acceleration,
TLS verification or browser security checks. Permission prompts, download
checks and script dialogs remain owned by WebView2's normal runtime behavior.
The app does not override Google authentication restrictions.

Native modal dialogs are shown after returning from synchronous engine
callbacks. Commands triggered by accelerator callbacks are queued to the
window. Code that prompts before an individual tab's download interruption or
explicit unload resolves the tab again afterward, because a nested modal loop
can deliver asynchronous page events. Closing the whole window does not show
these prompts.

## Module map

| File | Responsibility |
| --- | --- |
| `src/main.cpp` | Windows startup, COM apartment, single-instance handoff and message loop |
| `src/app.cpp` | Per-window controls, tabs, WebView2 events, lifecycle, queued UI updates and memory snapshot |
| `src/profile_host_decl.inl`, `src/profile_host_impl.inl` | Shared session, profile-window registry, save-all, focus routing and shared environment lifecycle |
| `src/launch_request.h` | Portable bounded/versioned profile-aware launch-message codec |
| `src/native_ui.inl` | Reference-based native geometry, painting, new-tab pages and bookmark UI |
| `src/chrome_layout.h` | Portable tab-strip and stable omnibox geometry |
| `src/render_helpers.h` | Reusable paint buffers, changed-only native layout and cached drawing state |
| `src/appearance.cpp`, `src/appearance_ui.inl` | Per-profile palettes, contrast, atomic preference files and native appearance menus |
| `src/favicon_image.h` | Bounded PNG favicon decoding into a 32×32 owned bitmap |
| `src/tab_search.cpp`, `src/tab_search_model.cpp` | Native searchable tab picker and portable matching/ranking logic |
| `src/dark_menu.cpp` | Native dark popup rows, keyboard/mouse handling, scrolling and compound zoom controls |
| `src/com_callback.h` | Small COM event-handler adapter and reference ownership |
| `src/core.cpp` | URL resolver, safe profile names, field escaping and pure sleep policy |
| `src/storage.cpp` | Validated session file and atomic writeback |
| `src/bookmarks.cpp` | Bounded per-profile bookmarks, defaults and atomic persistence |
| `tests/core_tests.cpp` | URL/security/escaping and memory-policy boundary checks |
| `tests/storage_tests.cpp` | Round trips, corruption, bounds and failed-save preservation |
| `tests/bookmark_tests.cpp` | Bookmark round trips, scheme/profile validation, empty lists and rejected-save preservation |
| `tests/launch_request_tests.cpp` | Launch packet round trips, malformed input, size bounds, UTF-8 and URL-policy separation |
| `tests/tab_search_tests.cpp` | Tab matching, ranking and result limits without a GUI |
| `tests/appearance_tests.cpp` | Theme/color parsing, contrast, profile separation, round trips and failed-save preservation |
| `tests/chrome_layout_tests.cpp` | Tab visibility, bounded geometry and stable address-field layout across sizes |
| `tests/windows_ui_tests.cpp` | Isolated profile windows, appearance, address selection, drag reorder, tab switching, visibility/GDI and direct-close checks |
| `resources/googlelogo.png` | Bundled official light Google logo, rendered locally on the normal new-tab page |
| `resources/webcom.ico`, `resources/webcom-icon.svg` | WebCom application icon and source artwork |
| `scripts/Build.ps1` | Visual Studio discovery, restore, build, tests and portable packaging |
| `scripts/Restore-WebView2.ps1` | Pinned SDK download, hash verification and offline override |
| `scripts/Measure-WebComMemory.ps1` | Scoped browser-process memory sampling |
| `Test-UI.cmd` | Runs the packaged native UI regression executable on Windows |

The native UI test uses a conditional `WEBCOM_TEST_MODE` constructor to place
all state in a temporary directory. It uses native tabs and never requests an
engine environment. The test executable has been cross-compiled; it has not
been executed on Windows in this build session. The portable tests have been
executed. See `VALIDATION.md` for current evidence and remaining runtime checks.

## Deliberate tradeoffs

A full Chromium fork would offer deeper browser integration, but would also
require a substantial browser security/update and distribution operation.
WebView2 is a practical Windows foundation for this first iteration. Its
embedded-browser limitations, especially Google authentication and Chrome
Sync, remain product constraints. Native GUI/runtime behavior and comparative
RAM use still require Windows testing.
