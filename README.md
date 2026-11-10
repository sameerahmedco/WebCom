# WebCom for Windows

**Version 0.5.1 privacy, connection security + profile lifecycle source preview — native C++20 / Win32 / Microsoft Edge WebView2**

WebCom is a Windows browser preview with a native interface developed from
the supplied Chrome reference screenshot, four dark themes, simultaneous local
profile windows, and deliberate tab-lifecycle management. The tabs, caption
buttons, toolbar, bookmarks bar, new-tab pages and menus are native Windows UI. Google is
the default search engine. The Windows app does not require the earlier Linux
Python application or its virtual environment.

The 0.4.7.4 baseline was confirmed by the user to compile successfully with MSVC on
Windows. Version 0.5.1 keeps that build lineage and completes the site-info surface with cookie/site-data visibility, clearer HTTPS/HTTP/certificate status, and supported WebView2 profile deletion. The portable GCC/Clang suite passes here, but the new Windows-only cookie/profile lifecycle paths still require a fresh Windows build and live acceptance before release. `docs/VALIDATION.md` records the completed checks and their limits.

## 0.5.1 privacy, connection security and profile lifecycle

The omnibox site-info shield now leads with connection security. HTTPS pages show a secure connection state, HTTP pages are explicitly marked unencrypted, and certificate-related navigation failures are surfaced as certificate problems instead of looking like a normal secure page. The same panel now includes a Cookies and site data row with a per-site cookie count and a native details menu. Cookie values are intentionally never displayed; WebCom shows only cookie names, domain/path and Secure/HttpOnly flags, and can clear cookies matching the current site.

Profiles now have a real Delete current profile action in the profile menu and native Profiles page. WebCom refuses to delete the last profile or a profile with an active download, asks for destructive confirmation, then uses the WebView2 profile-deletion API before removing WebCom-owned history, bookmarks, permissions, appearance and saved-session metadata. Downloaded files remain on disk. Session primary and recovery generations are both sanitized so the deleted profile cannot be resurrected by recovery.

## 0.5.0 site controls and browser chrome maturity

HTTP(S) pages now expose a native shield button in the omnibox. Camera, Microphone, Location and Notifications can be set to Ask, Allow or Block per site and profile. WebCom stores the rules atomically, synchronizes changes to the WebView2 profile permission API, and keeps Incognito on Ask-each-time rather than inheriting normal-profile overrides. Clearing browsing data also clears WebCom's saved permission rules.

Profiles with downloads now show a compact recent-download button between the omnibox and profile control. It displays up to five recent items with state/progress and routes active items to the native Downloads page; completed items can open directly. The button is layout-aware and disappears on narrow windows before the omnibox is squeezed.

## 0.4.9 organization and event-driven downloads

History now supports filtered bulk deletion plus 7-day and older-than-30-day cleanup. Bookmark folders can be nested with `/`, nested hierarchy round-trips through Chromium/Netscape bookmark HTML, and folder trees can be renamed from bookmark context actions. Tab groups use a clearer colored chip and can be closed as a group. The Profiles page can open every saved profile or close every other profile window.

Live Downloads now subscribe to WebView2 byte/state change events when available; the old 250 ms polling loop is no longer the normal path. A one-second fallback timer is used only if event registration fails. Completion and unexpected interruption surface non-blocking WebCom notifications.

## 0.4.8 recovery and download controls

WebCom now maintains a last known-good session generation and falls back to it automatically
when the current session file cannot pass the normal safety/parser checks. Downloads gained
native Pause/Resume/Cancel controls, runtime resume support for resumable interruptions, and
a safe Retry path that reopens only validated HTTP(S) sources. Paused downloads remain
protected and visible instead of being mistaken for completed history.

## 0.4.7.4 MSVC compile hotfix

This hotfix addresses the concrete MSVC errors reported after the COM-header fix: a Windows `small` macro collision in favicon decoding, a duplicate native-page variable, ambiguous `LONG`/`int` `std::max` calls, and mixed-type `auto` declarator lists for temporary filesystem paths. It also cleans the remaining warning in the name dialog and the explicit-black appearance regression check.

## 0.4.7.3 Windows build hotfix

This hotfix fixes the MSVC/Windows SDK header failure reported after the WebView2 SDK restored successfully. WebCom's native targets previously defined `WIN32_LEAN_AND_MEAN`, which left newer MSVC/SDK combinations without the COM ABI declarations required by `WebView2.h` and GDI+. The native browser targets now use the full Win32 header surface and explicitly include COM ABI headers before WebView2/GDI+. The WebView2 SDK remains pinned to Microsoft.Web.WebView2 1.0.4314.35.

