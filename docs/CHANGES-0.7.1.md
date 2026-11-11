# WebCom 0.7.1 changes

0.7.1 is the first-run/default-browser readiness milestone.

## Windows Default Apps registration
- The Inno Setup installer registers WebCom as a candidate handler for `http` and `https` through Windows Default Programs capabilities.
- Registration uses `HKA`, so administrative installs register machine-wide and `/CURRENTUSER` installs register for the current user.
- `ChangesAssociations=yes` asks Explorer to refresh association choices after install/uninstall.
- WebCom never writes the user-choice hash or silently takes over HTTP/HTTPS defaults.
- Settings, Help, Quick Commands and Getting Started can open the supported Windows Default Apps surface.
- On supported Windows 11 builds, WebCom deep-links to its registered app entry; otherwise it falls back to the general Default Apps page.

## First-run Getting Started
- A brand-new WebCom data root opens one native `webcom:welcome` tab.
- Existing users with `state.v1`/`state.v1.bak` are not interrupted after upgrading.
- `onboarding.v1` is a tiny atomic state file that records only whether the welcome page has been shown.
- The first-run welcome tab is ephemeral and excluded from restored session persistence.
- The page links to Default Apps, update choices, keyboard shortcuts and Start browsing.

## Internal navigation consistency
- The shared address resolver now recognizes all current native WebCom pages, including Welcome, Diagnostics, Updates, Sessions, Privacy and Reader Mode.
