# WebCom v3 roadmap

WebCom v3 has completed the planned public-beta feature pass. Latest-39 created the signed Release Candidate gate, Latest-40/41 hardened Windows accessibility, and Latest-42 folds concrete failures from the older real MSVC build logs into the current source-preflight and Windows UI harness. Further engineering changes should now be driven by the next real Windows/MSVC/WebView2/signed-installer/accessibility run rather than feature expansion.

## Next priorities

1. Run `scripts/Test-ReleaseCandidate.ps1` against the exact signed installer/manifest intended for rollout; archive `release-candidate.json` and every component report.
2. Supply a previous signed installer when available so the in-place upgrade/profile-state compatibility gate runs instead of being recorded as not-run.
3. Test both the custom tab provider and Latest-41 painted-action client provider with Narrator plus MSAA/UI Automation inspection tools at 100/125/150/200% DPI; verify native edit/button children remain discoverable.
4. Exercise update-health and rollback-blocking conditions using the same publisher pin and signed packages approved by the Release Candidate gate.
5. Record and approve the additional multi-tab, sleeping-tab, split-view, and multi-profile Windows memory baselines after the default one-tab 300 MiB gate.
6. Fix only concrete Windows/runtime/accessibility/security failures discovered by those gates before wider rollout.

- New Tab dashboard now includes recent sites, native quick actions, workspace cards, per-profile custom shortcuts, and non-destructive recent-site pin/hide controls; optional visual customization remains future work.

- History/Downloads organization pass implemented: top-site filtering/cleanup and local-date download grouping.

- Workspace tab-productivity pass implemented: safe duplicate cleanup and sleep-other-tabs actions.

- Performance timeout pass implemented: per-profile 1/5/15/30-minute/Never automatic sleep policy plus bounded exact-host Never-sleep exceptions, without changing the session-state schema.
- Performance hardening implemented: adaptive automatic-sleep polling and first-use loading for Downloads/Site Permissions/Saved Sessions/unpacked-extension metadata to reduce idle wakeups and cold-start I/O without adding background services.
- UI smoothness pass implemented: destination-first WebView visibility handoff, hidden-until-layout startup chrome, buffered omnibox suggestion reveal, and reduced-motion-safe tab/reflow/dark-menu animation; real Windows visual acceptance remains required.
- Warning-clean native MSVC policy implemented: WebCom and the Windows UI harness compile project warnings as errors by default; diagnostic warning allowance is explicit and Release Candidate sign-off remains warning-clean.
- Accessibility/DPI hardening implemented: effective Windows/profile reduced-motion and High Contrast policy, live system-setting refresh, DPI buffer/font rebuild, stale-popup dismissal, and keyboard focus/activation for native internal-page actions. Latest-41 additionally wraps the standard client accessibility tree and exposes visible painted actions as virtual MSAA push buttons with names/bounds/hit testing/focus/default actions. Narrator/UI Automation and real multi-monitor DPI transitions still require Windows acceptance.

- Downloads v2 pass implemented: validated source actions, bounded failed-retry workflow, and 30-day completed-history retention cleanup.

- Privacy Manager v2 pass implemented: searchable cookie/permission domains with live profile totals and unchanged scoped cleanup actions.

- Password/autofill UX pass implemented: first-class native manager for supported WebView2 controls with an explicit credential-store privacy boundary.

- Public-launch failure UX pass implemented: native failed-page recovery with retry/address-edit/New-Tab actions kept outside site content.

- Extensions v2 pass implemented: native search, All/Enabled/Disabled/Needs-attention filters, live counts, and source-health classification without claiming Chrome Web Store compatibility.

- Onboarding pass implemented: live Default browser / Bookmarks / Workspaces / Update checks setup checklist with explicit user-controlled actions.

- Ad Blocker v2 pass implemented: profile-scoped site pause/resume through Site Information, bounded host exceptions rendered into real DNR/cosmetic/YouTube bypass assets, automatic bundled-extension reload, Extensions health/count/clear controls, and expanded portable/script regression coverage.

- About/performance-observability pass implemented: native About page, host-vs-WebView2 live memory snapshot, and repeatable scenario-labelled Windows memory reports.

- Public-beta completion pass implemented: persistent Recently Closed recovery, actionable download/update notices, privacy-safe Report a Problem flow, honest Ad Blocker state in Privacy Manager, and expanded browsing-data/profile cleanup.
- Feature growth should now pause for Release Candidate acceptance unless Windows testing exposes a user-blocking gap.

- Log-driven MSVC regression hardening implemented: known annotation `LONG`/`int` clamp ambiguity, GDI+ line-overload ambiguity, brittle UI-test startup aggregates, and workspace command-ID collisions are guarded before the main Windows compile.
- Package-integrity hardening implemented: staged PE/version/source-manifest/content validation, release trust-input checks, pre/post-sign executable verification, and signed-dist-to-installed-executable SHA-256 identity binding.