## 0.4.7.1 build hotfix

This hotfix addresses build failures reported against 0.4.7. The build now avoids the newly introduced `std::set::contains` dependency, improves WebView2 restore verification, and writes `build-last.log` beside `build.cmd`. If the optional native UI harness is the only failing target, run `build.cmd -SkipUiTests` to build/package the browser executable while isolating that harness for diagnosis.

## Build and try 0.5.1 on Windows

The current delivery is source-only. On a Windows 11 x64 development machine,
install Visual Studio Build Tools with **Desktop development with C++**, a Windows
SDK and CMake tools, then run:

```bat
build.cmd
run.cmd
```

Keep the Microsoft WebView2 Evergreen Runtime installed. The project pins its
WebView2 SDK build dependency in the documented build path; see `BUILDING.md`.
An optional Linux-to-Windows cross-build script is also included, but it requires
Zig 0.13.0 and the official WebView2 SDK to be supplied locally.

## What is implemented

| Area | Current implementation |
| --- | --- |
| Interface | Custom tabs and Windows caption, stable rounded address field, local autocomplete for tabs/bookmarks/history, website favicons, native menus/search, reusable paint buffers and a WebCom application icon |
| Appearance | Graphite, Charcoal, Midnight and Forest themes; automatic, preset or custom hex profile colors; independent bookmarks-bar visibility per profile |
| Tabs | New, close, duplicate, reopen, compact favicon-only pinned tabs, persistent named/color-marked tab groups, searchable tab picker, mouse/keyboard reordering, middle-click close and tab mute/unmute |
| Google | Default address-bar search, native normal new-tab page with Google search and website shortcuts, and an AI Mode website shortcut |
| Bookmarks | Per-profile root bar plus top-level folder buttons, nested folder paths, searchable Bookmark Manager, Ctrl+D save/remove, folder-tree rename/moves, and nested Netscape/Chromium-compatible HTML import/export |
| Profiles | Run multiple named profiles simultaneously with separate runtime data; open all saved profiles, close other profile windows, move tabs between profile windows, and permanently delete the current profile when at least one other profile remains |
| Private browsing | Native Incognito new-tab page; Ctrl+Shift+N or `--private` opens a private tab using the runtime's InPrivate mode |
| Session restore | Normal tab URLs, titles, selected tab, pin/keep-awake state and preferences are saved locally; a validated last-known-good backup is maintained for automatic recovery; background pages load only when selected |
| Memory | Balanced and Saver modes, best-effort background suspension, low-memory hints, manual sleep and explicit unload |
| Credentials | WebView2's native local save/update-password prompts and autofill for normal profiles; no app-owned plaintext password database |
| Downloads | Native per-profile Downloads page with event-driven byte/state progress, Pause/Resume/Cancel, resumable interruption handling, safe Retry, completion/interruption notifications, completed-file opening, plus a compact recent-download toolbar bubble |
| Site controls | Native omnibox shield with HTTPS/HTTP/certificate status, per-site cookie count/details/clear action, and per-profile Camera/Microphone/Location/Notifications Ask/Allow/Block rules; Incognito remains ask-each-time |
| Internal pages | Native searchable History and Bookmark Manager plus Downloads and Settings tabs use the buffered Win32 renderer instead of creating another WebView |
| Page tools | Menu zoom controls, print preview, Find in page and developer tools through WebView2 APIs |
| Popups | User-initiated HTTP(S) popups become tabs in the originating profile/private context; unsolicited popups are blocked |
| Diagnostics | Menu memory snapshot and PowerShell process-tree measurement script |
| Recovery | Renderer failure page, shared-environment recovery on reload, runtime-install guidance, plus automatic session fallback from a validated last-known-good backup |

### WebCom icon

The approved ice-blue **W** on the charcoal tile is the single WebCom application
mark. The same artwork is embedded in `WebCom.exe` at multiple Windows icon sizes
and is used for the executable, taskbar, Alt+Tab and WebCom window icon surfaces.
The portable package also includes `WebCom.ico` and `WebCom-icon.png` for shortcuts
and future installer/package metadata. Website favicons and profile color badges stay
site/profile-specific by design.

## What changed in 0.4.8

