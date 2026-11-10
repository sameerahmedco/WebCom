# Design references for WebCom 0.3

Reviewed on 10 November 2026. This is a representative review of Chromium and
six Chromium-based browser families, not an inventory of every Chromium fork.
The references informed WebCom's own native implementation; their presence here
does not imply feature parity or affiliation.

## Changes selected for this release

| Reference | Useful behavior | WebCom application |
| --- | --- | --- |
| [Chrome tab management](https://support.google.com/chrome/answer/2391819?hl=en&co=GENIE.Platform%3DDesktop) | Search open tabs by title or address; keyboard access through Ctrl+Shift+A | A native searchable tab switcher with bounded results, current/private/audio/sleeping status and stable tab identities |
| [Chrome profiles](https://support.google.com/chrome/answer/2364824?hl=en) | Separate work and personal browsing data | Simultaneous profile windows with clearly identified local profiles |
| [Vivaldi Quick Commands](https://help.vivaldi.com/desktop/shortcuts/quick-commands/) | A compact, keyboard-driven way to locate tabs and commands | Search-first tab navigation with arrow keys, Enter and Escape; a general command palette remains separate future work |
| [Vivaldi workspaces](https://help.vivaldi.com/desktop/tabs/workspaces/) | Focus a workspace's existing window when it is already open | Focus an already-open profile window without closing another profile's browsing session |
| [Edge vertical tabs](https://www.microsoft.com/en-us/edge/features/vertical-tabs) | Legible tab identities and visible sleeping state | Clear tab status; optional vertical layout is deferred |
| [Brave Memory Saver](https://support.brave.app/hc/en-us/articles/13383683902733-How-do-I-use-the-Memory-and-Energy-Saver-features-in-Brave) | Explicit inactive-tab appearance and keep-active exceptions | Preserve WebCom's sleeping/keep-awake controls and make status visible in tab search |

The default visual direction remains the user's dark Chrome reference: compact
tabs, charcoal surfaces, a rounded address bar and restrained menus. Google
remains the default search endpoint. Additional browser features should fit
that layout and the memory goal instead of creating a dashboard around it.

## Rendering and profile architecture

[Chromium graphics architecture](https://www.chromium.org/developers/design-documents/aura/graphics-architecture/)
describes invalid-region tracking and separation between painting and
composition. The document includes historical implementation terminology; it
is a design reference, not a current embedding API. WebCom applies the general
principle by avoiding unnecessary layout/redraw work and retaining native paint
buffers. It does not embed Chromium's Views/Aura framework.

Microsoft's [WebView2 multiple-profile guidance](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/multi-profile-support)
documents named profiles with separate cookies, preferences and cached resources
inside one user-data folder. The [performance guidance](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/performance)
recommends shared environments, avoiding redundant controls and using lightweight
native UI before web content is needed. Those principles guide simultaneous
profile support and lazy tab restoration.

The [WebView2 process-model documentation](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/process-model)
also explains that shared user-data folders require consistent environment
options. Separate profile windows still need correct session ownership and
lifecycle handling in WebCom; the runtime does not manage the application's
own saved-tab files.

## Additional patterns reviewed, deferred

- [Brave vertical tabs](https://brave.com/blog/vertical-tabs/) and Edge's optional
  sidebar layout are candidates for a later layout preference.
- [Opera Tab Islands](https://www.opera.com/features/tab-islands) and
  [Vivaldi Tab Stacks](https://help.vivaldi.com/desktop/tabs/tab-stacks/) provide
  useful named, colored and collapsible tab-organization patterns. Grouping,
  drag-and-drop and related session schema changes require their own validation.
- Arc's sidebar, pinned tabs and profile/space distinction were considered as
  design patterns. Its [official sunset notice](https://resources.arc.net/hc/en-us/articles/44412972248471-Arc-Sunset-Key-Dates-and-What-to-Expect)
  announces desktop shutdown for 22 December 2026. It is included as a design
  reference, not a current product recommendation or dependency.
- Split view, automatic tab archiving, cloud sync, AI services and bundled
  extensions are not implied by this research. They have separate resource,
  privacy, data-loss and maintenance considerations.

## Supplied GitHub repository

The repository [scheib/chromium-latest-linux](https://github.com/scheib/chromium-latest-linux)
was inspected through GitHub, including its README, directory listing and all
three shell scripts. It downloads and launches Linux Chromium snapshots:

- [`update.sh`](https://github.com/scheib/chromium-latest-linux/blob/master/update.sh)
  reads the Linux snapshot revision, downloads `chrome-linux.zip`, extracts it
  and updates a local symlink.
- [`run.sh`](https://github.com/scheib/chromium-latest-linux/blob/master/run.sh)
  starts that Linux executable with a local user-data directory.
- [`update-and-run.sh`](https://github.com/scheib/chromium-latest-linux/blob/master/update-and-run.sh)
  combines the two operations.

It contains no Chromium browser source, Windows renderer or reusable native UI
framework. None of its scripts are executed or redistributed by WebCom 0.3.

## Validation boundary

Design references establish the source of an idea, not proof that WebCom behaves
the same way. Compilation and portable model tests do not establish native
Windows smoothness, accessibility, profile isolation or RAM usage. Consult
`VALIDATION.md` for the checks actually completed for this build.
