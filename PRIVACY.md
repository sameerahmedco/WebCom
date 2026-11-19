# WebCom Privacy

This document describes privacy behavior implemented by WebCom v3 itself. Websites, Microsoft Edge WebView2, Windows, search providers, extension content, and external applications can have separate privacy behavior and policies.

## WebCom telemetry

WebCom v3 does **not** include a WebCom analytics or telemetry upload service and does not automatically upload crash reports, diagnostics, browsing history, cookies, passwords, form contents, or support bundles.

## Browsing and WebView2

Web pages are rendered by the installed Microsoft Edge WebView2 Runtime. Visiting a website sends the normal network requests required to load that site, and websites can use their own cookies, storage, fingerprinting, accounts, analytics, and tracking technologies. WebCom's bundled blocker reduces some advertising/tracking requests but cannot guarantee that a site cannot track a visitor.

Google is the default search provider in this release. Searches sent through the omnibox are therefore requests to Google unless the user enters a direct address.

## Local browser data

Normal profiles can store local browser state such as history, bookmarks, saved sessions, recently closed tabs, profile preferences, permissions, downloads history, extension metadata, and WebView2 profile data. Private tabs are designed not to be added to WebCom's normal session/history/recently-closed stores; WebView2 private-profile behavior remains part of the runtime boundary.

Credential values are controlled by WebView2. WebCom exposes save/autofill controls but does not enumerate, decrypt, export, or display stored password values.

## Runtime logs and support exports

WebCom keeps a small bounded local runtime log for reliability diagnostics. HTTP, HTTPS, and local-file URLs are redacted from log details. Logs rotate locally and are not uploaded automatically.

Support Diagnostics, recovery reports, memory reports, and problem-report templates are created locally only after an explicit user action. Review any generated file before sharing it.

## Updates

Update checks are opt-in. Selecting Stable or Beta alone does not contact the release service. Once checks are enabled, WebCom may contact the configured HTTPS release-manifest endpoint in the background on a persisted six-hour cadence with bounded retry behavior. The hosting service can receive ordinary network metadata such as the user's IP address and the `WebCom/3.0.0 updater` user-agent string; WebCom does not attach browsing history, page URLs, cookies, passwords, form data, profile names, or diagnostics to those requests.

Automatic **verified** installer downloads are a separate opt-in preference. When enabled, WebCom may fetch the package URL from an accepted signed manifest and stage the installer only after size, SHA-256, Authenticode, product/version, and pinned-publisher verification. Installer launch and rollback remain explicit user actions; WebCom does not silently install, restart, or roll back itself. See `docs/BACKGROUND-UPDATES.md`.

## External applications

`mailto:`, `tel:`, and `sms:` links require confirmation before WebCom asks Windows to open another application. Other custom URL schemes are blocked by WebCom's external-protocol policy.

## Clearing data

WebCom provides profile data-clearing and profile-deletion controls. Downloaded files stored outside the profile and bookmarks intentionally preserved by a selected clear operation are not silently deleted.