Version 0.4.8 is the **recovery and download-control update**. Session persistence now
rotates a validated previous generation into `state.v1.bak` before replacing the primary
session file. If the primary session is malformed or otherwise fails the same safety checks
used during normal restore, WebCom automatically loads the backup and shows a non-blocking
recovery notice in the first profile window.

The native Downloads page now controls live WebView2 download operations: Pause/Resume,
Cancel, and resume of interrupted operations when WebView2 reports `CanResume`. WebView2
represents a paused download as an interrupted operation with `USER_PAUSED`; WebCom preserves
that operation and presents it as Paused instead of discarding it. Previous-run interruptions
that cannot be resumed in place can retry by opening only a validated HTTP(S) source in a new
tab. Paused operations survive Clear completed and continue to protect their source tab from
sleep/close/unload mistakes.

## What changed in 0.4.7

Version 0.4.7 is the **tab-group, history and library interaction update**. Tab groups now persist their own color and collapsed/expanded state in session format v3. A collapsed group occupies one representative slot; expanding it fans the hidden tabs back out with the same native 160 ms reflow language used by tab opening and closing. Group context actions include collapse/expand, rename and eight color choices. Older v1/v2 WebCom sessions remain readable.

History rows now have an in-page delete action in addition to search/date grouping, while private browsing still never enters the saved History list. The Bookmark Manager supports drag reordering within a folder, and moving a bookmark between folders preserves the user's manual ordering instead of silently alphabetizing the whole collection. Nested action hit testing was tightened so row-level open actions no longer steal hover/clicks from delete/remove controls.

The native Profiles page is the primary profile-management surface, showing each profile's identity color, current/running state and open/focus action while multiple profiles remain live in separate windows. Existing cross-window tab dragging uses screen coordinates, so the same move path works across monitors; the destination reloads the page in its isolated profile rather than transferring WebView session state.

## What changed in 0.4.6

Version 0.4.6 is the **tab organization and library-management update**. Pinned tabs now use a true compact favicon-only region on the left of the strip, while normal tabs retain the available width. Named tab groups persist with the saved session, keep same-group tabs together where pinning rules allow, and use a stable color marker plus the group name on the leading tab. Group assignment and removal live in the tab context menu.

History and Bookmark Manager now have native in-page filtering fields. Bookmark storage supports folders, the bookmarks bar renders folders as real folder buttons with dark popup menus, and bookmarks can be imported from or exported to standard Netscape/Chromium bookmark HTML. The storage reader remains backward compatible with older WebCom bookmark/session formats.

Tabs can be moved between simultaneously open profile windows from the tab context menu or by dragging onto another WebCom profile window. The destination keeps the URL, private-mode state, pinned/keep-awake metadata and group metadata, then reloads the page in that profile's isolated WebView2 context. Active downloads intentionally block the move. Omnibox suggestion hover states now use the same short native crossfade language as the rest of the chrome.

## What changed in 0.4.5

Version 0.4.5 is the **omnibox and bookmark-management update**. The address bar now
shows a custom native suggestion panel while you type. Suggestions are ranked from
open tabs, this profile's bookmarks and recent history, with a Google search action.
Choosing an open-tab result switches to that tab rather than navigating a duplicate.
The matching and ranking logic is local; WebCom does not send suggestion queries to
a separate autocomplete service.

**Ctrl+Shift+O** opens a first-class native Bookmark Manager. It lists saved titles
and URLs, opens a selected bookmark in the current tab, and removes bookmarks without
leaving the page. The Settings page links directly to the manager. History rows are
now grouped under local-date headings such as Today and Yesterday while retaining
per-profile storage and Incognito exclusion.

Pinned tabs now move to the left side of the tab order when pinned and return just
after the pinned set when unpinned. Omnibox suggestions use the same buffered native
painting strategy as the rest of the browser chrome; no hidden WebView is created.
Google remains the default search engine and the approved WebCom icon remains canonical.

## What changed in 0.4.4

Version 0.4.4 is the **native browser-pages and tab-motion update**. History,
Downloads and Settings now open as first-class WebCom tabs rendered by the same
buffered Win32 UI as the browser chrome, so these pages do not allocate an extra
web renderer. **Ctrl+H** opens full profile History, **Ctrl+J** opens Downloads,
and the main Settings command opens the native Settings page.

The Downloads page tracks current and completed downloads for the profile, shows
byte progress when the runtime exposes a total size, opens completed files, opens
the Windows Downloads folder, and can clear completed entries without cancelling
active transfers. History remains profile-local and excludes Incognito activity.

Opening and closing tabs now animate the remaining tab geometry with a 160 ms
cubic ease-out reflow rather than snapping immediately to the new positions. Menu
opening honors Windows' animation preference and uses a short native fade where
animations are enabled. Existing 100 ms hover crossfades and the 140 ms selected-tab
transition remain independent. No hidden WebView is pre-created for these effects.

The native tooltip-popup path stays disabled to prevent the yellow/top-left flash
reported during rapid tab/layout changes. The approved WebCom square icon remains
the canonical application mark, and Google remains the default search engine.

## The redesigned interface

At 100% scaling, the top regions are **40 pixels for tabs/caption, 44 pixels for
navigation, and an optional 37 pixels for bookmarks**. Content begins at pixel
121 with the bookmarks bar shown, or 84 with it hidden. Those dimensions scale
with Windows DPI. The active tab has curved shoulders; the address field uses
the profile's readable accent for focus. There is no permanent bottom status
bar; short notifications appear temporarily.

Open **Menu → Settings**, then choose **Appearance** on the native Settings page for **Graphite**, **Charcoal**, **Midnight** or **Forest**. Graphite is the default; Charcoal is the closest
palette to the supplied reference. **Profile color** offers Automatic, Blue,
Green, Yellow, Rose, Lavender, Cyan, Peach, Silver, and **Custom color…**. Enter
six hexadecimal digits, with or without `#`, such as `#8BD4BF`. The profile
badge uses that exact color; focus indicators may use a lighter related shade
to stay visible on dark backgrounds. **Reset appearance** restores Graphite,
the automatic profile color and a visible bookmarks bar for this profile.

The normal new-tab page provides a Google search field and Google, YouTube and
GitHub shortcuts. **AI Mode** opens Google's AI Mode website; its availability
and behavior are controlled by Google. WebCom is not affiliated with Google.

Open **Menu → Settings** and use the **Performance** card to switch between Balanced and Memory Saver. The Settings page also exposes password-save prompts, bookmarks-bar visibility, profile color and browsing-data controls. **Menu → Performance** or **Shift+Esc** opens the memory snapshot.
Zoom controls apply to loaded web pages; the native new-tab pages retain their
Windows DPI layout. Short Incognito windows use compact spacing, with mouse-wheel
or Page Up/Page Down scrolling when necessary. Print and the menu's Find action depend on the installed
runtime exposing their APIs. Ctrl+F uses that Find API from the native toolbar
and is passed through to the runtime when a web page has focus.

The layout follows the supplied reference; pixel-for-pixel rendering across
Windows versions and DPI settings has not yet been verified.

## Bookmarks and Incognito

The initial bookmarks are **Google, YouTube and GitHub**. The reference image's
personal bookmark destinations were not supplied, so WebCom uses these known
public addresses. Each normal profile has its own saved list, up to 100 entries.
An intentionally empty list remains empty after restart.

Press **Ctrl+D** to save the current page, or remove it if already bookmarked.
The star in a web page's address bar has the same action. Right-click a
bookmark to open or remove it; **Menu → Bookmarks** includes entries that do not
fit on the bar. **Ctrl+Shift+B** toggles the bar without deleting bookmarks.
Bookmark titles and HTTP(S) URLs are stored locally.

Press **Ctrl+Shift+N** to open the native Incognito page. The command line also
accepts:

```bat
WebCom.exe --private
WebCom.exe --private --url https://example.com
```

Private tab URLs and titles are excluded from restored and recently closed
tabs. Bookmarks deliberately saved from Incognito and downloaded files remain
on the device. Incognito does not hide activity from websites or the network.
The private page describes those limits without claiming a cookie-blocking
setting that WebCom has not configured. **Ctrl+T creates a normal tab.**

## Memory behavior

| Mode or condition | Behavior |
| --- | --- |
| Fresh native new tab | No WebView2 controller is created for that tab |
| Restored background tab | Metadata only until first selected |
| Balanced | Eligible background tabs become sleep candidates after 5 minutes |
| Memory Saver | Eligible background tabs become sleep candidates after 1 minute; inactive pages also receive lower memory-target hints |
| Playing audio, loading, downloading, pinned or keep-awake | Excluded from automatic sleep |
| Selecting a sleeping tab | Resumes the page and restores its normal memory target |
| Explicit **Unload tab** | Closes its controller after confirmation; the page reloads on selection and unsaved page state is lost |

The timer checks eligibility every 10 seconds. Suspension is best effort and
may be declined by a busy runtime. The UI marks a tab sleeping only after a
successful callback and a confirmed suspended state. Automatic sleep does not
destroy the page controller. Use **Keep this tab awake** for calls, live
dashboards, silent media, background processing or other pages that must keep
running; this version cannot reliably detect every background workload.

C++ gives the interface direct control over object lifetime, but web rendering
and JavaScript still run in WebView2's browser processes. One shared environment
is used across all tabs, named profiles and their windows. This follows Microsoft's guidance
for avoiding redundant process groups, but **no specific RAM reduction has
been established**. See Microsoft's [performance guidance](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/performance)
and [multiple-profile documentation](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/multi-profile-support).

## Profiles, passwords and Google accounts

Click the profile name on the toolbar, or press **Ctrl+N**, to open the profile
picker. Selecting another profile opens its own window while the current
profile's tabs remain running. Selecting an already-open profile focuses that
window. Version 0.4 supports **one window per named profile**; closing one window
leaves the others running, and closing the last window exits WebCom. Names are
1–40 ASCII letters, digits, spaces, underscores or hyphens and match without
regard to case.

The command line can select or create a profile:

```bat
WebCom.exe --profile Work
WebCom.exe --profile Personal https://example.com
WebCom.exe --profile Work --private
```

A valid new name creates a local profile. Launching into an existing window
opens the requested address, or a new tab when no address is given. All windows
share one process-level session owner, so saving from one window also captures
the other open profiles. Existing 0.2 and 0.3 session and bookmark files remain
compatible. New appearance preferences use a separate file. On restart, WebCom
opens the last-focused profile; open the other saved profiles from the picker
when needed.

The app's data directory is:

```text
%LOCALAPPDATA%\WebCom
```

`state.v1` contains normal tab metadata and preferences. `Bookmarks` contains
the separate per-profile bookmark files. `Appearance` contains each profile's
theme, color and bookmarks-bar visibility. `User Data` is the runtime-managed
browsing store. Private tabs are omitted from `state.v1`.
Private tabs belonging to the same named profile share its InPrivate session;
private browsing is not network anonymity, and downloaded files remain.

Password-saving is enabled for normal profiles through the WebView2 API. The
runtime controls prompts, storage and filling. WebCom does not read, export,
intercept or implement its own encryption for password values. Disabling the **Passwords and autofill** setting on the native Settings page stops new save prompts; passwords
already stored may still autofill. **Delete browsing data** requests the
runtime's all-data deletion operation, including saved passwords. Read the
[official autofill API behavior](https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2settings4).

**Chrome Sync is not included, and Google sign-in may be blocked by WebView2's
embedded-browser restrictions.** Local profiles and default-browser handoff do
not transfer another browser's account session into WebCom. See [WebView2 browser
differences](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/browser-features)
and [Chromium sign-in restrictions](https://www.chromium.org/developers/how-tos/api-keys/).

## Closing and session restore

The window close button, **Alt+F4**, **Menu → Close this window**, and the window-close command
close that profile window directly. There is no multi-tab, private-tab,
download or failed-save confirmation on this path. WebCom first attempts to
save normal tab metadata across the open profile windows. A rejected file
replacement preserves the previous saved session, but changes since the last
successful save may be lost. The close still completes.

Closing a window ends its private tabs and active downloads and discards
unsaved page contents. Normal tab addresses and titles can be restored; form
drafts, scroll positions, full navigation history and private sessions cannot.
Other profile windows keep running; closing the final window exits the app.
Individual tab actions retain their existing download and explicit-unload
prompts. Save work on its website before closing when it needs to survive.

## Keyboard shortcuts

| Shortcut | Action |
| --- | --- |
| Ctrl+T | New normal tab |
| Ctrl+N | Open the profile picker to create, open or focus a profile window |
| Ctrl+Shift+N | New private tab |
| Ctrl+W / Ctrl+F4 | Close current tab |
| Ctrl+Shift+T | Reopen last closed normal tab, within the current session/profile |
| Ctrl+L / Alt+D / F6 | Focus address bar |
| Ctrl+Tab / Ctrl+Shift+Tab | Next / previous tab |
| Ctrl+Shift+A | Search the current profile's open tabs by title or address |
| Ctrl+Shift+PgUp / Ctrl+Shift+PgDn | Move the current tab left / right |
| Ctrl+1…8 / Ctrl+9 | Select a numbered tab / last tab |
| Alt+Left / Alt+Right | Back / forward |
| Ctrl+R / F5 | Reload, or stop a currently loading page |
| Ctrl+J | Open the native Downloads tab |
| Ctrl+D | Save/remove the current page's bookmark |
| Ctrl+Shift+B | Show/hide this profile's bookmarks bar |
| Ctrl++ / Ctrl+- / Ctrl+0 | Increase/decrease/reset web-page zoom |
| Ctrl+P | Runtime print preview |
| Ctrl+F | Runtime Find in page; native-toolbar focus uses the Find API |
| Ctrl+Shift+Delete | Request browsing-data deletion for the current normal runtime profile |
| Alt+F / Alt+E | Main menu |
| Shift+Esc | Memory snapshot |
| F11 | Toggle full screen |
| F12 | Developer tools for the current web page |
| Alt+F4 | Close this profile window without a confirmation |

Native-toolbar Tab navigation is kept separate from website form navigation.
The menu also exposes page zoom, Find, Print and developer tools directly.

## Check RAM usage

In WebCom, press **Shift+Esc** or choose **Menu → Performance**. The snapshot
includes `WebCom.exe` and the runtime's reported process group. The runtime's
crash reporter is not included in that API list. Read failures are reported as
incomplete snapshots.

For repeated samples, open PowerShell in the extracted portable folder:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Measure-WebComMemory.ps1 -Watch
```

The script filters the process tree to WebCom and its `msedgewebview2.exe`
descendants. It does not count every WebView2 process on the computer. The
process-only execution-policy argument does not modify the machine policy.

Record both working set and private commit. Working set is resident memory;
summed values can double-count shared pages. Private commit is private
allocated memory and can include pages that are not currently resident.
Microsoft defines the counters in [PROCESS_MEMORY_COUNTERS_EX](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters_ex).

Compare the same pages, activity and idle durations with Chrome's full process
group. A tiny native host process alone does not prove a low-memory browser.

## Build and project files

The complete instructions are in `BUILDING.md`. With the Visual Studio C++
workload, Windows SDK and CMake tools installed:

```bat
build.cmd
run.cmd
```

The project includes the native app, eight portable test suites for core,
storage, bookmarks, launch requests, tab search, appearance, omnibox ranking and chrome layout,
a native Windows UI regression executable, checked PowerShell SDK restore, memory
measurement, an optional Linux cross-build script and a Windows GitHub Actions
workflow. The portable tests were executed for 0.4.7; the Windows app and native
UI harness were not rebuilt in this environment because the required Windows
cross-build toolchain is unavailable. On Windows, **Test-UI.cmd** runs that harness with temporary profile data
and native tabs, without navigating websites or using your normal WebCom data.
It does not validate WebView2 rendering. See `BUILDING.md` and
`docs/VALIDATION.md` for the exact boundary. No repository has been published
or workflow run on the user's behalf.

## Current limits and next work

- Native Windows runtime testing, accessibility and high-DPI visual QA remain.
- RAM usage has not been benchmarked against Chrome or the earlier Linux build.
- Nested bookmark folders, direct Chrome-profile import, Linux-profile migration,
  extensions UI, Chrome Sync, a password-list editor and a signed installer remain
  future work. History supports search, date grouping and per-entry deletion; date/range
  bulk deletion can be added later.
- Each named profile has one live window; duplicate windows for the same profile and
  profile rename/delete remain future work. The native profile manager, local omnibox
  autocomplete, cross-window/cross-monitor tab moves and compact pinned tabs are implemented.
- HTTP(S) browsing is supported. Local files, executable URL schemes and
  external protocol handlers are not exposed by this preview.
- Session restoration preserves URLs and titles, not forms, scroll positions,
  complete navigation history or private sessions. Tab and window closure can
  discard unsaved page contents; window closure has no confirmation.
- The engine follows the installed Evergreen Runtime. The WebCom shell itself
  has no automatic updater in this preview.

The next development gate is Windows acceptance testing and a reproducible RAM
comparison. `docs/VALIDATION.md` gives the exact checks; `docs/ARCHITECTURE.md`
explains the implementation and `docs/ROADMAP.md` gives the continuation order.
